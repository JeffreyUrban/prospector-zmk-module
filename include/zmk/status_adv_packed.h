/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * Prospector packed, field-selectable advertisement payload.
 *
 * Opt-in alternative to the fixed `struct zmk_status_adv_data` wire format,
 * enabled by CONFIG_PROSPECTOR_ADV_PACKED. Every field's presence and width is
 * a *build-time* contract shared by the keyboard (packer) and scanner
 * (unpacker), so the wire carries only values — no presence map, no length
 * prefixes (see docs/advertisement_protocol.md).
 *
 * Design decisions baked in here:
 *   - Explicit MSB-first bit codec (no C bitfields; layout defined by our code).
 *   - The `version` byte is a compile-time layout hash: it changes whenever the
 *     protocol version, the selected field set, or any variable width changes,
 *     so a mismatched pair is detected. Fully constant-folded (no runtime cost).
 *   - A BUILD_ASSERT fails the build if the selected fields exceed the budget.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <zephyr/sys/util.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Budget
 *
 * BLE legacy adv = 31 bytes: Flags(3) + [len(1)+type(1)] + manufacturer data.
 * The manufacturer data we build is at most 26 bytes and starts with the
 * 0xFFFF company id, matching the legacy struct so the scanner's magic check is
 * unchanged. Fixed header = company_id(2) + magic(2) + version(1) + channel(1).
 * ------------------------------------------------------------------------- */
#define PROSPECTOR_ADV_MAX_BYTES        26
#define PROSPECTOR_ADV_HEADER_BYTES     6
#define PROSPECTOR_ADV_HEADER_BITS      (PROSPECTOR_ADV_HEADER_BYTES * 8)
#define PROSPECTOR_ADV_BUDGET_BITS      (PROSPECTOR_ADV_MAX_BYTES * 8)
#define PROSPECTOR_ADV_FIELD_BUDGET_BITS \
    (PROSPECTOR_ADV_BUDGET_BITS - PROSPECTOR_ADV_HEADER_BITS)   /* 160 */

/* Fixed-header byte offsets in the packed buffer. */
#define PROSPECTOR_ADV_OFF_COMPANY_ID   0   /* 0xFF 0xFF */
#define PROSPECTOR_ADV_OFF_MAGIC        2   /* 0xAB 0xCD */
#define PROSPECTOR_ADV_OFF_VERSION      4   /* layout hash */
#define PROSPECTOR_ADV_OFF_CHANNEL      5   /* pairing/filter */
#define PROSPECTOR_ADV_OFF_FIELDS       6   /* packed fields begin here */

/* -------------------------------------------------------------------------
 * Field registry: stable ID (= canonical pack order) and fixed width in bits.
 * Variable widths (layer_name, keyboard_id) are computed from Kconfig below.
 * ------------------------------------------------------------------------- */
enum prospector_adv_field {
    PROSPECTOR_ADV_F_BATTERY_CENTRAL = 0,
    PROSPECTOR_ADV_F_BATTERY_PERIPH_0,
    PROSPECTOR_ADV_F_BATTERY_PERIPH_1,
    PROSPECTOR_ADV_F_BATTERY_PERIPH_2,
    PROSPECTOR_ADV_F_ACTIVE_LAYER,
    PROSPECTOR_ADV_F_LAYER_NAME,
    PROSPECTOR_ADV_F_PROFILE,
    PROSPECTOR_ADV_F_PATCH_LEVEL,
    PROSPECTOR_ADV_F_DEV_FLAG,
    PROSPECTOR_ADV_F_CONNECTION_COUNT,
    PROSPECTOR_ADV_F_STATUS_FLAGS,
    PROSPECTOR_ADV_F_DEVICE_ROLE,
    PROSPECTOR_ADV_F_DEVICE_INDEX,
    PROSPECTOR_ADV_F_MODIFIER_FLAGS,
    PROSPECTOR_ADV_F_WPM,
    PROSPECTOR_ADV_F_BRIGHTNESS,
    PROSPECTOR_ADV_F_KEYBOARD_ID,
    PROSPECTOR_ADV_F_COUNT
};

