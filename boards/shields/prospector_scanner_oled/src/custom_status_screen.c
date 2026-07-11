/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * Prospector Scanner OLED - custom status screen.
 *
 * Bring-up placeholder with EXPLICIT monochrome colors (black background,
 * white foreground) and a crisp native 1bpp font. Real scanner widgets
 * (ported from englmaxi) replace this later.
 */

#include <lvgl.h>

/*
 * Monochrome polarity quirk on this stack: with LVGL 1bpp feeding the Zephyr
 * SSD1306/SH1106 driver, the on-panel result is inverted relative to LVGL's
 * color names -- lv_color_black() lights the pixel (bright) and lv_color_white()
 * leaves it off (dark). Setting the DT `inversion-on` flips both the pixel
 * format and the panel's normal/reverse command, so it cancels out and does not
 * help. We want a dark panel with bright text, so use these semantic aliases.
 */
#define OLED_DARK lv_color_white() /* pixel off -> dark background */
#define OLED_LIT  lv_color_black() /* pixel on  -> bright foreground */

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);

    /* Dark background, no border/padding. */
    lv_obj_set_style_bg_color(screen, OLED_DARK, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(screen, 0, LV_PART_MAIN);

    /* Bright text in a crisp 1bpp font, centered. */
    lv_obj_t *label = lv_label_create(screen);
    lv_obj_set_style_text_color(label, OLED_LIT, LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_unscii_8, LV_PART_MAIN);
    lv_label_set_text(label, "Prospector\nOLED scanner");
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    return screen;
}
