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

# Inherit from oneplus sm8350-common
-include device/oneplus/sm8350-common/BoardConfigCommon.mk

DEVICE_PATH := device/oneplus/lemonade

# Bluetooth
BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR := $(DEVICE_PATH)/bluetooth/include

# Display
TARGET_SCREEN_DENSITY := 450

# Fingerprint
SOONG_CONFIG_ONEPLUS_LAHAINA_FOD_POS_X = 453
SOONG_CONFIG_ONEPLUS_LAHAINA_FOD_POS_Y = 1823
SOONG_CONFIG_ONEPLUS_LAHAINA_FOD_SIZE = 174

# Properties
TARGET_VENDOR_PROP += $(DEVICE_PATH)/vendor.prop

# Partitions
ifneq ($(WITH_GMS),true)
BOARD_PRODUCTIMAGE_PARTITION_RESERVED_SIZE := 467826463
BOARD_SYSTEMIMAGE_PARTITION_RESERVED_SIZE := 603881315
BOARD_SYSTEM_EXTIMAGE_PARTITION_RESERVED_SIZE := 46080000
endif
BOARD_ONEPLUS_DYNAMIC_PARTITIONS_SIZE := 3753902080
BOARD_SUPER_PARTITION_SIZE := 7516192768

# Recovery
TARGET_RECOVERY_FSTAB := $(DEVICE_PATH)/rootdir/etc/fstab.qcom

# inherit from the proprietary version
-include vendor/oneplus/lemonade/BoardConfigVendor.mk
