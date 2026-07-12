# Attribution

The monochrome status display for this shield — the LVGL widgets and the
status-screen layout that assembles them — is derived from
**[englmaxi/zmk-dongle-display](https://github.com/englmaxi/zmk-dongle-display)**.

The presentation code (widget rendering, image/symbol data, and the
`custom_status_screen` layout) is kept as close to the original as possible;
only the data transport was changed — instead of local ZMK keyboard events,
the widgets are fed from BLE status advertisements received by the Prospector
scanner.

Derived files (under `src/`):

- `custom_status_screen.c` / `.h` (status-screen layout / assembly)
- `widgets/battery_status.c` / `.h` (optional legacy battery widget)
- `widgets/layer_status.c` / `.h`
- `widgets/modifiers.c` / `.h`, `widgets/modifiers_sym.c`
- `widgets/output_status.c` / `.h`, `widgets/output_status_sym.c`
- `widgets/wpm_status.c` / `.h`, `widgets/wpm_status_sym.c`
- `widgets/bongo_cat.c` / `.h`, `widgets/bongo_cat_images.c`

## Original license

```
MIT License

Copyright (c) 2024 Maximilian Engl

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