/* Fixed per-field widths (bits). */
#define PROSPECTOR_ADV_W_BATTERY_CENTRAL   7
#define PROSPECTOR_ADV_W_BATTERY_PERIPH_0  7
#define PROSPECTOR_ADV_W_BATTERY_PERIPH_1  7
#define PROSPECTOR_ADV_W_BATTERY_PERIPH_2  7
#define PROSPECTOR_ADV_W_ACTIVE_LAYER      4
#define PROSPECTOR_ADV_W_PROFILE           3
#define PROSPECTOR_ADV_W_PATCH_LEVEL       3
#define PROSPECTOR_ADV_W_DEV_FLAG          1
#define PROSPECTOR_ADV_W_CONNECTION_COUNT  3
#define PROSPECTOR_ADV_W_STATUS_FLAGS      6
#define PROSPECTOR_ADV_W_DEVICE_ROLE       2
#define PROSPECTOR_ADV_W_DEVICE_INDEX      3
#define PROSPECTOR_ADV_W_MODIFIER_FLAGS    8
#define PROSPECTOR_ADV_W_WPM               7
#define PROSPECTOR_ADV_W_BRIGHTNESS        3   /* 8 levels: 0=off .. 7=max */

/* -------------------------------------------------------------------------
 * MSB-first bit codec. `bitpos` is a running cursor in bits from the start of
 * the buffer; both helpers advance it. Widths up to 32 bits.
 * ------------------------------------------------------------------------- */
static inline void prospector_bits_put(uint8_t *buf, size_t *bitpos,
                                       uint32_t value, unsigned width) {
    for (unsigned i = 0; i < width; i++) {
        size_t b = *bitpos + i;
        unsigned mask = 0x80u >> (b & 7u);
        if ((value >> (width - 1u - i)) & 1u) {
            buf[b >> 3] |= (uint8_t)mask;      /* MSB of value goes first */
        } else {
            buf[b >> 3] &= (uint8_t)~mask;
        }
    }
    *bitpos += width;
}

static inline uint32_t prospector_bits_get(const uint8_t *buf, size_t *bitpos,
                                           unsigned width) {
    uint32_t v = 0;
    for (unsigned i = 0; i < width; i++) {
        size_t b = *bitpos + i;
        v = (v << 1) | ((buf[b >> 3] >> (7u - (b & 7u))) & 1u);
    }
    *bitpos += width;
    return v;
}

/* -------------------------------------------------------------------------
 * Build-time layout: only meaningful when the packed format is selected.
 * ------------------------------------------------------------------------- */
#if IS_ENABLED(CONFIG_PROSPECTOR_ADV_PACKED)

/*
 * Optional manual input to the layout fingerprint, for a *semantic* change to
 * an existing field that keeps the same bit position and width (rare). Field
 * presence, widths, and order are captured automatically below, so you do NOT
 * bump this for adding/removing/resizing fields — and NEVER for module
 * releases, git revisions, or anything that doesn't change the wire interface.
 */
#define PROSPECTOR_ADV_PROTOCOL_VERSION 1

/* Variable widths from Kconfig. */
#define PROSPECTOR_ADV_LAYER_NAME_LEN       CONFIG_PROSPECTOR_ADV_LAYER_NAME_LEN
#define PROSPECTOR_ADV_LAYER_NAME_CHAR_BITS CONFIG_PROSPECTOR_ADV_LAYER_NAME_CHAR_BITS
#define PROSPECTOR_ADV_KEYBOARD_ID_BITS     CONFIG_PROSPECTOR_ADV_KEYBOARD_ID_BITS
#define PROSPECTOR_ADV_W_LAYER_NAME \
    (PROSPECTOR_ADV_LAYER_NAME_LEN * PROSPECTOR_ADV_LAYER_NAME_CHAR_BITS)
