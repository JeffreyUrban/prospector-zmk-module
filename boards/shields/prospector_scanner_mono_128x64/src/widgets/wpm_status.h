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

struct zmk_widget_wpm_status
{
    sys_snode_t node;
    lv_obj_t *obj;
    lv_obj_t *wpm_label;
};

int zmk_widget_wpm_status_init(struct zmk_widget_wpm_status *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_wpm_status_obj(struct zmk_widget_wpm_status *widget);

/* Transport: set WPM value + current layer name from scanner data. */
void zmk_widget_wpm_status_set(int wpm, const char *layer);
