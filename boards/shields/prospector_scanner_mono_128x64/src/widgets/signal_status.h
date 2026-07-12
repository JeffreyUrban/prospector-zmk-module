/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * A small 4-bar signal-strength meter for the mono OLED scanner, driven by the
 * RSSI of the received keyboard advertisements. Original Prospector-OLED code.
 */

#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

struct zmk_widget_signal_status {
    sys_snode_t node;
    lv_obj_t *obj;
};

int zmk_widget_signal_status_init(struct zmk_widget_signal_status *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_signal_status_obj(struct zmk_widget_signal_status *widget);

/*
 * Transport: set the link signal. rssi is in dBm; present=false (no keyboard)
 * blanks the meter.
 */
void zmk_widget_signal_status_set(int8_t rssi, bool present);
