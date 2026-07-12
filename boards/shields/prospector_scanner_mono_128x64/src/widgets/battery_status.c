/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * Adapted from englmaxi/zmk-dongle-display (MIT). The rendering (draw_battery,
 * set_battery_symbol) and LVGL construction are kept verbatim. Only the
 * transport changed: the ZMK battery-event listener is replaced by
 * zmk_widget_dongle_battery_status_set(), fed by the BLE scanner.
 */

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include "battery_status.h"

/* Scanner sources: 0 = central battery, 1.. = peripheral batteries. */
#define BATTERY_SOURCE_COUNT 4

#define BUFFER_SIZE LV_CANVAS_BUF_SIZE(5, 8, LV_COLOR_FORMAT_GET_BPP(LV_COLOR_FORMAT_L8), LV_DRAW_BUF_STRIDE_ALIGN)

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct battery_state {
    uint8_t source;
    uint8_t level;
    bool usb_present;
};

struct battery_object {
    lv_obj_t *symbol;
    lv_obj_t *label;
} battery_objects[BATTERY_SOURCE_COUNT];

static lv_color_t battery_image_buffer[BATTERY_SOURCE_COUNT][BUFFER_SIZE];

static void draw_battery(lv_obj_t *canvas, uint8_t level, bool usb_present) {
    lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_COVER);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    lv_draw_rect_dsc_t rect_fill_dsc;
    lv_draw_rect_dsc_init(&rect_fill_dsc);
    rect_fill_dsc.bg_color = lv_color_white();

    if (usb_present) {
        rect_fill_dsc.bg_opa = LV_OPA_TRANSP;
        rect_fill_dsc.border_color = lv_color_white();
        rect_fill_dsc.border_width = 1;
    }

    lv_canvas_set_px(canvas, 0, 0, lv_color_white(), LV_OPA_COVER);
    lv_canvas_set_px(canvas, 4, 0, lv_color_white(), LV_OPA_COVER);

    lv_area_t rect_coords;
    bool rect_draw = true;

    if (level <= 10 || usb_present) {
        rect_coords = (lv_area_t){1, 2, 3, 6};
    } else if (level <= 30) {
        rect_coords = (lv_area_t){1, 2, 3, 5};
    } else if (level <= 50) {
        rect_coords = (lv_area_t){1, 2, 3, 4};
    } else if (level <= 70) {
        rect_coords = (lv_area_t){1, 2, 3, 3};
    } else if (level <= 90) {
        rect_coords = (lv_area_t){1, 2, 3, 2};
    } else {
        rect_draw = false;
    }

    if (rect_draw) {
        lv_draw_rect(&layer, &rect_fill_dsc, &rect_coords);
    }

    lv_canvas_finish_layer(canvas, &layer);
}

static void set_battery_symbol(lv_obj_t *widget, struct battery_state state) {
    if (state.source >= BATTERY_SOURCE_COUNT) {
        return;
    }
    LOG_DBG("source: %d, level: %d, usb: %d", state.source, state.level, state.usb_present);
    lv_obj_t *symbol = battery_objects[state.source].symbol;
    lv_obj_t *label = battery_objects[state.source].label;

    draw_battery(symbol, state.level, state.usb_present);
    lv_label_set_text_fmt(label, "%4u%% ", state.level);

    if (state.level > 0 || state.usb_present) {
        lv_obj_clear_flag(symbol, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(symbol);
        lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(label);
    } else {
        lv_obj_add_flag(symbol, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
    }
}

void battery_status_update_cb(struct battery_state state) {
    struct zmk_widget_dongle_battery_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_battery_symbol(widget->obj, state); }
}

/* Transport: fed by the scanner instead of ZMK battery events. */
void zmk_widget_dongle_battery_status_set(uint8_t source, uint8_t level, bool usb_present) {
    battery_status_update_cb((struct battery_state){
        .source = source,
        .level = level,
        .usb_present = usb_present,
    });
}

int zmk_widget_dongle_battery_status_init(struct zmk_widget_dongle_battery_status *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);

    lv_obj_set_size(widget->obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

    for (int i = 0; i < BATTERY_SOURCE_COUNT; i++) {
        lv_obj_t *image_canvas = lv_canvas_create(widget->obj);
        lv_obj_t *battery_label = lv_label_create(widget->obj);

        lv_canvas_set_buffer(image_canvas, battery_image_buffer[i], 5, 8, LV_COLOR_FORMAT_L8);

        lv_obj_align(image_canvas, LV_ALIGN_TOP_RIGHT, 0, i * 10);
        lv_obj_align_to(battery_label, image_canvas, LV_ALIGN_OUT_LEFT_MID, 0, 0);

        lv_obj_add_flag(image_canvas, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(battery_label, LV_OBJ_FLAG_HIDDEN);

        battery_objects[i] = (struct battery_object){
            .symbol = image_canvas,
            .label = battery_label,
        };
    }

    sys_slist_append(&widgets, &widget->node);

    return 0;
}

lv_obj_t *zmk_widget_dongle_battery_status_obj(struct zmk_widget_dongle_battery_status *widget) {
    return widget->obj;
}
