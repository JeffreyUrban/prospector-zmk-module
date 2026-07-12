/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * BLE Scanner - Receives keyboard advertisements and pushes to ring buffer.
 *
 * Architecture:
 *   BT RX thread → scan_callback() → parse ADV → scanner_msg_send_keyboard_data()
 *   LVGL timer   → scanner_process_incoming() (in scanner_stub.c) → keyboards[] → widget
 *
 * This file does NOT touch keyboards[] or any display state.
 * All keyboard state management is in scanner_stub.c (LVGL timer context only).
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/logging/log.h>
#include <zephyr/net_buf.h>
#include <zephyr/init.h>
#include <string.h>

#include <zmk/status_scanner.h>
#include <zmk/status_advertisement.h>
#include <zmk/status_adv_packed.h>

// Scanner stub functions for lock-free ring buffer push
#include "../boards/shields/prospector_scanner/src/scanner_stub.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if IS_ENABLED(CONFIG_PROSPECTOR_MODE_SCANNER)

static bool scanning = false;

/* ========== Temporary Device Name Cache ========== */
/* Correlates BLE address with device name from SCAN_RSP packets.
 * Only used within scan_callback context (BT RX thread). */

static struct {
    bt_addr_le_t addr;
    char name[32];
    uint32_t timestamp;
} temp_device_names[5];

static void store_device_name(const bt_addr_le_t *addr, const char *name) {
    uint32_t now = k_uptime_get_32();
    int oldest_idx = 0;
    uint32_t oldest_time = UINT32_MAX;

    for (int i = 0; i < 5; i++) {
        /* Exact match: update existing entry */
        if (bt_addr_le_cmp(&temp_device_names[i].addr, addr) == 0) {
            strncpy(temp_device_names[i].name, name, sizeof(temp_device_names[i].name) - 1);
            temp_device_names[i].name[sizeof(temp_device_names[i].name) - 1] = '\0';
            temp_device_names[i].timestamp = now;
            return;
        }
        /* Track oldest entry for LRU eviction */
        if (temp_device_names[i].timestamp < oldest_time) {
            oldest_time = temp_device_names[i].timestamp;
            oldest_idx = i;
        }
    }

    /* No match found: evict oldest entry (LRU) */
    bt_addr_le_copy(&temp_device_names[oldest_idx].addr, addr);
    strncpy(temp_device_names[oldest_idx].name, name, sizeof(temp_device_names[oldest_idx].name) - 1);
    temp_device_names[oldest_idx].name[sizeof(temp_device_names[oldest_idx].name) - 1] = '\0';
    temp_device_names[oldest_idx].timestamp = now;
}

static const char *get_device_name(const bt_addr_le_t *addr) {
    for (int i = 0; i < 5; i++) {
        if (bt_addr_le_cmp(&temp_device_names[i].addr, addr) == 0) {
            return temp_device_names[i].name;
        }
    }
    return "Unknown";
}

/* ========== BLE Scan Callback ========== */
/* Runs in BT RX thread. Parses ADV packets, pushes to ring buffer. */

