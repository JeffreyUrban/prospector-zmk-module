/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * Transport layer for the ported dongle_display presentation.
 *
 * englmaxi's widgets self-update from local ZMK keyboard events. A scanner has
 * no local keyboard, so this file is the replacement transport: an LVGL timer
 * drains the scanner ring buffer and pushes the primary keyboard's
 * advertisement fields into each widget's setter. All presentation (widgets +
 * status-screen layout) is unchanged; only this data source is new.
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/display.h>
#include <lvgl.h>
#include <string.h>

#include <zmk/status_scanner.h>
#include <zmk/status_advertisement.h>
#include <zmk/endpoints.h>
#if IS_ENABLED(CONFIG_ZMK_BATTERY)
#include <zmk/battery.h>
#endif

#include "widgets/battery_gauge.h"
#include "widgets/battery_status.h"
#include "widgets/signal_status.h"
#include "widgets/layer_status.h"
#include "widgets/modifiers.h"
#include "widgets/output_status.h"
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_WPM)
#include "widgets/wpm_status.h"
#endif
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_BONGO_CAT)
#include "widgets/bongo_cat.h"
#endif

/* Provided by scanner_stub.c: drains the BT-RX ring buffer into keyboards[]. */
void scanner_process_incoming(void);

/* Packed-layout version-mismatch state (scanner_stub.c) + overlay toggle
 * (custom_status_screen.c). */
extern bool scanner_version_mismatch_active(void);
extern void scanner_show_version_mismatch(bool show);

#if IS_ENABLED(CONFIG_PROSPECTOR_MONO_BATTERY_GAUGE)
/* The dongle's (scanner device's) own battery, if it reports one. */
static uint8_t dongle_battery(void) {
#if IS_ENABLED(CONFIG_ZMK_BATTERY)
    return zmk_battery_state_of_charge();
#else
    return 0;
#endif
}
#endif /* CONFIG_PROSPECTOR_MONO_BATTERY_GAUGE */

#if IS_ENABLED(CONFIG_PROSPECTOR_ADV_FIELD_BRIGHTNESS) && DT_HAS_CHOSEN(zephyr_display)
/*
 * Map the requested level (0=off .. 7=max) to the SH1106 contrast register
 * (0..255) on a perceptual (gamma ~2.2) ramp, so the steps feel evenly spaced
 * rather than bunched at the top. Level 0 blanks the panel. This is a tunable
 * table -- the true contrast->luminance curve is display-specific.
 */
static const uint8_t brightness_contrast[PROSPECTOR_BRIGHTNESS_LEVELS] = {
    0, 8, 20, 42, 78, 125, 185, 255,
};

static void apply_brightness(uint8_t level) {
    static int last = -1;
    if (level >= PROSPECTOR_BRIGHTNESS_LEVELS || (int)level == last) {
        return;
    }
    last = level;

    const struct device *disp = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(disp)) {
        return;
    }
    if (level == 0) {
        display_blanking_on(disp);
    } else {
        display_blanking_off(disp);
        display_set_contrast(disp, brightness_contrast[level]);
    }
}
#else
static inline void apply_brightness(uint8_t level) { ARG_UNUSED(level); }
#endif

