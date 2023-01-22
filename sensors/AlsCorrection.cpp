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

#include "AlsCorrection.h"

#include <cutils/properties.h>
#include <fstream>
#include <log/log.h>
#include <time.h>

namespace android {
namespace hardware {
namespace sensors {
namespace V2_1 {
namespace implementation {

static int red_max_lux, green_max_lux, blue_max_lux, white_max_lux, max_brightness;
static int als_bias;

template <typename T>
static T get(const std::string& path, const T& def) {
    std::ifstream file(path);
    T result;

    file >> result;
    return file.fail() ? def : result;
}

void AlsCorrection::init() {
    red_max_lux = get("/mnt/vendor/persist/engineermode/red_max_lux", 0);
    green_max_lux = get("/mnt/vendor/persist/engineermode/green_max_lux", 0);
    blue_max_lux = get("/mnt/vendor/persist/engineermode/blue_max_lux", 0);
    white_max_lux = get("/mnt/vendor/persist/engineermode/white_max_lux", 0);
    als_bias = get("/mnt/vendor/persist/engineermode/als_bias", 0);
    max_brightness = get("/sys/class/backlight/panel0-backlight/max_brightness", 255);
    ALOGV("max r = %d, max g = %d, max b = %d", red_max_lux, green_max_lux, blue_max_lux);
}

void AlsCorrection::correct(float& light) {
    pid_t pid = property_get_int32("vendor.sensors.als_correction.pid", 0);
    if (pid != 0) {
        kill(pid, SIGUSR1);
    }
    // TODO: HIDL service and pass float instead
    int r = property_get_int32("vendor.sensors.als_correction.r", 0);
    int g = property_get_int32("vendor.sensors.als_correction.g", 0);
    int b = property_get_int32("vendor.sensors.als_correction.b", 0);
    ALOGV("Screen Color Above Sensor: %d, %d, %d", r, g, b);
    ALOGV("Original reading: %f", light);
    int screen_brightness = get("/sys/class/backlight/panel0-backlight/brightness", 0);
    float correction = 0.0f, correction_scaled = 0.0f;
    if (red_max_lux > 0 && green_max_lux > 0 && blue_max_lux > 0 && white_max_lux > 0) {
        constexpr float rgb_scale = 0x7FFFFFFF;
        int rgb_min = std::min({r, g, b});
        r -= rgb_min;
        g -= rgb_min;
        b -= rgb_min;
        correction += ((float) rgb_min) / rgb_scale * ((float) white_max_lux);
        correction += ((float) r) / rgb_scale * ((float) red_max_lux);
        correction += ((float) g) / rgb_scale * ((float) green_max_lux);
        correction += ((float) b) / rgb_scale * ((float) blue_max_lux);
        correction = correction * (((float) screen_brightness) / ((float) max_brightness));
        correction += als_bias;
        correction_scaled = correction * (((float) white_max_lux) /
                                          (red_max_lux + green_max_lux + blue_max_lux));
    }
    if (light - correction >= 0) {
        // Apply correction if light - correction >= 0
        light -= correction;
    } else if (light - correction > -4) {
        // Return positive value if light - correction > -4
        light = correction - light;
    } else if (light - correction_scaled >= 0) {
        // Substract scaled correction if light - correction_scaled >= 0
        light -= correction_scaled;
    } else {
        // In low light conditions, sensor is just reporting bad values, using
        // computed correction instead allows to fix the issue
        light = correction;
    }
    ALOGV("Corrected reading: %f", light);
}

}  // namespace implementation
}  // namespace V2_1
}  // namespace sensors
}  // namespace hardware
}  // namespace android