#define PROSPECTOR_ADV_W_KEYBOARD_ID        PROSPECTOR_ADV_KEYBOARD_ID_BITS

/* Per-field enabled flags (1/0), from Kconfig, evaluated at compile time. */
#define PROSPECTOR_ADV_EN_BATTERY_CENTRAL   IS_ENABLED(CONFIG_PROSPECTOR_ADV_FIELD_BATTERY_CENTRAL)
#define PROSPECTOR_ADV_EN_BATTERY_PERIPH_0  IS_ENABLED(CONFIG_PROSPECTOR_ADV_FIELD_BATTERY_PERIPH_0)
#define PROSPECTOR_ADV_EN_BATTERY_PERIPH_1  IS_ENABLED(CONFIG_PROSPECTOR_ADV_FIELD_BATTERY_PERIPH_1)
#define PROSPECTOR_ADV_EN_BATTERY_PERIPH_2  IS_ENABLED(CONFIG_PROSPECTOR_ADV_FIELD_BATTERY_PERIPH_2)
#define PROSPECTOR_ADV_EN_ACTIVE_LAYER      IS_ENABLED(CONFIG_PROSPECTOR_ADV_FIELD_ACTIVE_LAYER)
#define PROSPECTOR_ADV_EN_LAYER_NAME        IS_ENABLED(CONFIG_PROSPECTOR_ADV_FIELD_LAYER_NAME)
#define PROSPECTOR_ADV_EN_PROFILE           IS_ENABLED(CONFIG_PROSPECTOR_ADV_FIELD_PROFILE)
#define PROSPECTOR_ADV_EN_PATCH_LEVEL       IS_ENABLED(CONFIG_PROSPECTOR_ADV_FIELD_PATCH_LEVEL)
#define PROSPECTOR_ADV_EN_DEV_FLAG          IS_ENABLED(CONFIG_PROSPECTOR_ADV_FIELD_DEV_FLAG)
#define PROSPECTOR_ADV_EN_CONNECTION_COUNT  IS_ENABLED(CONFIG_PROSPECTOR_ADV_FIELD_CONNECTION_COUNT)
#define PROSPECTOR_ADV_EN_STATUS_FLAGS      IS_ENABLED(CONFIG_PROSPECTOR_ADV_FIELD_STATUS_FLAGS)
#define PROSPECTOR_ADV_EN_DEVICE_ROLE       IS_ENABLED(CONFIG_PROSPECTOR_ADV_FIELD_DEVICE_ROLE)
#define PROSPECTOR_ADV_EN_DEVICE_INDEX      IS_ENABLED(CONFIG_PROSPECTOR_ADV_FIELD_DEVICE_INDEX)
#define PROSPECTOR_ADV_EN_MODIFIER_FLAGS    IS_ENABLED(CONFIG_PROSPECTOR_ADV_FIELD_MODIFIER_FLAGS)
#define PROSPECTOR_ADV_EN_WPM               IS_ENABLED(CONFIG_PROSPECTOR_ADV_FIELD_WPM)
#define PROSPECTOR_ADV_EN_BRIGHTNESS        IS_ENABLED(CONFIG_PROSPECTOR_ADV_FIELD_BRIGHTNESS)
#define PROSPECTOR_ADV_EN_KEYBOARD_ID       IS_ENABLED(CONFIG_PROSPECTOR_ADV_FIELD_KEYBOARD_ID)

