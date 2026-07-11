/*
 * Copyright (c) 2024 The ZMK Contributors
 * Copyright (c) 2024 Maximilian Engl
 *
 * SPDX-License-Identifier: MIT
 *
 * Derived from englmaxi/zmk-dongle-display (MIT). Rendering kept as-is; only
 * the data transport changed to the Prospector BLE scanner. See ATTRIBUTION.md.
 */

#include <zephyr/kernel.h>
#include <string.h>
#include <stdio.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include "wpm_status.h"

LV_IMG_DECLARE(sym_speedometer);

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
static int last_wpm = -1;
static int64_t last_wpm_update_time = 0;
#define WPM_UPDATE_INTERVAL_MS 250  // Throttle: max 4 updates per second

struct wpm_status_state
{
    int wpm;
    const char *layer;
};

static void set_wpm(struct zmk_widget_wpm_status *widget, struct wpm_status_state state)
{
    // Early exit if WPM unchanged
    if (state.wpm == last_wpm) {
        return;
    }

    // Throttle updates to prevent display thread flooding
    int64_t now = k_uptime_get();
    if ((now - last_wpm_update_time) < WPM_UPDATE_INTERVAL_MS) {
        return;
    }
    last_wpm_update_time = now;
    last_wpm = state.wpm;

    // NULL check for layer name before strstr to prevent crash
    if (state.layer != NULL &&
        strstr(CONFIG_ZMK_DONGLE_DISPLAY_WPM_DISABLED_LAYERS, state.layer) != NULL) {
        lv_label_set_text(widget->wpm_label, "-");
        return;
    }

    char wpm_text[12];
    snprintf(wpm_text, sizeof(wpm_text), "%i", state.wpm);
    lv_label_set_text(widget->wpm_label, wpm_text);
}

static void wpm_status_update_cb(struct wpm_status_state state)
{
    struct zmk_widget_wpm_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node)
    {
        set_wpm(widget, state);
    }
}

/* Transport: fed by the scanner instead of ZMK WPM events. */
void zmk_widget_wpm_status_set(int wpm, const char *layer)
{
    wpm_status_update_cb((struct wpm_status_state){.wpm = wpm, .layer = layer});
}

int zmk_widget_wpm_status_init(struct zmk_widget_wpm_status *widget, lv_obj_t *parent)
{
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

    lv_obj_t *speedometer = lv_img_create(widget->obj);
    lv_obj_align(speedometer, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_img_set_src(speedometer, &sym_speedometer);

    widget->wpm_label = lv_label_create(widget->obj);
    lv_obj_align_to(widget->wpm_label, speedometer, LV_ALIGN_OUT_RIGHT_MID, 2, 1);

    sys_slist_append(&widgets, &widget->node);

    return 0;
}

lv_obj_t *zmk_widget_wpm_status_obj(struct zmk_widget_wpm_status *widget)
{
    return widget->obj;
}
