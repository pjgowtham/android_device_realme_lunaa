/*
 * Copyright (C) 2022 Project Kaleidoscope
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

#pragma once

#include <android/gui/BnScreenCaptureListener.h>
#include <gui/SurfaceComposerClient.h>

namespace android {

using gui::ScreenCaptureResults;

typedef std::function<void(const ScreenCaptureResults& captureResults)> CaptureCallback;

struct AsyncScreenCaptureListener : gui::BnScreenCaptureListener {
public:
    AsyncScreenCaptureListener(CaptureCallback callback, int timeout)
            : callback(callback), timeout(timeout) {}

    binder::Status onScreenCaptureCompleted(const ScreenCaptureResults& captureResults) override {
        if (captureResults.fence->wait(timeout) == NO_ERROR)
            callback(captureResults);
        return binder::Status::ok();
    }

private:
    CaptureCallback callback;
    int timeout;
};

}  // namespace android
