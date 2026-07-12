# Copyright (c) 2024 The ZMK Contributors
# SPDX-License-Identifier: MIT

if(CONFIG_SHIELD_PROSPECTOR_SCANNER_MONO_128X64)
    if(CONFIG_ZMK_DISPLAY_STATUS_SCREEN_CUSTOM)
        # Display presentation (status screen + widgets) ported from
        # englmaxi/zmk-dongle-display. See ATTRIBUTION.md.
        target_sources(app PRIVATE src/custom_status_screen.c)
        target_sources(app PRIVATE src/widgets/output_status.c)
        target_sources(app PRIVATE src/widgets/output_status_sym.c)
        target_sources_ifdef(CONFIG_PROSPECTOR_MONO_BATTERY_GAUGE app PRIVATE
            src/widgets/battery_gauge.c)
        target_sources_ifdef(CONFIG_PROSPECTOR_MONO_BATTERY_LEGACY app PRIVATE
            src/widgets/battery_status.c)
        target_sources(app PRIVATE src/widgets/layer_status.c)
        target_sources(app PRIVATE src/widgets/modifiers.c)
        target_sources(app PRIVATE src/widgets/modifiers_sym.c)
        target_sources(app PRIVATE src/widgets/signal_status.c)
        target_sources_ifdef(CONFIG_ZMK_DONGLE_DISPLAY_WPM app PRIVATE
            src/widgets/wpm_status.c src/widgets/wpm_status_sym.c)
        target_sources_ifdef(CONFIG_ZMK_DONGLE_DISPLAY_BONGO_CAT app PRIVATE
            src/widgets/bongo_cat.c src/widgets/bongo_cat_images.c)
    endif()

    # Scanner data path + transport that feeds the widgets.
    target_sources_ifdef(CONFIG_PROSPECTOR_MODE_SCANNER app PRIVATE src/scanner_stub.c)
    target_sources_ifdef(CONFIG_PROSPECTOR_MODE_SCANNER app PRIVATE src/scanner_transport.c)
    # Feeds received advertisements into ZMK's activity monitor (display wake).
    target_sources_ifdef(CONFIG_PROSPECTOR_MODE_SCANNER app PRIVATE src/scanner_activity.c)

    target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
endif()
