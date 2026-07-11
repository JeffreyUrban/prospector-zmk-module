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
#include <zmk/endpoints.h>

struct zmk_widget_output_status {
    sys_snode_t node;
    lv_obj_t *obj;
};

int zmk_widget_output_status_init(struct zmk_widget_output_status *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_output_status_obj(struct zmk_widget_output_status *widget);

/* Transport: set output/endpoint status from scanner data. */
void zmk_widget_output_status_set(enum zmk_transport transport, bool connected,
                                  int profile_index, bool profile_connected,
                                  bool profile_bonded, bool usb_hid_ready);
