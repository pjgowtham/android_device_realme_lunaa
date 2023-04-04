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

#include "AsyncScreenCaptureListener.h"

#include <android-base/properties.h>
#include <binder/ProcessState.h>
#include <gui/SurfaceComposerClient.h>
#include <gui/SyncScreenCaptureListener.h>
#include <ui/DisplayState.h>

#include <cstdio>
#include <signal.h>
#include <unistd.h>
#include <sysutils/FrameworkCommand.h>
#include <sysutils/FrameworkListener.h>
#include <utils/Timers.h>

using android::base::SetProperty;
using android::gui::ScreenCaptureResults;
using android::ui::DisplayState;
using android::ui::PixelFormat;
using android::AsyncScreenCaptureListener;
using android::DisplayCaptureArgs;
using android::GraphicBuffer;
using android::IBinder;
using android::Rect;
using android::ScreenshotClient;
using android::sp;
using android::SurfaceComposerClient;

constexpr int ALS_POS_X = 650;
constexpr int ALS_POS_Y = 40;
constexpr int ALS_RADIUS = 64;
constexpr int SCREENSHOT_INTERVAL = 1;

static Rect screenshot_rect(499, 110, 525, 136);

class TakeScreenshotCommand : public FrameworkCommand {
  public:
    TakeScreenshotCommand() : FrameworkCommand("take_screenshot") {}
    ~TakeScreenshotCommand() override = default;
    int runCommand(SocketClient* cli, int /*argc*/, char **/*argv*/) {
        auto screenshot = takeScreenshot();
        cli->sendData(&screenshot, sizeof(screenshot_t));
        return 0;
    }
  private:
    struct screenshot_t {
        uint32_t r, g, b;
        nsecs_t timestamp;
    };

    screenshot_t takeScreenshot() {
        static sp<GraphicBuffer> outBuffer = new GraphicBuffer(
                screenshot_rect.getWidth(), screenshot_rect.getHeight(),
                android::PIXEL_FORMAT_RGB_888,
                GraphicBuffer::USAGE_SW_READ_OFTEN | GraphicBuffer::USAGE_SW_WRITE_OFTEN);

        ScreenshotClient::capture(
                SurfaceComposerClient::getInternalDisplayToken(),
                android::ui::Dataspace::V0_SRGB, android::ui::PixelFormat::RGBA_8888,
                screenshot_rect, screenshot_rect.getWidth(), screenshot_rect.getHeight(),
                false, android::ui::ROTATION_0, &outBuffer);

        uint8_t *out;
        auto resultWidth = outBuffer->getWidth();
        auto resultHeight = outBuffer->getHeight();
        auto stride = outBuffer->getStride();

        outBuffer->lock(GraphicBuffer::USAGE_SW_READ_OFTEN, reinterpret_cast<void **>(&out));
        // we can sum this directly on linear light
        uint32_t rsum = 0, gsum = 0, bsum = 0;
        for (int y = 0; y < resultHeight; y++) {
            for (int x = 0; x < resultWidth; x++) {
                rsum += out[y * (stride * 4) + x * 4];
                gsum += out[y * (stride * 4) + x * 4 + 1];
                bsum += out[y * (stride * 4) + x * 4 + 2];
            }
        }
        uint32_t max = resultWidth * resultHeight;
        outBuffer->unlock();

        return { rsum / max, gsum / max, bsum / max, systemTime(SYSTEM_TIME_BOOTTIME) };
    }
};

class AlsCorrectionListener : public FrameworkListener {
  public:
    AlsCorrectionListener() : FrameworkListener("als_correction") {
        registerCmd(new TakeScreenshotCommand);
    }
};

int main() {
    auto listener = new AlsCorrectionListener();
    listener->startListener();

    while (true) {
        pause();
    }

    return 0;
}
