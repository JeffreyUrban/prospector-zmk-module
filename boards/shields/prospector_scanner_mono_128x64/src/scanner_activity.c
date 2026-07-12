/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * Wires received keyboard advertisements into ZMK's activity monitor so the
 * display stays awake while the (paired) keyboard is broadcasting, and
 * idle-blanks for power saving only when no keyboard is around.
 *
 * This uses ZMK's own event-subscription mechanism -- no core patch, no
 * synthetic key events. Only valid, channel-matched keyboard advertisements
 * reach scanner_note_keyboard_activity() (filtered in status_scanner.c), so
 * unrelated BLE traffic never wakes the display.
 */

#include <zephyr/kernel.h>
#include <zmk/event_manager.h>

#include "scanner_activity.h"

ZMK_EVENT_IMPL(zmk_status_scanner_activity);

/*
 * Subscribe ZMK's core activity listener (zmk_listener_activity, a global const
 * defined by ZMK_LISTENER in app/src/activity.c) to our event. Raising the
 * event makes the activity monitor run note_activity(): it resets the idle
 * timer and returns to the ACTIVE state, which unblanks the display.
 */
ZMK_SUBSCRIPTION(activity, zmk_status_scanner_activity);

static void activity_work_cb(struct k_work *work) {
    ARG_UNUSED(work);
    raise_zmk_status_scanner_activity((struct zmk_status_scanner_activity){0});
}
static K_WORK_DEFINE(activity_work, activity_work_cb);

void scanner_note_keyboard_activity(void) {
    /*
     * Called from the BT RX thread. Defer the event raise to the system
     * workqueue -- k_work_submit is idempotent when already queued, so bursts
     * of advertisements coalesce into one activity reset.
     */
    k_work_submit(&activity_work);
}
