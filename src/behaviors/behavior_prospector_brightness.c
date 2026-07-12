/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * Behavior that adjusts the Prospector scanner display brightness. It runs on
 * the central (where behaviors execute), nudging the requested level held by the
 * status-advertisement module; the next advertisement carries it to the scanner.
 */

#define DT_DRV_COMPAT zmk_behavior_prospector_brightness

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <zmk/behavior.h>
#include <zmk/status_advertisement.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);
    /* param1 non-zero -> brighter (+1); 0 -> dimmer (-1). Clamped in the module. */
    zmk_status_adv_brightness_adjust(binding->param1 ? +1 : -1);
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_prospector_brightness_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
};

static int behavior_prospector_brightness_init(const struct device *dev) {
    ARG_UNUSED(dev);
    return 0;
}

#define PB_INST(n)                                                                                 \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_prospector_brightness_init, NULL, NULL, NULL,              \
                            POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                       \
                            &behavior_prospector_brightness_driver_api);

DT_INST_FOREACH_STATUS_OKAY(PB_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
