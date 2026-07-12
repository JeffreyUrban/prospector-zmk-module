/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * Pack/unpack for the opt-in field-selectable advertisement payload.
 * See include/zmk/status_adv_packed.h and docs/advertisement_protocol.md.
 *
 * The existing `struct zmk_status_adv_data` is reused as the field container
 * for all the small fields (both ends already speak it), so only the two things
 * that don't fit it — a possibly-longer layer name and the new brightness — are
 * passed separately.
 */

#include <zephyr/kernel.h>
#include <string.h>

#include <zmk/status_advertisement.h>
#include <zmk/status_adv_packed.h>

#if IS_ENABLED(CONFIG_PROSPECTOR_ADV_PACKED)

/* --- layer-name character alphabet --------------------------------------
 * 8-bit: raw ASCII. 6-bit: a 64-symbol table; out-of-set chars are skipped by
 * the writer, and decode maps back through the same table.
 */
#if CONFIG_PROSPECTOR_ADV_LAYER_NAME_CHAR_BITS == 6
static const char prospector_alpha6[64] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -";

static int alpha6_encode(char c) {
    for (int i = 0; i < 64; i++) {
        if (prospector_alpha6[i] == c) {
            return i;
        }
    }
    return -1; /* not representable -> skip */
}
static char alpha6_decode(uint32_t v) {
    return (v < 64) ? prospector_alpha6[v] : ' ';
}
#endif

/* Read the broadcaster's 4-byte keyboard_id as one value (matches the color
 * scanner's big-endian interpretation). */
static uint32_t kbid_to_u32(const uint8_t id[4]) {
    return ((uint32_t)id[0] << 24) | ((uint32_t)id[1] << 16) |
           ((uint32_t)id[2] << 8) | (uint32_t)id[3];
}
static void u32_to_kbid(uint32_t v, uint8_t id[4]) {
    id[0] = (uint8_t)(v >> 24);
    id[1] = (uint8_t)(v >> 16);
    id[2] = (uint8_t)(v >> 8);
    id[3] = (uint8_t)v;
}

/* Pack `base` (+ full layer name, brightness) into `out`. Returns packed byte
 * length via *out_len. */
