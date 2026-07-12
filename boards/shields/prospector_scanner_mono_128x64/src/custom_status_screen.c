/*
 * Copyright (c) 2024 The ZMK Contributors
 * Copyright (c) 2024 Maximilian Engl
 *
 * SPDX-License-Identifier: MIT
 *
 * Status-screen layout / assembly derived from englmaxi/zmk-dongle-display
 * (MIT). Kept verbatim except for the single scanner_transport_start() call
 * that starts the data transport feeding the widgets. See ATTRIBUTION.md.
 */

/* Transport hook: starts the LVGL timer that feeds widgets from the scanner. */
void scanner_transport_start(void);

#include "custom_status_screen.h"
#include "widgets/battery_gauge.h"
#include "widgets/battery_status.h"
#include "widgets/signal_status.h"
#include "widgets/modifiers.h"
#include "widgets/bongo_cat.h"
#include "widgets/layer_status.h"
#include "widgets/output_status.h"
#include "widgets/hid_indicators.h"
#include "widgets/wpm_status.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static struct zmk_widget_output_status output_status_widget;

/* Full-screen takeover shown only on a packed-layout version mismatch. */
static lv_obj_t *mismatch_overlay;

void scanner_show_version_mismatch(bool show) {
    if (mismatch_overlay == NULL) {
        return;
    }
    if (show) {
        lv_obj_clear_flag(mismatch_overlay, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(mismatch_overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

#if IS_ENABLED(CONFIG_PROSPECTOR_MONO_BATTERY_GAUGE)
static struct zmk_widget_battery_gauge battery_gauge_widget;
static struct zmk_widget_signal_status signal_status_widget;
#else
static struct zmk_widget_dongle_battery_status dongle_battery_status_widget;
#endif

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_LAYER)
static struct zmk_widget_layer_status layer_status_widget;
#endif

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_MODIFIERS)
static struct zmk_widget_modifiers modifiers_widget;
#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
static struct zmk_widget_hid_indicators hid_indicators_widget;
#endif

#endif

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_BONGO_CAT)
static struct zmk_widget_bongo_cat bongo_cat_widget;
#endif

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_WPM)
static struct zmk_widget_wpm_status wpm_status_widget;
#endif

lv_style_t global_style;

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen;

    screen = lv_obj_create(NULL);

    lv_style_init(&global_style);
    lv_style_set_bg_color(&global_style, lv_color_white());
    lv_style_set_bg_opa(&global_style, LV_OPA_COVER);
    lv_style_set_text_color(&global_style, lv_color_black());
    lv_style_set_text_font(&global_style, &lv_font_unscii_8);
    lv_style_set_text_letter_space(&global_style, 1);
    lv_style_set_text_line_space(&global_style, 1);
    lv_obj_add_style(screen, &global_style, LV_PART_MAIN);
    
    zmk_widget_output_status_init(&output_status_widget, screen);
    lv_obj_align(zmk_widget_output_status_obj(&output_status_widget), LV_ALIGN_TOP_LEFT, 0, 0);

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_WPM)
    zmk_widget_wpm_status_init(&wpm_status_widget, screen);
    lv_obj_align_to(zmk_widget_wpm_status_obj(&wpm_status_widget), zmk_widget_output_status_obj(&output_status_widget), LV_ALIGN_OUT_RIGHT_MID, 7, 0);
#endif

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_BONGO_CAT)
    zmk_widget_bongo_cat_init(&bongo_cat_widget, screen);
    lv_obj_align(zmk_widget_bongo_cat_obj(&bongo_cat_widget), LV_ALIGN_BOTTOM_RIGHT, 0, -7);
#endif

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_MODIFIERS)
    zmk_widget_modifiers_init(&modifiers_widget, screen);
    lv_obj_align(zmk_widget_modifiers_obj(&modifiers_widget), LV_ALIGN_BOTTOM_LEFT, 0, 0);
#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
    zmk_widget_hid_indicators_init(&hid_indicators_widget, screen);
    lv_obj_align_to(zmk_widget_hid_indicators_obj(&hid_indicators_widget), zmk_widget_modifiers_obj(&modifiers_widget), LV_ALIGN_OUT_TOP_LEFT, 0, -2);
#endif
#endif

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_LAYER)
    zmk_widget_layer_status_init(&layer_status_widget, screen);
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_BONGO_CAT)
    lv_obj_align_to(zmk_widget_layer_status_obj(&layer_status_widget), zmk_widget_bongo_cat_obj(&bongo_cat_widget), LV_ALIGN_BOTTOM_RIGHT, 0, 5);
#else
    lv_obj_align(zmk_widget_layer_status_obj(&layer_status_widget), LV_ALIGN_BOTTOM_RIGHT, 0, -3);
#endif
#endif

#if IS_ENABLED(CONFIG_PROSPECTOR_MONO_BATTERY_GAUGE)
    zmk_widget_battery_gauge_init(&battery_gauge_widget, screen);
    lv_obj_align(zmk_widget_battery_gauge_obj(&battery_gauge_widget), LV_ALIGN_TOP_RIGHT, 0, 0);
#else
    zmk_widget_dongle_battery_status_init(&dongle_battery_status_widget, screen);
    lv_obj_align(zmk_widget_dongle_battery_status_obj(&dongle_battery_status_widget), LV_ALIGN_TOP_RIGHT, 0, 0);
#endif

    /* Signal-strength meter, just left of the battery gauges. */
    zmk_widget_signal_status_init(&signal_status_widget, screen);
    lv_obj_align(zmk_widget_signal_status_obj(&signal_status_widget), LV_ALIGN_TOP_RIGHT, -24, 6);

    /*
     * Version-mismatch overlay: full-screen, opaque, created last so it sits on
     * top of every widget. Hidden until the transport detects a channel-matched
     * keyboard whose packed layout version differs from ours. Factual text only
     * (no call to action). Mono polarity: white bg = dark panel, black = lit.
     */
    mismatch_overlay = lv_obj_create(screen);
    lv_obj_remove_style_all(mismatch_overlay);
    lv_obj_set_size(mismatch_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(mismatch_overlay, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(mismatch_overlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(mismatch_overlay, LV_OBJ_FLAG_SCROLLABLE);
    {
        lv_obj_t *msg = lv_label_create(mismatch_overlay);
        lv_label_set_text(msg, "VERSION\nMISMATCH");
        lv_obj_set_style_text_color(msg, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_text_font(msg, &lv_font_unscii_8, LV_PART_MAIN);
        lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_center(msg);
    }
    lv_obj_add_flag(mismatch_overlay, LV_OBJ_FLAG_HIDDEN);

    scanner_transport_start();

    return screen;
}