/* Bitmap of enabled fields (bit N set => field ID N present). */
#define PROSPECTOR_ADV_ENABLED_MASK ( \
    ((uint32_t)PROSPECTOR_ADV_EN_BATTERY_CENTRAL  << PROSPECTOR_ADV_F_BATTERY_CENTRAL)  | \
    ((uint32_t)PROSPECTOR_ADV_EN_BATTERY_PERIPH_0 << PROSPECTOR_ADV_F_BATTERY_PERIPH_0) | \
    ((uint32_t)PROSPECTOR_ADV_EN_BATTERY_PERIPH_1 << PROSPECTOR_ADV_F_BATTERY_PERIPH_1) | \
    ((uint32_t)PROSPECTOR_ADV_EN_BATTERY_PERIPH_2 << PROSPECTOR_ADV_F_BATTERY_PERIPH_2) | \
    ((uint32_t)PROSPECTOR_ADV_EN_ACTIVE_LAYER     << PROSPECTOR_ADV_F_ACTIVE_LAYER)     | \
    ((uint32_t)PROSPECTOR_ADV_EN_LAYER_NAME       << PROSPECTOR_ADV_F_LAYER_NAME)       | \
    ((uint32_t)PROSPECTOR_ADV_EN_PROFILE          << PROSPECTOR_ADV_F_PROFILE)          | \
    ((uint32_t)PROSPECTOR_ADV_EN_PATCH_LEVEL      << PROSPECTOR_ADV_F_PATCH_LEVEL)      | \
    ((uint32_t)PROSPECTOR_ADV_EN_DEV_FLAG         << PROSPECTOR_ADV_F_DEV_FLAG)         | \
    ((uint32_t)PROSPECTOR_ADV_EN_CONNECTION_COUNT << PROSPECTOR_ADV_F_CONNECTION_COUNT) | \
    ((uint32_t)PROSPECTOR_ADV_EN_STATUS_FLAGS     << PROSPECTOR_ADV_F_STATUS_FLAGS)     | \
    ((uint32_t)PROSPECTOR_ADV_EN_DEVICE_ROLE      << PROSPECTOR_ADV_F_DEVICE_ROLE)      | \
    ((uint32_t)PROSPECTOR_ADV_EN_DEVICE_INDEX     << PROSPECTOR_ADV_F_DEVICE_INDEX)     | \
    ((uint32_t)PROSPECTOR_ADV_EN_MODIFIER_FLAGS   << PROSPECTOR_ADV_F_MODIFIER_FLAGS)   | \
    ((uint32_t)PROSPECTOR_ADV_EN_WPM              << PROSPECTOR_ADV_F_WPM)              | \
    ((uint32_t)PROSPECTOR_ADV_EN_BRIGHTNESS       << PROSPECTOR_ADV_F_BRIGHTNESS)       | \
    ((uint32_t)PROSPECTOR_ADV_EN_KEYBOARD_ID      << PROSPECTOR_ADV_F_KEYBOARD_ID))

/* Total selected field bits (enabled widths summed at compile time). */
#define PROSPECTOR_ADV_FIELDS_BITS ( \
    PROSPECTOR_ADV_EN_BATTERY_CENTRAL  * PROSPECTOR_ADV_W_BATTERY_CENTRAL  + \
    PROSPECTOR_ADV_EN_BATTERY_PERIPH_0 * PROSPECTOR_ADV_W_BATTERY_PERIPH_0 + \
    PROSPECTOR_ADV_EN_BATTERY_PERIPH_1 * PROSPECTOR_ADV_W_BATTERY_PERIPH_1 + \
    PROSPECTOR_ADV_EN_BATTERY_PERIPH_2 * PROSPECTOR_ADV_W_BATTERY_PERIPH_2 + \
    PROSPECTOR_ADV_EN_ACTIVE_LAYER     * PROSPECTOR_ADV_W_ACTIVE_LAYER     + \
    PROSPECTOR_ADV_EN_LAYER_NAME       * PROSPECTOR_ADV_W_LAYER_NAME       + \
    PROSPECTOR_ADV_EN_PROFILE          * PROSPECTOR_ADV_W_PROFILE          + \
    PROSPECTOR_ADV_EN_PATCH_LEVEL      * PROSPECTOR_ADV_W_PATCH_LEVEL      + \
    PROSPECTOR_ADV_EN_DEV_FLAG         * PROSPECTOR_ADV_W_DEV_FLAG         + \
    PROSPECTOR_ADV_EN_CONNECTION_COUNT * PROSPECTOR_ADV_W_CONNECTION_COUNT + \
    PROSPECTOR_ADV_EN_STATUS_FLAGS     * PROSPECTOR_ADV_W_STATUS_FLAGS     + \
    PROSPECTOR_ADV_EN_DEVICE_ROLE      * PROSPECTOR_ADV_W_DEVICE_ROLE      + \
    PROSPECTOR_ADV_EN_DEVICE_INDEX     * PROSPECTOR_ADV_W_DEVICE_INDEX     + \
    PROSPECTOR_ADV_EN_MODIFIER_FLAGS   * PROSPECTOR_ADV_W_MODIFIER_FLAGS   + \
    PROSPECTOR_ADV_EN_WPM              * PROSPECTOR_ADV_W_WPM              + \
    PROSPECTOR_ADV_EN_BRIGHTNESS       * PROSPECTOR_ADV_W_BRIGHTNESS       + \
    PROSPECTOR_ADV_EN_KEYBOARD_ID      * PROSPECTOR_ADV_W_KEYBOARD_ID)

