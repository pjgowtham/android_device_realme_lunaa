#
# Copyright (C) 2021-2022 The LineageOS Project
#
# SPDX-License-Identifier: Apache-2.0
#

# Inherit from those products. Most specific first.
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base_telephony.mk)

# Inherit from RMX3360 device
$(call inherit-product, device/oplus/RMX3360/device.mk)

# Inherit some common Lineage stuff.
$(call inherit-product, vendor/aosp/config/common_full_phone.mk)

PRODUCT_NAME := aosp_RMX3360
PRODUCT_DEVICE := RMX3360
PRODUCT_MANUFACTURER := realme
PRODUCT_BRAND := realme
PRODUCT_MODEL := RMX3360

PRODUCT_SYSTEM_NAME := RMX3360
PRODUCT_SYSTEM_DEVICE := RE54ABL1

PRODUCT_GMS_CLIENTID_BASE := android-oppo

BUILD_FINGERPRINT := realme/RMX3360/RE54ABL1:12/RKQ1.210503.001/R.202202112205:user/release-keys


# PixelExperience Props
TARGET_BOOT_ANIMATION_RES = 1080
#TARGET_INCLUDE_STOCK_ARCORE := false
#TARGET_INCLUDE_LIVE_WALLPAPERS := false
#TARGET_SUPPORTS_GOOGLE_RECORDER := false
TARGET_GAPPS_ARCH := arm64
