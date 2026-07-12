/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * A ZMK event raised when the scanner receives a keyboard status advertisement.
 * ZMK's activity monitor is subscribed to it (see scanner_activity.c), so a
 * received advertisement resets the idle timer -- keeping the display awake
 * while a keyboard is broadcasting, and letting it idle-blank (power save) only
 * when no keyboard is around. Only valid keyboard advertisements reach this;
 * unrelated BLE advertisements are filtered out upstream in status_scanner.c.
 */

#pragma once

#include <zmk/event_manager.h>

struct zmk_status_scanner_activity {
    uint8_t reserved;
};

ZMK_EVENT_DECLARE(zmk_status_scanner_activity);

/*
 * Note that a keyboard advertisement was received. Safe to call from the BT RX
 * thread; the event raise is deferred to the system workqueue.
 */
void scanner_note_keyboard_activity(void);
