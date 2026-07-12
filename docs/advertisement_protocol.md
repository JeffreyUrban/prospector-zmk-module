# Prospector Advertisement Protocol — Packed & Field-Selectable (Draft v0)

Status: **design draft for review, no code yet.**

## Goals

- **Bit-packed** payload — every field gets exactly the bits it needs, no
  byte-boundary waste.
- **Field-selectable** — each build (via Kconfig) chooses *which* fields it
  broadcasts; it spends the byte budget on what matters to that build.
- **Build-time contract** — the keyboard and its scanner are built with the
  same field selection, so the exact wire layout is known to both at compile
  time. Because we own both ends, nothing about the layout is transmitted:
  - **no presence map** (which fields are present is fixed by shared config)
  - **no per-field length prefixes** (variable fields are fixed-width with a
    build-time max, null-padded)
- **One-byte guard** — the existing `version` byte does its normal job
  (compatibility): if the two sides disagree, the scanner doesn't trust the
  bytes. Its value should cover the field selection too (see below).
- **Build-time budget assert** — selecting more than fits is a build error,
  never a silent truncation.
- Upstreamable: the generality lives in the registry + Kconfig + build check;
  the runtime packet stays minimal.

## BLE budget

Legacy advertising = 31 bytes total.

| Consumer | Bytes |
|---|---|
| Flags AD (len, type, data) | 3 |
| Mfg AD framing: len + type(0xFF) | 2 |
| **Manufacturer data we build** | **26 (208 bits)** |