static void scan_callback(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
                         struct net_buf_simple *buf) {
    static int scan_count = 0;
    scan_count++;

    if (scan_count % 100 == 1) {
        LOG_INF("BLE scan #%d (RSSI: %d)", scan_count, rssi);
    }

    if (!scanning) {
        return;
    }

    const struct zmk_status_adv_data *prospector_data = NULL;
    const char *fwd_layer_name = NULL;   /* full layer name to forward (packed) */
    uint8_t fwd_brightness = 0;
#if IS_ENABLED(CONFIG_PROSPECTOR_ADV_PACKED)
    /* BT RX thread only -> static scratch for the unpacked result. */
    static struct zmk_status_adv_data unpacked;
    static char unpacked_layer[ZMK_STATUS_LAYER_NAME_MAX];
#endif

    /* Parse advertisement data to extract both name and Prospector data */
    struct net_buf_simple buf_copy = *buf;
    while (buf_copy.len > 1) {
        uint8_t len = net_buf_simple_pull_u8(&buf_copy);
        if (len == 0 || len > buf_copy.len) {
            break;
        }

        uint8_t ad_type = net_buf_simple_pull_u8(&buf_copy);
        len--;

        /* Extract device name */
        if ((ad_type == BT_DATA_NAME_COMPLETE || ad_type == BT_DATA_NAME_SHORTENED) && len > 0) {
            char device_name[32];
            memcpy(device_name, buf_copy.data, MIN(len, sizeof(device_name) - 1));
            device_name[MIN(len, sizeof(device_name) - 1)] = '\0';

            LOG_DBG("%s packet - %s name (len=%d): '%s'",
                   (type & BT_HCI_LE_ADV_EVT_TYPE_SCAN_RSP) ? "SCAN_RSP" : "ADV",
                   (ad_type == BT_DATA_NAME_COMPLETE) ? "COMPLETE" : "SHORTENED",
                   len, device_name);

            if (ad_type == BT_DATA_NAME_SHORTENED && strncmp(device_name, "LalaPad", 7) == 0) {
                strcpy(device_name, "LalaPadmini");
                LOG_INF("Expanded shortened name to: %s", device_name);
            }

            store_device_name(addr, device_name);
        }

        /* Check for Prospector manufacturer data */
        if (ad_type == BT_DATA_MANUFACTURER_DATA) {
            const uint8_t *mfg = buf_copy.data;
            /* Magic sits at fixed offsets 0-3 in BOTH the legacy struct and the
             * packed header, so detection is format-independent. */
            bool is_prospector = (len >= 4 &&
                                  mfg[0] == 0xFF && mfg[1] == 0xFF &&
                                  mfg[2] == 0xAB && mfg[3] == 0xCD);
            if (is_prospector) {
                /* Scanner's own channel: runtime override, else Kconfig. */
                extern uint8_t scanner_get_runtime_channel(void) __attribute__((weak));
                uint8_t scanner_channel = 0;
                if (scanner_get_runtime_channel) {
                    scanner_channel = scanner_get_runtime_channel();
                }
#ifdef CONFIG_PROSPECTOR_SCANNER_CHANNEL
                else {
                    scanner_channel = CONFIG_PROSPECTOR_SCANNER_CHANNEL;
                }
#endif
#if IS_ENABLED(CONFIG_PROSPECTOR_ADV_PACKED)
                uint8_t keyboard_channel =
                    (len > PROSPECTOR_ADV_OFF_CHANNEL) ? mfg[PROSPECTOR_ADV_OFF_CHANNEL] : 0;
#else
                uint8_t keyboard_channel =
                    (len >= sizeof(struct zmk_status_adv_data)) ?
                        ((const struct zmk_status_adv_data *)mfg)->channel : 0;
#endif
                /*
                 * Channel pairing:
                 *   scanner channel 0  -> receive every keyboard (default,
                 *                         unpaired; includes channel-0 keyboards).
                 *   scanner channel N  -> receive ONLY keyboards on channel N, so
                 *                         a pinned scanner never shows the wrong
                 *                         keyboard.
                 */
                bool channel_match = (scanner_channel == 0 ||
                                      scanner_channel == keyboard_channel);
                if (channel_match) {
#if IS_ENABLED(CONFIG_PROSPECTOR_ADV_PACKED)
                    int r = prospector_adv_unpack(mfg, len, &unpacked, unpacked_layer,
                                                  sizeof(unpacked_layer), &fwd_brightness);
                    if (r == 0) {
                        prospector_data = &unpacked;
                        fwd_layer_name = unpacked_layer;
                        LOG_DBG("Valid packed data: Ch:%d->%d Bat=%d%%",
                                keyboard_channel, scanner_channel, unpacked.battery_level);
                    } else if (r == -EPROTO) {
                        /* Channel-matched but layout version differs: alert. */
                        extern void scanner_report_version_mismatch(void) __attribute__((weak));
                        if (scanner_report_version_mismatch) {
                            scanner_report_version_mismatch();
                        }
                        LOG_DBG("Packed adv version mismatch (Ch:%d)", keyboard_channel);
                    }
#else
                    if (len >= sizeof(struct zmk_status_adv_data)) {
                        prospector_data = (const struct zmk_status_adv_data *)mfg;
                        LOG_DBG("Valid Prospector data: Ch:%d->%d Ver=%d Bat=%d%%",
                                keyboard_channel, scanner_channel,
                                prospector_data->version, prospector_data->battery_level);
                    }
#endif
                } else {
                    LOG_DBG("Channel mismatch - KB Ch:%d, Scanner Ch:%d (filtered)",
                            keyboard_channel, scanner_channel);
                }
            }
        }

        net_buf_simple_pull(&buf_copy, len);
    }

    /* Push to ring buffer for LVGL timer to process */
    if (prospector_data) {
        LOG_DBG("Central=%d%%, Peripheral=[%d,%d,%d], Layer=%d",
               prospector_data->battery_level, prospector_data->peripheral_battery[0],
               prospector_data->peripheral_battery[1], prospector_data->peripheral_battery[2],
               prospector_data->active_layer);

        /* Legacy path has no separate full name: derive it from the 4-char field. */
        char legacy_name[5];
        if (fwd_layer_name == NULL) {
            memcpy(legacy_name, prospector_data->layer_name, 4);
            legacy_name[4] = '\0';
            fwd_layer_name = legacy_name;
        }

        const char *device_name = get_device_name(addr);
        int ret = scanner_msg_send_keyboard_data(prospector_data, rssi, device_name,
                                                  addr->a.val, addr->type,
                                                  fwd_layer_name, fwd_brightness);
        if (ret != 0) {
            LOG_DBG("Ring buffer full, advertisement dropped");
        }
    }
}

