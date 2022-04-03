#
# Copyright (C) 2021 The LineageOS Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# Inherit from oplus sm8350-common
-include device/oplus/sm8350-common/BoardConfigCommon.mk

DEVICE_PATH := device/oplus/RMX3360

# Bluetooth
BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR := $(DEVICE_PATH)/bluetooth/include

# Display
TARGET_SCREEN_DENSITY := 450

# Kernel
TARGET_FORCE_PREBUILT_KERNEL := true
TARGET_PREBUILT_KERNEL := $(DEVICE_PATH)/prebuilt/RMX3360/Image.gz
BOARD_PREBUILT_DTBOIMAGE := $(DEVICE_PATH)/prebuilt/RMX3360/dtbo.img
TARGET_PREBUILT_DTB := $(DEVICE_PATH)/prebuilt/RMX3360/dtb.img
BOARD_MKBOOTIMG_ARGS += --dtb $(TARGET_PREBUILT_DTB)

# Copy kernel modules to vendor_boot and vendor_dlkm
PRODUCT_COPY_FILES += \
    $(call find-copy-subdir-files,*,vendor/oplus/sm8350-common/RMX3360_vendor_dlkm,$(TARGET_COPY_OUT_VENDOR_DLKM)/lib/modules) \
    $(call find-copy-subdir-files,*,vendor/oplus/sm8350-common/RMX3360_vendor_ramdisk,$(TARGET_COPY_OUT_VENDOR_RAMDISK)/lib/modules)
    
# Properties
TARGET_VENDOR_PROP += $(DEVICE_PATH)/vendor.prop

# Recovery
TARGET_OTA_ASSERT_DEVICE := RMX3360,RMX3363,RMX3360L1,RE54ABL1

# inherit from the proprietary version
-include vendor/oplus/RMX3360/BoardConfigVendor.mk
