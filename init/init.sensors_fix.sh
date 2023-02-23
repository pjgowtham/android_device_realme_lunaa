#!/vendor/bin/sh
#
# Copyright (C) 2022 Paranoid Android
#
# SPDX-License-Identifier: Apache-2.0
#

fix_applied=$(getprop persist.vendor.sensors.fix_applied)

if [ "$fix_applied" != "true" ]; then
    # Nuke rgb sensor if it exists
    sed -i '/rgb/d' /mnt/vendor/persist/sensors/sensors_list.txt
    setprop persist.vendor.sensors.fix_applied true
fi