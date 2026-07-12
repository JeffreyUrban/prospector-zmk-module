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

static void transport_update_cb(lv_timer_t *timer) {
    ARG_UNUSED(timer);

    scanner_process_incoming();

    int idx = zmk_status_scanner_get_primary_keyboard();
    struct zmk_keyboard_status *kbd =
        (idx >= 0) ? zmk_status_scanner_get_keyboard(idx) : NULL;
    if (kbd == NULL) {
#if IS_ENABLED(CONFIG_PROSPECTOR_MONO_BATTERY_GAUGE)
        /* No keyboard yet: still show the dongle gauge, halves empty. */
        zmk_widget_battery_gauge_set(0, dongle_battery(), 0);
#endif
        return;
    }
    const struct zmk_status_adv_data *d = &kbd->data;

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

    /* Layer: use the broadcast name if printable, else fall back to the index. */
    static char layer_name[5];
    memcpy(layer_name, d->layer_name, 4);
    layer_name[4] = '\0';
    const char *label = (layer_name[0] >= 0x20 && layer_name[0] < 0x7f) ? layer_name : NULL;
    zmk_widget_layer_status_set(d->active_layer, label);

    /* Modifiers: advertisement modifier_flags share the HID MOD_* bit layout. */
    zmk_widget_modifiers_set(d->modifier_flags);

    /* Output status: approximate the selected endpoint from the status flags. */
    const enum zmk_transport transport = usb_hid ? ZMK_TRANSPORT_USB : ZMK_TRANSPORT_BLE;
    const bool connected = usb_hid ? usb_conn : ble_conn;
    const int profile_index = PROSPECTOR_DECODE_PROFILE(d->profile_slot);
    zmk_widget_output_status_set(transport, connected, profile_index, ble_conn, ble_bond, usb_hid);

    /* WPM: number widget + bongo-cat animation, both driven by wpm_value. */
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_WPM)
    zmk_widget_wpm_status_set(d->wpm_value, label);
#endif
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_BONGO_CAT)
    zmk_widget_bongo_cat_set(d->wpm_value);
#endif
}

/* Called once from zmk_display_status_screen() (LVGL context). */
void scanner_transport_start(void) {
    lv_timer_create(transport_update_cb, 300, NULL);
}
