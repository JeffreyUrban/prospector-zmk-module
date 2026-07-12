/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * Three vertical battery gauges (left half / dongle / right half) for the mono
 * OLED scanner. Each gauge fills bottom-to-top by percentage; gauges below a
 * configurable threshold blink. This is original Prospector-OLED code (it
 * replaces the dongle_display battery widget).
 */

#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

struct zmk_widget_battery_gauge {
    sys_snode_t node;
    lv_obj_t *obj;
};

int zmk_widget_battery_gauge_init(struct zmk_widget_battery_gauge *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_battery_gauge_obj(struct zmk_widget_battery_gauge *widget);

/*
 * Transport: set the three gauge levels (0-100), in physical left-to-right
 * order: left keyboard half, dongle (center), right keyboard half.
 * A level of 0 renders an empty gauge and does not blink.
 */
void zmk_widget_battery_gauge_set(uint8_t left, uint8_t center, uint8_t right);
