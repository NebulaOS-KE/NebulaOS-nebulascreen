# NebulaScreen

NebulaScreen is NebulaOS's touchscreen UI — a KE-focused fork of
[GuppyScreen](https://github.com/ballaswag/guppyscreen). Full print control, an interactive 3D bed
mesh, an on-screen calibration suite, running right on the printer's display with no X11, Wayland,
or display server involved.

It shares history with [OpenKE](https://github.com/coreflake1/guppyscreen), but they're separate
projects now — OpenKE has its own installer and its own releases for stock Creality firmware. If
you followed a link expecting that, `coreflake1/guppyscreen` is the repo you want; this one is the
NebulaOS component.

## Features

- 🖨️ **Print control & status** — temps, fans, LED, movement/homing, file browser (incl. USB sticks), Spoolman
- 🟦 **Interactive 3D bed mesh** — rotate / zoom / pan colour height map (plus a table view)
- 🎯 **Guided Calibration hub** — a single numbered menu: Axis Twist, a combined Z-offset + bed mesh
  Recalibration Wizard (talks to NebulaOS-klipper's `z_compensate` status), Input Shaper, E-Steps
  Calibration, Skew Correction, TMC Autotune
- 🎚️ **Fine-tune mid-print** — speed, flow, Z-offset, pressure advance (firmware retraction is its own panel)
- 📷 **Camera** — persistent image tuning (contrast/saturation)
- 🔔 **Buzzer beeps & songs** — real-pitch `M300`, `PLAY_TUNE` jingles (editable `songs.conf`), soft touchscreen click
- 🔒 **Print-state safety locks** — anything that could ruin a running job is blocked or asks first
- 📐 Tuned **480×272** layout

## Building

Two different workflows depending on what you're doing.

**If you're just trying to build the whole OS** — use
[`NebulaOS-firmware`](https://github.com/coreflake1/NebulaOS-firmware) instead. It pins an exact
commit of this repo and cross-compiles + installs it automatically as part of the full image. You
don't need to clone this repo directly for that.

**If you're actually developing NebulaScreen itself:**

```bash
git clone --recurse-submodules https://github.com/coreflake1/NebulaOS-guppyscreen.git
cd NebulaOS-guppyscreen
```

Submodules: `lvgl` (LVGL v8), `lv_drivers`, `libhv`, `spdlog`. `wpa_supplicant` is vendored in-tree.
Full prerequisites, the SDL simulator target, and the MIPS cross-build are covered in
**[Building from Source](wiki/Building-from-Source.md)**.

Offline logic tests (no cross-compile, no LVGL/SDL2 needed) run with `make test`.

## Compatibility

| | |
|---|---|
| **Printer** | Creality Ender-3 V3 KE, running NebulaOS |
| **SoC / arch** | Ingenic XBurst2 X2000 — MIPS (mipsel) |
| **Display** | 480×272 |

## Config and theme now actually persist

Commit `b15ad7f` fixed a real bug where `Config::init()`/`ThemeConfig::init()` silently fell back to
in-memory defaults on every boot on NebulaOS's read-only-squashfs setup, instead of reading your
actual saved `config.json`/`theme.json`. No crash, just settings that quietly never stuck. We've
since tested this on real hardware — config and theme survive a real flash now. See
[`NebulaOS-firmware`'s `manifests/dependencies.conf`](https://github.com/coreflake1/NebulaOS-firmware/blob/main/manifests/dependencies.conf)
for the pin history.

## Documentation

Developer and user docs live in [`wiki/`](wiki/) and [`DEVELOPMENT.md`](DEVELOPMENT.md). The wiki
describes NebulaScreen as it ships inside NebulaOS; for building and installing NebulaOS itself, start
at [`NebulaOS-firmware`](https://github.com/coreflake1/NebulaOS-firmware).

## License & credits

**GPL-3.0** — see [LICENSE](./LICENSE). Builds on
[ballaswag/guppyscreen](https://github.com/ballaswag/guppyscreen),
[probielodan/guppyscreen](https://github.com/probielodan/guppyscreen), and
[pellcorp/grumpyscreen](https://github.com/pellcorp/grumpyscreen), with the 3D bed mesh from
[prestonbrown/guppyscreen](https://github.com/prestonbrown/guppyscreen). Vendored Klipper mods keep
their own upstream licenses and credits.