The 26-byte manufacturer buffer we build starts with the 0xFFFF company id (so
BLE frames it and the scanner's magic check is unchanged), then our header, then
the packed fields.

## Fixed header — always present (6 bytes / 48 bits)

| Field | Width | Notes |
|---|---|---|
| company id (0xFFFF) | 16 b | BLE manufacturer company id; also half the magic check |
| magic (0xABCD) | 16 b | marks a Prospector ad; used by the core scan filter |
| version | 8 b  | structure fingerprint; scanner decodes only if it matches its own (see below) |
| channel | 8 b | pairing/filter; consumed by the core scan_callback, so mandatory |

Leaves **160 bits** for selectable fields (the `BUILD_ASSERT` budget).

## Selectable field registry (canonical order = ID order)

| ID | Field | Width | Range / notes |
|---|---|---|---|
| 0  | battery_central   | 7 b | 0–100 |
| 1  | battery_periph_0  | 7 b | 0–100 |
| 2  | battery_periph_1  | 7 b | 0–100 |
| 3  | battery_periph_2  | 7 b | 0–100 |
| 4  | active_layer      | 4 b | 0–15 |
| 5  | layer_name        | N×W b | N = `LAYER_NAME_LEN`, W = `LAYER_NAME_CHAR_BITS` (6/7/8) |
| 6  | profile           | 3 b | 0–4 |
| 7  | patch_level       | 3 b | |
| 8  | dev_flag          | 1 b | |
| 9  | connection_count  | 3 b | 0–5 |
| 10 | status_flags      | 6 b | USB conn / USB HID / BLE conn / BLE bond |
| 11 | device_role       | 2 b | 0–2 |
| 12 | device_index      | 3 b | |
| 13 | modifier_flags    | 8 b | HID MOD_* bit layout |
| 14 | wpm               | 7 b | 0–127 |
| 15 | brightness        | 2 b | 4 levels (NEW; transmitted as *state*, keyboard holds the level) |
| 16 | keyboard_id       | ≤32 b | identity (multi-keyboard, upstream); `_KEYBOARD_ID_BITS`, default 32 (= existing behavior) |

RSSI is receiver-measured (scanner-side), never transmitted.

## Layer name (user-configurable, budget-driving)

Two user Kconfig settings:

- `CONFIG_PROSPECTOR_ADV_LAYER_NAME_LEN` — number of characters. This is the
  primary budget lever: the field costs `LEN × CHAR_BITS` bits, which counts
  toward the total like any other field. Longer name → fewer bits for
  everything else. There is no wire length prefix; the field is fixed at `LEN`
  and null-padded/terminated.
- `CONFIG_PROSPECTOR_ADV_LAYER_NAME_CHAR_BITS` — bits per character:
  - **8 b** — full printable ASCII, simplest, no alphabet table. *(default — matches the "breadth of symbols over length" priority)*
  - **6 b** — 64-symbol alphabet (upper+lower+digits+2 symbols); out-of-set chars skipped.

If the chosen length (together with the rest of the selected fields) exceeds the
budget, the **build fails** — see below.

## Version (compatibility)

One byte, doing its normal job: if the keyboard's value ≠ the scanner's, the
scanner treats the layout as untrusted (→ "update me" state) instead of
decoding garbage. It must cover the **field selection**, not just the release —
two same-release builds with different selected fields have incompatible
layouts.

**Decided: a build-time constant fingerprint of the wire structure only.** The
byte folds, for every field, `(present? width : 0)` mixed at the field's ID
position — so it captures the **selected set, every width, and the field order**.
It is a pure function of the **communication interface structure**, and changes
**if and only if** the wire layout changes — never on a module-version, git,
build-date, or otherwise protocol-irrelevant change. The hash has *no runtime
input*, so it is fully **constant-folded at compile time** into the literal
`PROSPECTOR_ADV_VERSION_HASH`. Both keyboard and scanner `#include` the registry
against the same config and bake in the **identical literal**; the keyboard
transmits it, the scanner compares to its own copy. No per-ad hashing, no codegen.

`PROTOCOL_VERSION` is a small optional input for a *semantic* change to an
existing field that keeps the same bits (rare) — never bumped for releases or
anything that doesn't change the interface.

Residual risk: an 8-bit hash means two *different* layouts collide (same value,
no alert) with probability 1/256 — only relevant during an update window, low
stakes, and widenable to 16 bits later if ever wanted.

## Build-time surface

- `CONFIG_PROSPECTOR_ADV_FIELD_<NAME>=y` per field.
- `CONFIG_PROSPECTOR_ADV_LAYER_NAME_LEN`, `_LAYER_NAME_CHAR_BITS`.
- `CONFIG_PROSPECTOR_ADV_KEYBOARD_ID_BITS` — new (upstream has none); default 32
  to preserve current behavior. Used when `keyboard_id` is selected; a truncated
  low-N-bit slice of the 32-bit HWINFO hash. Note the color scanner uses this for
  cross-address identity, so shrinking it trades collision-resistance (negligible
  at 16 b for a handful of keyboards).
- **Budget check (hard error):** the shared header sums the enabled field widths
  (including `LAYER_NAME_LEN × CHAR_BITS`) plus the 32-bit fixed header and
  asserts it fits the 192-bit usable payload:

  ```c
  BUILD_ASSERT(PROSPECTOR_ADV_HEADER_BITS + PROSPECTOR_ADV_FIELDS_BITS
                   <= PROSPECTOR_ADV_BUDGET_BITS,
               "Prospector advertisement over budget: reduce LAYER_NAME_LEN "
               "or disable a field.");
  ```

  Both the keyboard and scanner include this header, so an over-budget config
  fails *both* builds with the same message — no silent truncation, no way to
  ship a packet that doesn't fit.

## Framing & length — why there are no length fields

Two separate things, both correctly absent:

- **Per-field length** (TLV-style): not present. The field set and every width
  are a build-time contract shared by both ends, so field boundaries are known
  without self-description. Per-field lengths exist to skip *unknown* fields in
  self-describing formats (protobuf/TLV) — a cost we don't pay.
- **Total-message length**: not present, and not needed — **BLE already provides
  it.** Advertising data is a sequence of `[len][type][data]` AD structures; the
  Manufacturer Specific Data element carries its own length in the BLE framing,
  so the scan callback receives the manufacturer payload with its byte length
  for free. An in-payload length would just duplicate the container's framing.

The payload is bit-packed, so the final byte may be partially used; the trailing
**pad bits are ignored** — the scanner reads exactly the bits the known layout
defines and stops, so field boundaries never depend on the padding. The layer
name is fixed-length null-padded (not runtime-variable), so for a given config
the whole message is a **constant size** — meaning the BLE-reported length is a
free sanity check (must equal the compile-time-expected size; another mismatch
signal alongside the version).

Principle: explicit length framing belongs at a layer with genuine runtime
variable length and no outer delimiter (raw streams; or self-describing formats
that tolerate unknown fields). Here: fixed schema, self-delimiting container,
compile-time-known size → no in-payload length is the correct, standard choice.

## Wire layout

```
company_id(16=0xFFFF) | magic(16=0xABCD) | version(8) | channel(8) | <enabled fields, canonical order, tightly packed, MSB-first> | zero-pad to byte
```

## Open decisions

*(none — all resolved; see below)*

## Transition

The packed format is **opt-in** via `CONFIG_PROSPECTOR_ADV_PACKED`:

- **off (default)** — legacy fixed `zmk_status_adv_data` struct; existing
  keyboards and the color scanner are byte-for-byte unchanged.
- **on** — bit-packed, field-selectable payload described here. Our keyboard +
  mono scanner opt in.

Both the broadcaster and the scanner core carry both paths, gated on the symbol,
so nothing existing breaks and the color scanner can migrate later. This keeps
the change upstreamable rather than a hard fork of the wire format.

## Decided

- **Version** — build-time constant fingerprint of the wire structure only
  (per-field presence + width + order). Changes iff the layout changes; never on
  module/git/date/unrelated-config changes.
- **Framing** — no per-field lengths, no total length (BLE frames it); bit-pack
  with ignored trailing pad bits.
- **Layer name** — user-configurable length + char width; drives the budget;
  over-budget is a hard build error.
- **Profile fields** — `profile` / `patch_level` / `dev_flag` are three
  independent selectable fields, not one combined `profile_slot`.
- **Bit order** — explicit bit-writer/reader (no C bitfields); **MSB-first**,
  big-endian at the bit level.
- **keyboard_id** — default width unchanged (32 b); add a new
  `_KEYBOARD_ID_BITS` knob (upstream has none) so it can be shrunk when selected.
- **Mismatch UI** (our mono shield) — a full-display **takeover** state shown
  only on version mismatch (a matching-channel ad whose version ≠ ours). Not a
  persistent badge (wastes space) and not the no-keyboard state (misleading).
  Since a mismatch can't be decoded, there's no keyboard data to preserve.
  Wording is **factual, not a call to action**: e.g. `VERSION` / `MISMATCH`
  (two centered lines). No "update"/"upgrade" — that presumes the user's intent.
  Three display states total: no-ad/idle · version-mismatch · valid keyboard.
