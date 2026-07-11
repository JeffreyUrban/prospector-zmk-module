/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * Adapted from englmaxi/zmk-dongle-display (MIT). Presentation code is kept
 * verbatim; only the transport (data source) is changed: instead of ZMK
 * battery events, the scanner feeds levels via the setter below.
 */

#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

struct zmk_widget_dongle_battery_status {
    sys_snode_t node;
    lv_obj_t *obj;
};

int zmk_widget_dongle_battery_status_init(struct zmk_widget_dongle_battery_status *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_dongle_battery_status_obj(struct zmk_widget_dongle_battery_status *widget);

/* Transport: set the level for a battery source (0 = central, 1.. = peripherals). */
void zmk_widget_dongle_battery_status_set(uint8_t source, uint8_t level, bool usb_present);
