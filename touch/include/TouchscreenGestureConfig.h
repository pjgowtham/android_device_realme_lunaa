/*
 * SPDX-FileCopyrightText: 2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "TouchscreenGesture.h"

namespace aidl {
namespace vendor {
namespace lineage {
namespace touch {

const int TouchscreenGesture::kSupportedGestures = makeBitField(
        kGestureUpVee, kGestureDownVee, kGestureLeftVee, kGestureRightVee, kGestureCircle,
        kGestureDoubleSwipe, kGestureLeftToRight, kGestureRightToLeft, kGestureUpToDown,
        kGestureDownToUp, kGestureM, kGestureW, kGestureSingleTap);

}  // namespace touch
}  // namespace lineage
}  // namespace vendor
}  // namespace aidl