static void transport_update_cb(lv_timer_t *timer) {
    ARG_UNUSED(timer);

    scanner_process_incoming();

    int idx = zmk_status_scanner_get_primary_keyboard();
    struct zmk_keyboard_status *kbd =
        (idx >= 0) ? zmk_status_scanner_get_keyboard(idx) : NULL;

    /*
     * Version-mismatch takeover: a channel-matched keyboard we can't decode
     * (mismatched packed layout). Only when there's no valid keyboard to show;
     * a decodable keyboard always wins. The overlay covers the whole screen.
     */
    if (kbd == NULL && scanner_version_mismatch_active()) {
        scanner_show_version_mismatch(true);
        return;
    }
    scanner_show_version_mismatch(false);

    if (kbd == NULL) {
#if IS_ENABLED(CONFIG_PROSPECTOR_MONO_BATTERY_GAUGE)
        /* No keyboard yet: still show the dongle gauge, halves empty. */
        zmk_widget_battery_gauge_set(0, dongle_battery(), 0);
#endif
        zmk_widget_signal_status_set(0, false);
        /* No keyboard -> default to max brightness so the display stays visible. */
        apply_brightness(PROSPECTOR_BRIGHTNESS_MAX);
        return;
    }
    const struct zmk_status_adv_data *d = &kbd->data;

    zmk_widget_signal_status_set(kbd->rssi, true);

    /* Apply the keyboard's requested display brightness (packed builds). */
    apply_brightness(kbd->brightness);

    const uint8_t sf = d->status_flags;
    const bool usb_conn = sf & ZMK_STATUS_FLAG_USB_CONNECTED;
    const bool usb_hid = sf & ZMK_STATUS_FLAG_USB_HID_READY;
    const bool ble_conn = sf & ZMK_STATUS_FLAG_BLE_CONNECTED;
    const bool ble_bond = sf & ZMK_STATUS_FLAG_BLE_BONDED;

#if IS_ENABLED(CONFIG_PROSPECTOR_MONO_BATTERY_GAUGE)
    /*
     * Battery gauges: center = dongle, outer two = keyboard halves. The
     * advertisement carries the central half's battery (battery_level) and the
     * peripheral half (peripheral_battery[0]); which physical side is central
     * is a scanner-side config.
     */
    const uint8_t central = d->battery_level;
    const uint8_t peripheral = d->peripheral_battery[0];
    uint8_t left, right;
    if (strcmp(CONFIG_PROSPECTOR_MONO_CENTRAL_SIDE, "RIGHT") == 0) {
        left = peripheral;
        right = central;
    } else {
        left = central;
        right = peripheral;
    }
    zmk_widget_battery_gauge_set(left, dongle_battery(), right);
#else
    /* Legacy dongle_display battery: source 0 = central, 1.. = peripherals. */
    zmk_widget_dongle_battery_status_set(0, d->battery_level, usb_conn);
    for (int i = 0; i < 3; i++) {
        zmk_widget_dongle_battery_status_set(1 + i, d->peripheral_battery[i], false);
    }
#endif

    /* Layer: use the broadcast name (full, packed builds) if printable, else
     * fall back to the index. */
    const char *ln = kbd->layer_name_full;
    const char *label = (ln[0] >= 0x20 && ln[0] < 0x7f) ? ln : NULL;
    zmk_widget_layer_status_set(d->active_layer, label);

    /* Modifiers: advertisement modifier_flags share the HID MOD_* bit layout. */
    zmk_widget_modifiers_set(d->modifier_flags);

    /* Output status: approximate the selected endpoint from the status flags. */
    const enum zmk_transport transport = usb_hid ? ZMK_TRANSPORT_USB : ZMK_TRANSPORT_BLE;
    const bool connected = usb_hid ? usb_conn : ble_conn;
    const int profile_index = PROSPECTOR_DECODE_PROFILE(d->profile_slot);
    zmk_widget_output_status_set(transport, connected, profile_index, ble_conn, ble_bond, usb_hid);

    /*
     * WPM: while the keyboard sends frequent (typing) ads, show its live value.
     * Once typing stops, the keyboard's broadcast value freezes (it only sends
     * sparse idle ads), so decay the shown value to zero here instead. Drives
     * both the number widget and the bongo-cat animation.
     */
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_WPM) || IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_BONGO_CAT)
    extern bool scanner_keyboard_active(void);
    static uint8_t shown_wpm;
    if (scanner_keyboard_active()) {
        shown_wpm = d->wpm_value;
    } else if (shown_wpm > 0) {
        shown_wpm = (uint8_t)((shown_wpm * 3) / 4); /* ~25%/tick -> 0 in a few seconds */
    }
#endif
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_WPM)
    zmk_widget_wpm_status_set(shown_wpm, label);
#endif
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_BONGO_CAT)
    zmk_widget_bongo_cat_set(shown_wpm);
#endif
}

/* Called once from zmk_display_status_screen() (LVGL context). */
void scanner_transport_start(void) {
    lv_timer_create(transport_update_cb, 300, NULL);
}
