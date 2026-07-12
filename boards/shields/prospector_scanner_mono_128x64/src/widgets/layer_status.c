/*
 * Copyright (c) 2020 The ZMK Contributors
 * Copyright (c) 2024 Maximilian Engl
 *
 * SPDX-License-Identifier: MIT
 *
 * Derived from englmaxi/zmk-dongle-display (MIT). Rendering/layout kept as-is;
 * only the data transport changed to the Prospector BLE scanner.
 * See ATTRIBUTION.md.
 */

#include <string.h>
#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include "layer_status.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct layer_status_state {
    uint8_t index;
    const char *label;
};

static void set_layer_symbol(lv_obj_t *label, struct layer_status_state state) {
    char text[13] = {};
    if (state.label == NULL) {
        snprintf(text, sizeof(text), "%i", state.index);
    } else {
        snprintf(text, sizeof(text), "%s", state.label);
    }

    /*
     * The scanner transport calls this every ~300ms, but lv_label_set_text
     * restarts the circular scroll animation each time -> the name lurches back
     * to the start several times a second. englmaxi's original was event-driven
     * (only on layer change). Skip redundant sets so the scroll stays smooth.
     */
    const char *cur = lv_label_get_text(label);
    if (cur != NULL && strcmp(cur, text) == 0) {
        return;
    }
    lv_label_set_text(label, text);
}

static void layer_status_update_cb(struct layer_status_state state) {
    struct zmk_widget_layer_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_layer_symbol(widget->obj, state); }
}

/* Transport: fed by the scanner instead of ZMK layer events. */
void zmk_widget_layer_status_set(uint8_t index, const char *label) {
    layer_status_update_cb((struct layer_status_state){.index = index, .label = label});
}

int zmk_widget_layer_status_init(struct zmk_widget_layer_status *widget, lv_obj_t *parent) {
    widget->obj = lv_label_create(parent);
    lv_obj_set_width(widget->obj, CONFIG_ZMK_DONGLE_DISPLAY_LAYER_NAME_SCROLL_WIDTH);
    lv_label_set_long_mode(widget->obj, LV_LABEL_LONG_SCROLL_CIRCULAR);

    // Set text alignment based on config
    if (strcmp(CONFIG_ZMK_DONGLE_DISPLAY_LAYER_TEXT_ALIGN, "right") == 0) {
        lv_obj_set_style_text_align(widget->obj, LV_TEXT_ALIGN_RIGHT, 0);
    } else if (strcmp(CONFIG_ZMK_DONGLE_DISPLAY_LAYER_TEXT_ALIGN, "center") == 0) {
        lv_obj_set_style_text_align(widget->obj, LV_TEXT_ALIGN_CENTER, 0);
    } else {
        lv_obj_set_style_text_align(widget->obj, LV_TEXT_ALIGN_LEFT, 0);
    }

    sys_slist_append(&widgets, &widget->node);

    return 0;
}

lv_obj_t *zmk_widget_layer_status_obj(struct zmk_widget_layer_status *widget) {
    return widget->obj;
}