/* Total packed size in bytes (header + fields, rounded up). */
#define PROSPECTOR_ADV_PACKED_BYTES \
    (PROSPECTOR_ADV_HEADER_BYTES + \
     (PROSPECTOR_ADV_FIELDS_BITS + 7) / 8)

/* Hard budget check: fail the build (both ends) rather than truncate. */
BUILD_ASSERT(PROSPECTOR_ADV_FIELDS_BITS <= PROSPECTOR_ADV_FIELD_BUDGET_BITS,
             "Prospector packed advertisement over budget: "
             "reduce CONFIG_PROSPECTOR_ADV_LAYER_NAME_LEN or disable a field.");

/*
 * Layout fingerprint -> the on-wire `version` byte. Constant-folded (no runtime
 * input). It is a pure function of the COMMUNICATION STRUCTURE ONLY: for every
 * field, whether it is present and its bit width -- and, since each field
 * contributes at its own ID position, the field order too. It therefore changes
 * if and only if the wire layout changes, and NEVER when the module version, a
 * git revision, the build date, or any protocol-irrelevant config changes.
 * Keyboard and scanner agree exactly when their packed layouts match. Not
 * cryptographic -- a change detector.
 */
#define PROSPECTOR_ADV_FLD_FP(idx, en, w) \
    ((uint32_t)((en) ? ((uint32_t)(w) + 1u) : 0u) * (0x9E3779B1u * ((uint32_t)(idx) + 1u)))