/* ========== Public API ========== */
/* All keyboard state queries delegate to scanner_stub.c (single source of truth) */

int zmk_status_scanner_init(void) {
    LOG_INF("Status scanner initialized (lock-free architecture)");
    return 0;
}

int zmk_status_scanner_start(void) {
    if (scanning) {
        return 0;
    }

    struct bt_le_scan_param scan_param = {
        .type = BT_LE_SCAN_TYPE_ACTIVE,
        .options = BT_LE_SCAN_OPT_NONE,
        .interval = BT_GAP_SCAN_FAST_WINDOW,
        .window = BT_GAP_SCAN_FAST_WINDOW,
    };

    int err = bt_le_scan_start(&scan_param, scan_callback);
    if (err) {
        LOG_ERR("Failed to start scanning: %d", err);
        return err;
    }

    scanning = true;
    LOG_INF("Status scanner started (ACTIVE mode, 100%% duty cycle)");
    return 0;
}

int zmk_status_scanner_stop(void) {
    if (!scanning) {
        return 0;
    }

    scanning = false;

    int err = bt_le_scan_stop();
    if (err) {
        LOG_ERR("Failed to stop scanning: %d", err);
        return err;
    }

    LOG_INF("Status scanner stopped");
    return 0;
}

int zmk_status_scanner_register_callback(zmk_status_scanner_callback_t callback) {
    /* No-op: callbacks removed in lock-free architecture.
     * Display updates via pending_data in scanner_stub.c. */
    ARG_UNUSED(callback);
    return 0;
}

struct zmk_keyboard_status *zmk_status_scanner_get_keyboard(int index) {
    return scanner_get_keyboard_status(index);
}

int zmk_status_scanner_get_active_count(void) {
    return scanner_get_active_keyboard_count();
}

int zmk_status_scanner_get_primary_keyboard(void) {
    /* Find most recently seen keyboard in scanner_stub.c's keyboards[] */
    int primary = -1;
    uint32_t latest_seen = 0;

    for (int i = 0; i < ZMK_STATUS_SCANNER_MAX_KEYBOARDS; i++) {
        struct zmk_keyboard_status *kbd = scanner_get_keyboard_status(i);
        if (kbd && kbd->last_seen > latest_seen) {
            latest_seen = kbd->last_seen;
            primary = i;
        }
    }

    return primary;
}

SYS_INIT(zmk_status_scanner_init, APPLICATION, 99);

#endif /* CONFIG_PROSPECTOR_MODE_SCANNER */
