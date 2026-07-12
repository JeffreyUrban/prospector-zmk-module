/*
 * Copyright (c) 2024 The ZMK Contributors
 * Copyright (c) 2024 Maximilian Engl
 *
 * SPDX-License-Identifier: MIT
 *
 * Derived from englmaxi/zmk-dongle-display (MIT). See ATTRIBUTION.md.
 */

#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

#define SIZE_SYMBOLS 14 // 14 x 14 pixel

struct zmk_widget_modifiers {
    sys_snode_t node;
    lv_obj_t *obj;
};

int zmk_widget_modifiers_init(struct zmk_widget_modifiers *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_modifiers_obj(struct zmk_widget_modifiers *widget);

/* Transport: set active modifiers (HID MOD_* bit layout) from scanner data. */
void zmk_widget_modifiers_set(uint8_t modifiers);