int prospector_adv_pack(const struct zmk_status_adv_data *base,
                        const char *layer_name, uint8_t brightness,
                        uint8_t *out, size_t *out_len) {
    memset(out, 0, PROSPECTOR_ADV_PACKED_BYTES);

    /* Fixed header. */
    out[PROSPECTOR_ADV_OFF_COMPANY_ID + 0] = 0xFF;
    out[PROSPECTOR_ADV_OFF_COMPANY_ID + 1] = 0xFF;
    out[PROSPECTOR_ADV_OFF_MAGIC + 0] = 0xAB;
    out[PROSPECTOR_ADV_OFF_MAGIC + 1] = 0xCD;
    out[PROSPECTOR_ADV_OFF_VERSION] = PROSPECTOR_ADV_VERSION_HASH;
    out[PROSPECTOR_ADV_OFF_CHANNEL] = base->channel;

    size_t bit = PROSPECTOR_ADV_OFF_FIELDS * 8;

#if PROSPECTOR_ADV_EN_BATTERY_CENTRAL
    prospector_bits_put(out, &bit, base->battery_level, PROSPECTOR_ADV_W_BATTERY_CENTRAL);
#endif
#if PROSPECTOR_ADV_EN_BATTERY_PERIPH_0
    prospector_bits_put(out, &bit, base->peripheral_battery[0], PROSPECTOR_ADV_W_BATTERY_PERIPH_0);
#endif
#if PROSPECTOR_ADV_EN_BATTERY_PERIPH_1
    prospector_bits_put(out, &bit, base->peripheral_battery[1], PROSPECTOR_ADV_W_BATTERY_PERIPH_1);
#endif
#if PROSPECTOR_ADV_EN_BATTERY_PERIPH_2
    prospector_bits_put(out, &bit, base->peripheral_battery[2], PROSPECTOR_ADV_W_BATTERY_PERIPH_2);
#endif
#if PROSPECTOR_ADV_EN_ACTIVE_LAYER
    prospector_bits_put(out, &bit, base->active_layer, PROSPECTOR_ADV_W_ACTIVE_LAYER);
#endif
#if PROSPECTOR_ADV_EN_LAYER_NAME
    {
        int n = 0;
        for (int i = 0; layer_name && layer_name[i] && n < PROSPECTOR_ADV_LAYER_NAME_LEN; i++) {
#if CONFIG_PROSPECTOR_ADV_LAYER_NAME_CHAR_BITS == 6
            int code = alpha6_encode(layer_name[i]);
            if (code < 0) {
                continue; /* skip unrepresentable char */
            }
            prospector_bits_put(out, &bit, (uint32_t)code, 6);
#else
            prospector_bits_put(out, &bit, (uint8_t)layer_name[i], 8);
#endif
            n++;
        }
        /* pad remaining chars with the terminator code (0) */
        for (; n < PROSPECTOR_ADV_LAYER_NAME_LEN; n++) {
            prospector_bits_put(out, &bit, 0, CONFIG_PROSPECTOR_ADV_LAYER_NAME_CHAR_BITS);
        }
    }
#endif
#if PROSPECTOR_ADV_EN_PROFILE
    prospector_bits_put(out, &bit, PROSPECTOR_DECODE_PROFILE(base->profile_slot), PROSPECTOR_ADV_W_PROFILE);
#endif
#if PROSPECTOR_ADV_EN_PATCH_LEVEL
    prospector_bits_put(out, &bit, PROSPECTOR_DECODE_PATCH(base->profile_slot), PROSPECTOR_ADV_W_PATCH_LEVEL);
#endif
#if PROSPECTOR_ADV_EN_DEV_FLAG
    prospector_bits_put(out, &bit, PROSPECTOR_DECODE_DEV(base->profile_slot), PROSPECTOR_ADV_W_DEV_FLAG);
#endif
#if PROSPECTOR_ADV_EN_CONNECTION_COUNT
    prospector_bits_put(out, &bit, base->connection_count, PROSPECTOR_ADV_W_CONNECTION_COUNT);
#endif
#if PROSPECTOR_ADV_EN_STATUS_FLAGS
    prospector_bits_put(out, &bit, base->status_flags, PROSPECTOR_ADV_W_STATUS_FLAGS);
#endif
#if PROSPECTOR_ADV_EN_DEVICE_ROLE
    prospector_bits_put(out, &bit, base->device_role, PROSPECTOR_ADV_W_DEVICE_ROLE);
#endif
#if PROSPECTOR_ADV_EN_DEVICE_INDEX
    prospector_bits_put(out, &bit, base->device_index, PROSPECTOR_ADV_W_DEVICE_INDEX);
#endif
#if PROSPECTOR_ADV_EN_MODIFIER_FLAGS
    prospector_bits_put(out, &bit, base->modifier_flags, PROSPECTOR_ADV_W_MODIFIER_FLAGS);
#endif
#if PROSPECTOR_ADV_EN_WPM
    prospector_bits_put(out, &bit, base->wpm_value, PROSPECTOR_ADV_W_WPM);
#endif
#if PROSPECTOR_ADV_EN_BRIGHTNESS
    prospector_bits_put(out, &bit, brightness, PROSPECTOR_ADV_W_BRIGHTNESS);
#endif
#if PROSPECTOR_ADV_EN_KEYBOARD_ID
    prospector_bits_put(out, &bit,
        PROSPECTOR_ADV_TRUNC(kbid_to_u32(base->keyboard_id), PROSPECTOR_ADV_KEYBOARD_ID_BITS),
        PROSPECTOR_ADV_W_KEYBOARD_ID);
#endif

    *out_len = PROSPECTOR_ADV_PACKED_BYTES;
    return 0;
}

/* Unpack `in` into `base` (+ full layer name, brightness). Returns 0 on a
 * matching layout; -EPROTO on a version-hash mismatch; -EINVAL on bad magic. */
