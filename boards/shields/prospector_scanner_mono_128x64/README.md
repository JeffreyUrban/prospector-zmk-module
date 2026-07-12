# prospector_scanner_mono_128x64

A Prospector BLE status scanner for a **128x64 monochrome OLED** (SH1106) on a
nice!nano v2. It passively receives ZMK status advertisements and shows keyboard
status on a mono LVGL screen (widgets ported from
[`englmaxi/zmk-dongle-display`](https://github.com/englmaxi/zmk-dongle-display) —
see `ATTRIBUTION.md`).

## Required: pair the scanner to your keyboard with a channel

This display has **no touch/button input** (the only button is reset), so —
unlike the color `prospector_scanner` — it cannot select a keyboard at runtime.
It simply shows the keyboard it hears. If more than one Prospector keyboard is
ever in range (an office, a shared space), an unpaired scanner would show
whichever keyboard advertised most recently, which is not acceptable.

**You must pin this scanner to your keyboard by channel.** Channel filtering is
enforced in the scanner core before any advertisement is tracked:

- Scanner channel **0** (default): receives every keyboard (unpaired).
- Scanner channel **1-255**: receives **only** the keyboard broadcasting that
  exact channel; all others — including uncoordinated keyboards left on channel
  0 — are ignored.

### How to pair

1. **Pick a random channel from 1-255.** Use the full range and choose randomly
   so you don't collide with nearby people who haven't coordinated with you
   (e.g. `echo $(( (RANDOM % 255) + 1 ))`). Avoid low, "obvious" numbers.
2. Set it on the **scanner**: `CONFIG_PROSPECTOR_SCANNER_CHANNEL=<n>` (in your
   `zmk-config` for this shield).
3. Set the **same** value on the **keyboard**: `CONFIG_PROSPECTOR_CHANNEL=<n>`.

Both sides must match. After this, the scanner shows only your keyboard, no
matter how many other Prospector keyboards are nearby.

> Note: strict pairing works for the full 1-255 range. (An upstream bug that
> made channels >= 10 behave as "accept all" — capping strict pairing at 9
> channels — is fixed in this fork.)
