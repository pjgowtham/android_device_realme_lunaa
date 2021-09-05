/*
 * Copyright (C) 2021 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_NDEBUG 0
#define LOG_TAG "AlsCorrection"

#include "AlsCorrection.h"

#include <android-base/file.h>

#include <algorithm>
#include <cmath>
#include <cutils/properties.h>
#include <fstream>
#include <log/log.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <utils/Timers.h>

#define PERSIST_ENG "/mnt/vendor/persist/engineermode/"
#define SYSFS_BACKLIGHT "/sys/class/backlight/panel0-backlight/"

namespace android {
namespace hardware {
namespace sensors {
namespace V2_1 {
namespace implementation {

static const std::string rgbw_max_lux_paths[4] = {
    PERSIST_ENG "red_max_lux",
    PERSIST_ENG "green_max_lux",
    PERSIST_ENG "blue_max_lux",
    PERSIST_ENG "white_max_lux",
};

struct als_config {
    bool hbr;
    float rgbw_max_lux[4];
    float rgbw_max_lux_div[4];
    float rgbw_lux_postmul[4];
    float rgbw_poly[4][4];
    float grayscale_weights[3];
    float sensor_inverse_gain[4];
    float agc_threshold;
    float calib_gain;
    float bias;
    float max_brightness;
};

#if defined(DEVICE_guacamole) || defined(DEVICE_guacamoleg)
static als_config conf = {
    .hbr = false,
    .rgbw_max_lux = { 199.0, 216.0, 79.0, 428.0 },
    .rgbw_max_lux_div = { 198.0, 216.0, 78.0, 409.0 },
    .rgbw_poly = {
        { 1.7404983E-6, 0.0018088078, 0.003599656, -0.5450117 },
        { 4.12301E-6, 0.0017906721, -0.034968063, -0.08428217 },
        { 1.361745E-6, 8.127534E-4, -0.046870504, 0.52842677 },
        { 2.7275946E-6, 0.0016300974, -0.021769103, -0.16610238 },
    },
    .grayscale_weights = { 0.4, 0.43, 0.17 },
    .sensor_inverse_gain = { 1.003141, 0.497163, 0.28517, 0.178429 },
};
#elif defined(DEVICE_guacamoleb)
static als_config conf = {
    .hbr = false,
    .rgbw_max_lux = { 670.0, 680.0, 347.0, 1294.0 },
    .rgbw_max_lux_div = { 485.0, 647.0, 275.0, 1225.0 },
    .rgbw_poly = {
        { 4.6876557E-6, 0.0039652763, 0.58725166, 0.0 },
        { 3.999002E-6, 0.0076181223, 0.35004568, 0.0 },
        { 1.8699997E-6, 0.0030195534, 0.19580707, 0.0 },
        { 3.83552E-6, 0.0054658977, 0.40376243, 0.0 },
    },
    .grayscale_weights = { 0.35, 0.46, 0.19 },
    .sensor_inverse_gain = { 0.472747, 0.206944, 0.117592, 0.067704 },
};
#elif defined(DEVICE_hotdog) || defined(DEVICE_hotdogg)
static als_config conf = {
    .hbr = true,
    .rgbw_max_lux = { 419.0, 822.0, 251.0, 1255.0 },
    .rgbw_max_lux_div = { 414.0, 817.0, 251.0, 1245.0 },
    .rgbw_poly = {
        { 1.1255767E-5, 0.0023767108, 0.2771623, 0.0 },
        { 2.8453156E-5, 0.004190259, 0.25827286, 0.0 },
        { 6.5442537E-6, 0.0020840308, 0.020182181, 0.0 },
        { 1.9053505E-5, 0.003323373, 0.22403096, 0.0 },
    },
    .grayscale_weights = { 0.33, 0.5, 0.17 },
    .sensor_inverse_gain = { 0.232, 0.175, 0.133, 0.097 },
};
#elif defined(DEVICE_hotdogb)
static als_config conf = {
    .hbr = true,
    .rgbw_max_lux = { 1500.0, 2600.0, 1400.0, 4600.0 },
    .rgbw_max_lux_div = { 1437.0, 2427.0, 1369.0, 4606.0 },
    .rgbw_poly = {
        { 4.787111E-5, 0.0073087, 0.6651031, 0.0 },
        { 1.1037093E-4, 0.0059161806, 0.82983816, 0.0 },
        { 4.6553232E-5, 0.008220689, 0.24763061, 0.0 },
        { 7.379156E-5, 0.006951839, 0.6299237, 0.0 },
    },
    .grayscale_weights = { 0.33, 0.42, 0.25 },
    .sensor_inverse_gain = { 0.0615, 0.0466, 0.0361, 0.0288 },
};
#else
#error No ALS configuration for this device
#endif

static struct {
    float middle;
    float min, max;
} hysteresis_ranges[] = {
    { 0, 0, 4 },
    { 7, 1, 12 },
    { 15, 5, 30 },
    { 30, 10, 50 },
    { 360, 25, 700 },
    { 1200, 300, 1600 },
    { 2250, 1000, 2940 },
    { 4600, 2000, 5900 },
    { 10000, 4000, 80000 },
    { HUGE_VALF, 8000, HUGE_VALF },
};

static struct {
    nsecs_t last_update, last_forced_update;
    bool force_update;
    float hyst_min, hyst_max;
    float last_corrected_value;
    float last_agc_gain;
} state = {
    .last_update = 0,
    .force_update = true,
    .hyst_min = -1.0, .hyst_max = -1.0,
    .last_agc_gain = 0.0,
};

template <typename T>
static T get(const std::string& path, const T& def) {
    std::ifstream file(path);
    T result;

    file >> result;
    return file.fail() ? def : result;
}

void AlsCorrection::init() {
    static bool initialized = false;
    if (initialized) {
        return;
    }
    initialized = true;

    // TODO: Constantly update and persist this
    float screen_on_time = get(PERSIST_ENG "screenontimebyhours", 0.0);
    float screen_aging_factor = 1.0 - screen_on_time / 87600.0;
    ALOGI("Screen on time: %.2fh (aging factor: %.2f%%)",
        screen_on_time, screen_aging_factor * 100.0);

    float rgbw_acc = 0.0;
    for (int i = 0; i < 4; i++) {
        float max_lux = get(rgbw_max_lux_paths[i], 0.0);
        if (max_lux != 0.0) {
            conf.rgbw_max_lux[i] = max_lux;
        }
        conf.rgbw_max_lux[i] *= screen_aging_factor;
        if (i < 3) {
            rgbw_acc += conf.rgbw_max_lux[i];
            conf.rgbw_lux_postmul[i] = conf.rgbw_max_lux[i] / conf.rgbw_max_lux_div[i];
        } else {
            rgbw_acc -= conf.rgbw_max_lux[i];
            conf.rgbw_lux_postmul[i] = rgbw_acc / conf.rgbw_max_lux_div[i];
        }
    }
    ALOGI("Display maximums: R=%.0f G=%.0f B=%.0f W=%.0f",
        conf.rgbw_max_lux[0], conf.rgbw_max_lux[1],
        conf.rgbw_max_lux[2], conf.rgbw_max_lux[3]);

    float row_coe = get(PERSIST_ENG "row_coe", 0.0);
    if (row_coe != 0.0) {
        conf.sensor_inverse_gain[0] = row_coe / 1000.0;
    }
    conf.agc_threshold = 800.0 / conf.sensor_inverse_gain[0];

    float cali_coe = get(PERSIST_ENG "cali_coe", 0.0);
    conf.calib_gain = cali_coe > 0.0 ? cali_coe / 1000.0 : 1.0;
    ALOGI("Calibrated sensor gain: %.2fx", 1.0 / (conf.calib_gain * conf.sensor_inverse_gain[0]));

    float als_bias = get(PERSIST_ENG "als_bias", 0.0);
    conf.bias = als_bias <= 4.0 ? als_bias : 0.0;
    ALOGI("Sensor bias: %.2f", conf.bias);

    float max_brightness = get(SYSFS_BACKLIGHT "max_brightness", 0.0);
    conf.max_brightness = max_brightness > 0.0 ? max_brightness : 1023.0;

    for (auto& range : hysteresis_ranges) {
        range.min /= conf.calib_gain * conf.sensor_inverse_gain[0];
        range.max /= conf.calib_gain * conf.sensor_inverse_gain[0];
    }
    hysteresis_ranges[0].min = -1.0;
}

void AlsCorrection::process(Event& event) {
    /*
    ALOGV("%f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f",
            event.u.data[0],
            event.u.data[1],
            event.u.data[2],
            event.u.data[3],
            event.u.data[4],
            event.u.data[5],
            event.u.data[6],
            event.u.data[7],
            event.u.data[8],
            event.u.data[9],
            event.u.data[10],
            event.u.data[11],
            event.u.data[12],
            event.u.data[13],
            event.u.data[14],
            event.u.data[15]
         );
    */
    ALOGV("Raw sensor reading: %.0f", event.u.scalar);

    if (event.u.scalar > conf.bias) {
        event.u.scalar -= conf.bias;
    }

    if (conf.hbr && event.u.data[8] != 2.0) {
        state.force_update = true;
        event.u.scalar *= conf.calib_gain * conf.sensor_inverse_gain[0];
        ALOGV("Skipping correction, calibrated ambient light: %.0f lux", event.u.scalar);
        return;
    }

    nsecs_t now = systemTime(SYSTEM_TIME_BOOTTIME);
    float brightness = get(SYSFS_BACKLIGHT "brightness", 0.0);

    if (state.last_update == 0) {
        state.last_update = now;
        state.last_forced_update = now;
    } else {
        if (brightness > 0.0 && (now - state.last_forced_update) > s2ns(3)) {
            ALOGV("Forcing screenshot");
            state.last_forced_update = now;
            state.force_update = true;
        }
        if ((now - state.last_update) < ms2ns(100)) {
            ALOGV("Events coming too fast, dropping");
            // TODO figure out a better way to drop events
            event.sensorHandle = 0;
            return;
        }
        state.last_update = now;
    }

    float sensor_raw_calibrated = event.u.scalar * conf.calib_gain * state.last_agc_gain;
    if (state.force_update
            || ((event.u.scalar < state.hyst_min || event.u.scalar > state.hyst_max)
                && (event.u.data[6] > 2.0
                    || sensor_raw_calibrated < 10.0 || sensor_raw_calibrated > (5.0 / .07)))) {
        android::base::unique_fd fd(socket(PF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0));
        if (fd.get() < 0) {
            ALOGV("Failed to open als correction socket: %s", strerror(errno));
            // TODO figure out a better way to drop events
            event.sensorHandle = 0;
            return;
        }

        sockaddr_un addr{ AF_UNIX, "/dev/socket/als_correction" };
        if (connect(fd.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
            ALOGV("Failed to connect to als correction socket: %s", strerror(errno));
            // TODO figure out a better way to drop events
            event.sensorHandle = 0;
            return;
        }

        const char *cmd = "take_screenshot";
        if (send(fd.get(), cmd, strlen(cmd) + 1, 0) == -1) {
            ALOGV("Failed to send command to als correction socket: %s", strerror(errno));
            // TODO figure out a better way to drop events
            event.sensorHandle = 0;
            return;
        }

        pollfd fds{ fd.get(), POLLIN, 0 };
        if (poll(&fds, 1, -1) != 1) {
            ALOGV("Invalid poll als correction socket fd");
            // TODO figure out a better way to drop events
            event.sensorHandle = 0;
            return;
        }

        struct screenshot_t {
            uint32_t r, g, b;
            nsecs_t timestamp;
        } screenshot;

        if (read(fd.get(), &screenshot, sizeof(screenshot_t)) != sizeof(screenshot_t)) {
            ALOGV("Invalid reply from als correction socket");
            // TODO figure out a better way to drop events
            event.sensorHandle = 0;
            return;
        }

        if ((now - screenshot.timestamp) > ms2ns(1000)) {
            ALOGV("Screenshot too old, dropping event");
            // TODO figure out a better way to drop events
            event.sensorHandle = 0;
            return;
        }
        ALOGV("Screen color above sensor: %d %d %d", screenshot.r, screenshot.g, screenshot.b);

        float rgbw[4] = {
            static_cast<float>(screenshot.r), static_cast<float>(screenshot.g), static_cast<float>(screenshot.b),
            screenshot.r * conf.grayscale_weights[0]
                + screenshot.g * conf.grayscale_weights[1]
                + screenshot.b * conf.grayscale_weights[2]
        };
        float cumulative_correction = 0.0;
        for (int i = 0; i < 4; i++) {
            float corr = 0.0;
            for (float coef : conf.rgbw_poly[i]) {
                corr *= rgbw[i];
                corr += coef;
            }
            corr *= conf.rgbw_lux_postmul[i];
            if (i < 3) {
                cumulative_correction += std::max(corr, 0.0f);
            } else {
                cumulative_correction -= corr;
            }
        }
        cumulative_correction *= brightness / conf.max_brightness;
        float brightness_fullwhite = conf.rgbw_max_lux[3] * brightness / conf.max_brightness;
        float brightness_grayscale_gamma = std::pow(rgbw[3] / 255.0, 2.2) * brightness_fullwhite;
        cumulative_correction = std::min(cumulative_correction, brightness_fullwhite);
        cumulative_correction = std::max(cumulative_correction, brightness_grayscale_gamma);
        ALOGV("Estimated screen brightness: %.0f", cumulative_correction);

        float sensor_raw_corrected = std::max(event.u.scalar - cumulative_correction, 0.0f);

        float agc_gain = conf.sensor_inverse_gain[0];
        if (sensor_raw_corrected > conf.agc_threshold) {
            if (!conf.hbr) {
                float gain_estimate = sensor_raw_corrected / event.u.data[8];
                if (gain_estimate > 85.0) {
                    agc_gain = conf.sensor_inverse_gain[0];
                } else if (gain_estimate >= 39.0) {
                    agc_gain = conf.sensor_inverse_gain[1];
                } else if (gain_estimate >= 29.0) {
                    agc_gain = conf.sensor_inverse_gain[2];
                } else {
                    agc_gain = conf.sensor_inverse_gain[3];
                }
            } else {
                float gain_estimate = event.u.data[8] * 1000.0 / event.u.scalar;
                if (gain_estimate > 1050.0) {
                    agc_gain = conf.sensor_inverse_gain[3];
                } else if (gain_estimate > 800.0) {
                    agc_gain = conf.sensor_inverse_gain[2];
                } else if (gain_estimate > 450.0) {
                    agc_gain = conf.sensor_inverse_gain[1];
                } else {
                    agc_gain = conf.sensor_inverse_gain[0];
                }
            }
        }
        ALOGV("AGC gain: %f", agc_gain);

        if (cumulative_correction <= event.u.scalar * 1.35
                || event.u.scalar * conf.calib_gain * agc_gain < 10000.0
                || state.force_update) {
            float sensor_corrected = sensor_raw_corrected * conf.calib_gain * agc_gain;
            state.last_agc_gain = agc_gain;
            for (auto& range : hysteresis_ranges) {
                if (sensor_corrected <= range.middle) {
                    state.hyst_min = range.min;
                    state.hyst_max = range.max + brightness_fullwhite;
                    break;
                }
            }
            sensor_corrected = std::max(sensor_corrected - 14.0, 0.0);
            event.u.scalar = sensor_corrected;
            state.last_corrected_value = sensor_corrected;
            ALOGV("Fully corrected sensor value: %.0f lux", sensor_corrected);
        } else {
            event.u.scalar = state.last_corrected_value;
            ALOGV("Reusing cached value: %.0f lux", event.u.scalar);
        }

        state.force_update = false;
    } else {
        event.u.scalar = state.last_corrected_value;
        ALOGV("Reusing cached value: %.0f lux", event.u.scalar);
    }
}

}  // namespace implementation
}  // namespace V2_1
}  // namespace sensors
}  // namespace hardware
}  // namespace android
