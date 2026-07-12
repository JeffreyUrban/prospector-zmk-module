/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * Lean scanner stub for the mono OLED scanner.
 *
 * The core (src/status_scanner.c) runs BLE scanning on the BT RX thread and
 * hands each parsed advertisement to scanner_msg_send_keyboard_data(). That
 * pushes into a lock-free SPSC ring buffer. scanner_process_incoming() drains
 * the ring into keyboards[] and is called only from the display timer
 * (LVGL/main context), so keyboards[] needs no locking.
 *
 * This implements the scanner_stub.h contract that the core links against
 * (the color prospector_scanner shield has its own, richer implementation).
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <errno.h>

#include <zmk/status_scanner.h>
#include <zmk/status_advertisement.h>
#include "../../prospector_scanner/src/scanner_stub.h"

LOG_MODULE_REGISTER(oled_scanner, LOG_LEVEL_INF);

#define MAX_KEYBOARDS ZMK_STATUS_SCANNER_MAX_KEYBOARDS
/*
 * Drop a keyboard only after this long without an advertisement. Uses the
 * module's scanner timeout (default 8 min), which must exceed the keyboard's
 * idle advertisement interval so an idle keyboard isn't dropped (and its
 * battery isn't zeroed) between infrequent idle broadcasts. 0 = never drop.
 */
#define KEYBOARD_TIMEOUT_MS CONFIG_PROSPECTOR_SCANNER_TIMEOUT_MS

/* keyboards[] and selected_keyboard are touched only from the display timer. */
static struct zmk_keyboard_status keyboards[MAX_KEYBOARDS];
static int selected_keyboard;

/* ---- Lock-free SPSC ring buffer (BT RX writes, display timer reads) ---- */
struct incoming_adv {
    struct zmk_status_adv_data data;
    int8_t rssi;
    char name[32];
    uint8_t ble_addr[6];
    uint8_t ble_addr_type;
};

#define INCOMING_BUF_SIZE 8 /* power of two */
static struct incoming_adv incoming_buf[INCOMING_BUF_SIZE];
static volatile uint8_t incoming_w; /* written by BT RX only */
static volatile uint8_t incoming_r; /* written by display timer only */

static int incoming_push(const struct incoming_adv *e) {
    uint8_t next = (incoming_w + 1) & (INCOMING_BUF_SIZE - 1);
    if (next == incoming_r) {
        return -ENOMEM; /* full: drop (next advertisement will catch up) */
    }
    incoming_buf[incoming_w] = *e;
    incoming_w = next;
    return 0;
}

static bool incoming_pop(struct incoming_adv *out) {
    if (incoming_r == incoming_w) {
        return false;
    }
    *out = incoming_buf[incoming_r];
    incoming_r = (incoming_r + 1) & (INCOMING_BUF_SIZE - 1);
    return true;
}

/* ---- Contract: called from the BT RX thread ---- */
int scanner_msg_send_keyboard_data(const struct zmk_status_adv_data *adv_data,
                                   int8_t rssi, const char *device_name,
                                   const uint8_t *ble_addr, uint8_t ble_addr_type) {
    struct incoming_adv e = {0};
    e.data = *adv_data;
    e.rssi = rssi;
    if (device_name) {
        strncpy(e.name, device_name, sizeof(e.name) - 1);
    }
    if (ble_addr) {
        memcpy(e.ble_addr, ble_addr, 6);
    }
    e.ble_addr_type = ble_addr_type;
    return incoming_push(&e);
}

/* ---- Contract: called from the display timer (LVGL context) ---- */
void scanner_process_incoming(void) {
    struct incoming_adv e;
    while (incoming_pop(&e)) {
        int slot = -1, free_slot = -1;
        for (int i = 0; i < MAX_KEYBOARDS; i++) {
            if (keyboards[i].active &&
                memcmp(keyboards[i].ble_addr, e.ble_addr, 6) == 0) {
                slot = i;
                break;
            }
            if (!keyboards[i].active && free_slot < 0) {
                free_slot = i;
            }
        }
        if (slot < 0) {
            slot = (free_slot >= 0) ? free_slot : 0; /* evict slot 0 if full */
        }

        keyboards[slot].active = true;
        keyboards[slot].last_seen = k_uptime_get_32();
        keyboards[slot].data = e.data;
        keyboards[slot].rssi = e.rssi;
        strncpy(keyboards[slot].ble_name, e.name, sizeof(keyboards[slot].ble_name) - 1);
        keyboards[slot].ble_name[sizeof(keyboards[slot].ble_name) - 1] = '\0';
        memcpy(keyboards[slot].ble_addr, e.ble_addr, 6);
        keyboards[slot].ble_addr_type = e.ble_addr_type;
    }

    if (KEYBOARD_TIMEOUT_MS > 0) {
        uint32_t now = k_uptime_get_32();
        for (int i = 0; i < MAX_KEYBOARDS; i++) {
            if (keyboards[i].active &&
                (now - keyboards[i].last_seen) > KEYBOARD_TIMEOUT_MS) {
                keyboards[i].active = false;
            }
        }
    }
}

/* ---- Contract: accessors (display context) ---- */
struct zmk_keyboard_status *scanner_get_keyboard_status(int index) {
    if (index < 0 || index >= MAX_KEYBOARDS || !keyboards[index].active) {
        return NULL;
    }
    return &keyboards[index];
}

int scanner_get_active_keyboard_count(void) {
    int count = 0;
    for (int i = 0; i < MAX_KEYBOARDS; i++) {
        if (keyboards[i].active) {
            count++;
        }
    }
    return count;
}

bool scanner_get_keyboard_data(int index, struct zmk_status_adv_data *data,
                               int8_t *rssi, char *name, size_t name_len) {
    if (index < 0 || index >= MAX_KEYBOARDS || !keyboards[index].active) {
        return false;
    }
    if (data) {
        *data = keyboards[index].data;
    }
    if (rssi) {
        *rssi = keyboards[index].rssi;
    }
    if (name && name_len) {
        strncpy(name, keyboards[index].ble_name, name_len - 1);
        name[name_len - 1] = '\0';
    }
    return true;
}

int scanner_get_selected_keyboard(void) {
    return selected_keyboard;
}

void scanner_set_selected_keyboard(int index) {
    if (index >= 0 && index < MAX_KEYBOARDS) {
        selected_keyboard = index;
    }
}

/* Not used by the mono OLED UI, but part of the contract. */
int scanner_msg_send_timeout_check(void) {
    return 0;
}

int scanner_msg_send_display_refresh(void) {
    return 0;
}

/* ---- Start BLE scanning shortly after boot (retry until BT is ready) ---- */
static void scanner_start_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(scanner_start_work, scanner_start_work_handler);

static void scanner_start_work_handler(struct k_work *work) {
    ARG_UNUSED(work);
    int ret = zmk_status_scanner_start();
    if (ret == 0) {
        LOG_INF("OLED scanner: BLE scanning started");
    } else {
        LOG_WRN("OLED scanner: start failed (%d), retrying", ret);
        k_work_schedule(&scanner_start_work, K_SECONDS(1));
    }
}

static int oled_scanner_init(void) {
    k_work_schedule(&scanner_start_work, K_MSEC(500));
    return 0;
}

SYS_INIT(oled_scanner_init, APPLICATION, 98);
