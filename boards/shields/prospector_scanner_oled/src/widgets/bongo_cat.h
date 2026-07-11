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

struct zmk_widget_bongo_cat {
    sys_snode_t node;
    lv_obj_t *obj;
};

int zmk_widget_bongo_cat_init(struct zmk_widget_bongo_cat *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_bongo_cat_obj(struct zmk_widget_bongo_cat *widget);

/* Transport: drive the animation from scanner WPM. */
void zmk_widget_bongo_cat_set(uint8_t wpm);