#define PROSPECTOR_ADV_FP32 ( \
    ((uint32_t)PROSPECTOR_ADV_PROTOCOL_VERSION * 0x01000193u) ^ \
    PROSPECTOR_ADV_FLD_FP(0,  PROSPECTOR_ADV_EN_BATTERY_CENTRAL,  PROSPECTOR_ADV_W_BATTERY_CENTRAL)  ^ \
    PROSPECTOR_ADV_FLD_FP(1,  PROSPECTOR_ADV_EN_BATTERY_PERIPH_0, PROSPECTOR_ADV_W_BATTERY_PERIPH_0) ^ \
    PROSPECTOR_ADV_FLD_FP(2,  PROSPECTOR_ADV_EN_BATTERY_PERIPH_1, PROSPECTOR_ADV_W_BATTERY_PERIPH_1) ^ \
    PROSPECTOR_ADV_FLD_FP(3,  PROSPECTOR_ADV_EN_BATTERY_PERIPH_2, PROSPECTOR_ADV_W_BATTERY_PERIPH_2) ^ \
    PROSPECTOR_ADV_FLD_FP(4,  PROSPECTOR_ADV_EN_ACTIVE_LAYER,     PROSPECTOR_ADV_W_ACTIVE_LAYER)     ^ \
    PROSPECTOR_ADV_FLD_FP(5,  PROSPECTOR_ADV_EN_LAYER_NAME,       PROSPECTOR_ADV_W_LAYER_NAME)       ^ \
    PROSPECTOR_ADV_FLD_FP(6,  PROSPECTOR_ADV_EN_PROFILE,          PROSPECTOR_ADV_W_PROFILE)          ^ \
    PROSPECTOR_ADV_FLD_FP(7,  PROSPECTOR_ADV_EN_PATCH_LEVEL,      PROSPECTOR_ADV_W_PATCH_LEVEL)      ^ \
    PROSPECTOR_ADV_FLD_FP(8,  PROSPECTOR_ADV_EN_DEV_FLAG,         PROSPECTOR_ADV_W_DEV_FLAG)         ^ \
    PROSPECTOR_ADV_FLD_FP(9,  PROSPECTOR_ADV_EN_CONNECTION_COUNT, PROSPECTOR_ADV_W_CONNECTION_COUNT) ^ \
    PROSPECTOR_ADV_FLD_FP(10, PROSPECTOR_ADV_EN_STATUS_FLAGS,     PROSPECTOR_ADV_W_STATUS_FLAGS)     ^ \
    PROSPECTOR_ADV_FLD_FP(11, PROSPECTOR_ADV_EN_DEVICE_ROLE,      PROSPECTOR_ADV_W_DEVICE_ROLE)      ^ \
    PROSPECTOR_ADV_FLD_FP(12, PROSPECTOR_ADV_EN_DEVICE_INDEX,     PROSPECTOR_ADV_W_DEVICE_INDEX)     ^ \
    PROSPECTOR_ADV_FLD_FP(13, PROSPECTOR_ADV_EN_MODIFIER_FLAGS,   PROSPECTOR_ADV_W_MODIFIER_FLAGS)   ^ \
    PROSPECTOR_ADV_FLD_FP(14, PROSPECTOR_ADV_EN_WPM,              PROSPECTOR_ADV_W_WPM)              ^ \
    PROSPECTOR_ADV_FLD_FP(15, PROSPECTOR_ADV_EN_BRIGHTNESS,       PROSPECTOR_ADV_W_BRIGHTNESS)       ^ \
    PROSPECTOR_ADV_FLD_FP(16, PROSPECTOR_ADV_EN_KEYBOARD_ID,      PROSPECTOR_ADV_W_KEYBOARD_ID))

/* Fold the 32-bit fingerprint down to the one on-wire byte. */
#define PROSPECTOR_ADV_VERSION_HASH \
    ((uint8_t)((PROSPECTOR_ADV_FP32 ^ (PROSPECTOR_ADV_FP32 >> 8) ^ \
                (PROSPECTOR_ADV_FP32 >> 16) ^ (PROSPECTOR_ADV_FP32 >> 24)) & 0xFFu))

/* Fold a 32-bit value to the low N bits (keyboard_id truncation). */
#define PROSPECTOR_ADV_TRUNC(v, bits) \
    ((bits) >= 32 ? (uint32_t)(v) : ((uint32_t)(v) & (((uint32_t)1 << (bits)) - 1)))

/* -------------------------------------------------------------------------
 * Codec (implemented in src/status_adv_packed.c). `base` is the shared field
 * container; the possibly-longer layer name and brightness are passed apart
 * because they don't fit the legacy struct.
 * ------------------------------------------------------------------------- */
struct zmk_status_adv_data;

/* Serialize into `out` (>= PROSPECTOR_ADV_PACKED_BYTES). Returns 0; sets *out_len. */
int prospector_adv_pack(const struct zmk_status_adv_data *base,
                        const char *layer_name, uint8_t brightness,
                        uint8_t *out, size_t *out_len);

/* Deserialize `in`. Returns 0 on match, -EPROTO on version-hash mismatch,
 * -EINVAL on bad magic / short buffer. */
int prospector_adv_unpack(const uint8_t *in, size_t in_len,
                          struct zmk_status_adv_data *base,
                          char *layer_name_out, size_t layer_name_cap,
                          uint8_t *brightness_out);

#endif /* CONFIG_PROSPECTOR_ADV_PACKED */

#ifdef __cplusplus
}
#endif
