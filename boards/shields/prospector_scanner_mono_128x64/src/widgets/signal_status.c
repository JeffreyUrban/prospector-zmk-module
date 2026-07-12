/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * 4-bar signal-strength meter driven by advertisement RSSI. Uses the same LVGL
 * canvas / mono-polarity convention as battery_gauge.c so it renders correctly
 * on the SH1106 (lv_color_black() lights the pixel; lv_color_white() is dark).
 */

#include <zephyr/kernel.h>

#include "signal_status.h"

#define BARS 4
#define BAR_W 2
#define BAR_GAP 1
#define SIG_W (BARS * BAR_W + (BARS - 1) * BAR_GAP) /* 11 */
#define SIG_H 9

#define BUFFER_SIZE                                                                                \
    LV_CANVAS_BUF_SIZE(SIG_W, SIG_H, LV_COLOR_FORMAT_GET_BPP(LV_COLOR_FORMAT_L8),                   \
                       LV_DRAW_BUF_STRIDE_ALIGN)

#define SIG_LIT lv_color_black()  /* pixel on  -> white on panel */
#define SIG_DARK lv_color_white() /* pixel off -> dark on panel  */

static lv_obj_t *canvas;
static lv_color_t canvas_buf[BUFFER_SIZE];

/* Map RSSI (dBm) to 1-4 bars. Receiving an ad at all means at least 1 bar. */
static int rssi_to_level(int8_t rssi) {
    if (rssi >= -55) {
        return 4;
    } else if (rssi >= -65) {
        return 3;
    } else if (rssi >= -75) {
        return 2;
    }
    return 1;
}

/* level 0 = nothing lit (blank). Unlit bars show a 1px baseline so the 4-bar
 * scale is visible; lit bars fill their full increasing height. */
static void draw_signal(int level) {
    lv_canvas_fill_bg(canvas, SIG_DARK, LV_OPA_COVER);
    for (int b = 0; b < BARS; b++) {
        int h = 3 + b * 2; /* 3, 5, 7, 9 */
        int x0 = b * (BAR_W + BAR_GAP);
        int top = (b < level) ? (SIG_H - h) : (SIG_H - 1);
        for (int y = top; y < SIG_H; y++) {
            for (int x = x0; x < x0 + BAR_W; x++) {
                lv_canvas_set_px(canvas, x, y, SIG_LIT, LV_OPA_COVER);
            }
        }
    }
}

void zmk_widget_signal_status_set(int8_t rssi, bool present) {
    if (canvas == NULL) {
        return;
    }
    draw_signal(present ? rssi_to_level(rssi) : 0);
}

int zmk_widget_signal_status_init(struct zmk_widget_signal_status *widget, lv_obj_t *parent) {
    widget->obj = lv_canvas_create(parent);
    lv_canvas_set_buffer(widget->obj, canvas_buf, SIG_W, SIG_H, LV_COLOR_FORMAT_L8);
    canvas = widget->obj;
    draw_signal(0);
    return 0;
}

lv_obj_t *zmk_widget_signal_status_obj(struct zmk_widget_signal_status *widget) {
    return widget->obj;
}
