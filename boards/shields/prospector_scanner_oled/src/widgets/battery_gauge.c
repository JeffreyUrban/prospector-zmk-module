/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * Three vertical battery gauges for the mono OLED scanner. Original code (not
 * from dongle_display). Uses the same LVGL canvas / color convention as the
 * dongle_display battery widget so it renders correctly on the SH1106.
 */

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include "battery_gauge.h"

#define NUM_GAUGES 3

/*
 * Gauge geometry (pixels). Each battery is narrow: 1px walls + a 2px interior
 * (4px total). The fill spans the full interior width (no inset). A wide gap
 * spreads the three across roughly the original span with black space between.
 * Height aligns the bottom with the top-left output icons.
 */
#define GAUGE_W 4
#define GAUGE_H 18
#define GAUGE_GAP 4

#define BUFFER_SIZE                                                                                \
    LV_CANVAS_BUF_SIZE(GAUGE_W, GAUGE_H, LV_COLOR_FORMAT_GET_BPP(LV_COLOR_FORMAT_L8),              \
                       LV_DRAW_BUF_STRIDE_ALIGN)

/* Single widget instance; canvases + state are file scoped. */
static lv_obj_t *gauge_canvas[NUM_GAUGES];
static lv_color_t gauge_buffer[NUM_GAUGES][BUFFER_SIZE];
static uint8_t gauge_level[NUM_GAUGES];
static bool blink_on = true;

static bool is_low(uint8_t level) {
    return level > 0 && level < CONFIG_PROSPECTOR_OLED_BATTERY_LOW_THRESHOLD;
}

/*
 * Monochrome polarity on this LVGL 1bpp -> SH1106 path is inverted vs LVGL's
 * color names: lv_color_black() lights the pixel (renders white), lv_color_white()
 * leaves it off (renders black/dark). So the outline/cap/fill are drawn with
 * "black" (lit) on a "white" (dark) background.
 */
#define GAUGE_LIT lv_color_black()  /* pixel on  -> white on panel */
#define GAUGE_DARK lv_color_white() /* pixel off -> dark on panel  */

static void draw_gauge(lv_obj_t *canvas, uint8_t level) {
    lv_color_t w = GAUGE_LIT;
    lv_canvas_fill_bg(canvas, GAUGE_DARK, LV_OPA_COVER);

    /* Terminal nub (top), spanning the interior width. */
    for (int x = 1; x <= GAUGE_W - 2; x++) {
        lv_canvas_set_px(canvas, x, 0, w, LV_OPA_COVER);
        lv_canvas_set_px(canvas, x, 1, w, LV_OPA_COVER);
    }
    /* Body outline rectangle (x 0..GAUGE_W-1, y 2..GAUGE_H-1); 1px walls. */
    for (int x = 0; x < GAUGE_W; x++) {
        lv_canvas_set_px(canvas, x, 2, w, LV_OPA_COVER);
        lv_canvas_set_px(canvas, x, GAUGE_H - 1, w, LV_OPA_COVER);
    }
    for (int y = 2; y < GAUGE_H; y++) {
        lv_canvas_set_px(canvas, 0, y, w, LV_OPA_COVER);
        lv_canvas_set_px(canvas, GAUGE_W - 1, y, w, LV_OPA_COVER);
    }
    /*
     * Fill the interior from the bottom up, proportional to level, dithered as
     * a checkerboard ((x+y) parity) to roughly halve the lit pixels so it reads
     * dimmer. With a 2px interior this alternates left/right each row.
     */
    const int top = 3;            /* first interior row (below top border) */
    const int bot = GAUGE_H - 2;  /* last interior row (above bottom border) */
    const int height = bot - top + 1;
    int fill = (level * height + 50) / 100;
    for (int i = 0; i < fill; i++) {
        int y = bot - i;
        for (int x = 1; x <= GAUGE_W - 2; x++) {
            if (((x + y) & 1) == 0) {
                lv_canvas_set_px(canvas, x, y, w, LV_OPA_COVER);
            }
        }
    }
}

static void clear_gauge(lv_obj_t *canvas) {
    lv_canvas_fill_bg(canvas, GAUGE_DARK, LV_OPA_COVER);
}

static void redraw(void) {
    for (int i = 0; i < NUM_GAUGES; i++) {
        if (gauge_canvas[i] == NULL) {
            continue;
        }
        if (is_low(gauge_level[i]) && !blink_on) {
            clear_gauge(gauge_canvas[i]);
        } else {
            draw_gauge(gauge_canvas[i], gauge_level[i]);
        }
    }
}

static void blink_timer_cb(lv_timer_t *timer) {
    ARG_UNUSED(timer);
    blink_on = !blink_on;
    redraw();
}

void zmk_widget_battery_gauge_set(uint8_t left, uint8_t center, uint8_t right) {
    gauge_level[0] = left;
    gauge_level[1] = center;
    gauge_level[2] = right;
    redraw();
}

int zmk_widget_battery_gauge_init(struct zmk_widget_battery_gauge *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, NUM_GAUGES * GAUGE_W + (NUM_GAUGES - 1) * GAUGE_GAP, GAUGE_H);

    for (int i = 0; i < NUM_GAUGES; i++) {
        lv_obj_t *canvas = lv_canvas_create(widget->obj);
        lv_canvas_set_buffer(canvas, gauge_buffer[i], GAUGE_W, GAUGE_H, LV_COLOR_FORMAT_L8);
        lv_obj_align(canvas, LV_ALIGN_TOP_LEFT, i * (GAUGE_W + GAUGE_GAP), 0);
        gauge_canvas[i] = canvas;
        clear_gauge(canvas);
    }

    lv_timer_create(blink_timer_cb, CONFIG_PROSPECTOR_OLED_BATTERY_BLINK_MS, NULL);

    return 0;
}

lv_obj_t *zmk_widget_battery_gauge_obj(struct zmk_widget_battery_gauge *widget) {
    return widget->obj;
}