int prospector_adv_unpack(const uint8_t *in, size_t in_len,
                          struct zmk_status_adv_data *base,
                          char *layer_name_out, size_t layer_name_cap,
                          uint8_t *brightness_out) {
    if (in_len < PROSPECTOR_ADV_OFF_FIELDS) {
        return -EINVAL;
    }
    if (in[PROSPECTOR_ADV_OFF_MAGIC] != 0xAB || in[PROSPECTOR_ADV_OFF_MAGIC + 1] != 0xCD) {
        return -EINVAL;
    }
    base->version = in[PROSPECTOR_ADV_OFF_VERSION];
    base->channel = in[PROSPECTOR_ADV_OFF_CHANNEL];
    if (base->version != PROSPECTOR_ADV_VERSION_HASH) {
        return -EPROTO; /* layout mismatch -> caller shows mismatch state */
    }
    if (in_len < PROSPECTOR_ADV_PACKED_BYTES) {
        return -EINVAL;
    }

    /* Defaults for fields this build does not carry. */
    uint8_t profile = 0, patch = 0, dev = 0;
    if (brightness_out) {
        *brightness_out = 0;
    }
    if (layer_name_out && layer_name_cap) {
        layer_name_out[0] = '\0';
    }

    size_t bit = PROSPECTOR_ADV_OFF_FIELDS * 8;

#if PROSPECTOR_ADV_EN_BATTERY_CENTRAL
    base->battery_level = prospector_bits_get(in, &bit, PROSPECTOR_ADV_W_BATTERY_CENTRAL);
#endif
#if PROSPECTOR_ADV_EN_BATTERY_PERIPH_0
    base->peripheral_battery[0] = prospector_bits_get(in, &bit, PROSPECTOR_ADV_W_BATTERY_PERIPH_0);
#endif
#if PROSPECTOR_ADV_EN_BATTERY_PERIPH_1
    base->peripheral_battery[1] = prospector_bits_get(in, &bit, PROSPECTOR_ADV_W_BATTERY_PERIPH_1);
#endif
#if PROSPECTOR_ADV_EN_BATTERY_PERIPH_2
    base->peripheral_battery[2] = prospector_bits_get(in, &bit, PROSPECTOR_ADV_W_BATTERY_PERIPH_2);
#endif
#if PROSPECTOR_ADV_EN_ACTIVE_LAYER
    base->active_layer = prospector_bits_get(in, &bit, PROSPECTOR_ADV_W_ACTIVE_LAYER);
#endif
#if PROSPECTOR_ADV_EN_LAYER_NAME
    {
        size_t o = 0;
        for (int i = 0; i < PROSPECTOR_ADV_LAYER_NAME_LEN; i++) {
            uint32_t code = prospector_bits_get(in, &bit, CONFIG_PROSPECTOR_ADV_LAYER_NAME_CHAR_BITS);
#if CONFIG_PROSPECTOR_ADV_LAYER_NAME_CHAR_BITS == 6
            char c = (code == 0) ? '\0' : alpha6_decode(code);
#else
            char c = (char)code;
#endif
            if (c == '\0') {
                break;
            }
            if (layer_name_out && o + 1 < layer_name_cap) {
                layer_name_out[o++] = c;
            }
        }
        if (layer_name_out && o < layer_name_cap) {
            layer_name_out[o] = '\0';
        }
        /* keep base->layer_name[4] populated for any legacy reader */
        if (layer_name_out) {
            memset(base->layer_name, 0, sizeof(base->layer_name));
            memcpy(base->layer_name, layer_name_out,
                   MIN(strlen(layer_name_out), sizeof(base->layer_name)));
        }
    }
#endif
#if PROSPECTOR_ADV_EN_PROFILE
    profile = prospector_bits_get(in, &bit, PROSPECTOR_ADV_W_PROFILE);
#endif
#if PROSPECTOR_ADV_EN_PATCH_LEVEL
    patch = prospector_bits_get(in, &bit, PROSPECTOR_ADV_W_PATCH_LEVEL);
#endif
#if PROSPECTOR_ADV_EN_DEV_FLAG
    dev = prospector_bits_get(in, &bit, PROSPECTOR_ADV_W_DEV_FLAG);
#endif
    base->profile_slot = ((dev & 0x01) << 6) | ((patch & 0x07) << 3) | (profile & 0x07);
#if PROSPECTOR_ADV_EN_CONNECTION_COUNT
    base->connection_count = prospector_bits_get(in, &bit, PROSPECTOR_ADV_W_CONNECTION_COUNT);
#endif
#if PROSPECTOR_ADV_EN_STATUS_FLAGS
    base->status_flags = prospector_bits_get(in, &bit, PROSPECTOR_ADV_W_STATUS_FLAGS);
#endif
#if PROSPECTOR_ADV_EN_DEVICE_ROLE
    base->device_role = prospector_bits_get(in, &bit, PROSPECTOR_ADV_W_DEVICE_ROLE);
#endif
#if PROSPECTOR_ADV_EN_DEVICE_INDEX
    base->device_index = prospector_bits_get(in, &bit, PROSPECTOR_ADV_W_DEVICE_INDEX);
#endif
#if PROSPECTOR_ADV_EN_MODIFIER_FLAGS
    base->modifier_flags = prospector_bits_get(in, &bit, PROSPECTOR_ADV_W_MODIFIER_FLAGS);
#endif
#if PROSPECTOR_ADV_EN_WPM
    base->wpm_value = prospector_bits_get(in, &bit, PROSPECTOR_ADV_W_WPM);
#endif
#if PROSPECTOR_ADV_EN_BRIGHTNESS
    {
        uint8_t b = prospector_bits_get(in, &bit, PROSPECTOR_ADV_W_BRIGHTNESS);
        if (brightness_out) {
            *brightness_out = b;
        }
    }
#endif
#if PROSPECTOR_ADV_EN_KEYBOARD_ID
    u32_to_kbid(prospector_bits_get(in, &bit, PROSPECTOR_ADV_W_KEYBOARD_ID), base->keyboard_id);
#endif

    return 0;
}

#endif /* CONFIG_PROSPECTOR_ADV_PACKED */
