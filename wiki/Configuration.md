# Configuration

## Runtime configuration: `nebulascreen.json`

NebulaScreen reads `nebulascreen.json` from the directory of its own executable. On a NebulaOS printer
that is `/opt/nebulascreen/nebulascreen.json`, a bind mount of the persistent file
`/usr/data/nebulaos/nebulascreen/nebulascreen.json`. The same file is visible in Mainsail as
`config/nebulascreen/nebulascreen.json`, so you can edit it in the browser. Restart the screen
afterwards (**Settings → System → Restart GUI**).

The image's default copy is `/usr/share/nebulaos/nebulascreen/nebulascreen.default.json`
(NebulaOS-firmware ships it); **Settings → Reset → Reset GUI settings** copies it over your file.

If the file is missing or is not valid JSON, NebulaScreen starts with built-in defaults. Saves are
power-cut safe: a complete copy is written to `nebulascreen.json.saving` next to the persistent file
first, and an interrupted save is finished or dropped at the next start.

Most settings are changed on the screen (Settings → Display / Printer / Reset); the file is for the
rest.

### Top-level fields

| Field | Meaning |
|---|---|
| `default_printer` | Key of the active entry in `printers` |
| `printers` | Named printer (Moonraker) configurations, see below |
| `display_rotate` | Framebuffer rotation. **`2` is the KE value** (see below) |
| `display_sleep_sec` | Screen sleep after this many seconds (`-1` = never); set by Settings → Display → Sleep after |
| `theme` | Colour theme name (`blue`, `red`, `green`, `purple`, `pink`, `yellow`); file `themes/<name>.json` next to the executable |
| `touch_beep` | Click on every tap |
| `default_extruder_temp` | Target used by Load/Unload when none is set |
| `prompt_emergency_stop` | Ask before an emergency stop (default `true`) |
| `invert_y_direction`, `invert_z_direction` | Swap the Y / Z jog arrows |
| `touch_calibrated`, `touch_calibration_coeff` | Touch calibration, see below |
| `log_path` | Log file (`/opt/printer_data/logs/nebulascreen.log` on NebulaOS) |
| `thumbnail_path` | Directory for cached G-code thumbnails |
| `nebulascreen_init_script` | The screen's service script (`/etc/init.d/S58nebulascreen`) |

### Per-printer fields (`printers.<name>`)

| Field | Meaning |
|---|---|
| `moonraker_host` / `moonraker_port` | Moonraker address (default `127.0.0.1:7125`) |
| `moonraker_api_key` | API key string, or `false` if not required |
| `monitored_sensors` | Temperatures shown on the Home tab. Each entry has `id` (Klipper object), `display_name`, `controllable`, `color` |
| `fans` | Fans shown as editable sliders on the Fans panel. Each entry has `id` and `display_name`. Empty: the part-cooling fan, generic fans and PWM fan pins are editable |
| `default_macros` | Macro names for `cooldown`, `load_filament`, `unload_filament` |
| `log_level` | spdlog level (`info`, `warn`, `debug`); set by Settings → System → Report a problem |

### Default monitored sensors

The NebulaOS default shows **Extruder**, **Bed** and **MCU Temp** (`temperature_sensor mcu_temp`).
NebulaScreen puts these three back whenever `monitored_sensors` is missing or empty. If none of the
listed `id`s exists in Klipper, every heater and sensor is shown under its Klipper name instead.

### Settings from the GuppyScreen era

A file copied from a GuppyScreen-era image still has the old default paths
(`/opt/guppyscreen/...`, `.../guppyscreen.log`, `/etc/init.d/S58guppyscreen`) and the key
`guppy_init_script`. NebulaScreen maps exactly those old values to the new ones when it starts and
writes them on the next save; values you set yourself are left alone.

Macro and console favorites are not in this file: they are in Moonraker's database, namespace
`guppyscreen` (kept on purpose, so favorites survive the rename).

### Display rotation

The KE screen is mounted upside down and the kernel display driver rotates the framebuffer 180°, so
NebulaScreen also rotates 180° for the two to cancel out:

- `display_rotate: 0` → upside down
- `display_rotate: 1` → 90° off
- `display_rotate: 2` → **correct** (KE default)

> **Changed `display_rotate`? Redo touch calibration.** The coefficients are rotation-specific. Use
> **Settings → Reset → Reset touch calibration**.

### Touch calibration

The resistive panel works without calibration (raw mode), but taps can be 10–20 px off near the edges.
A one-time calibration fixes that.

**Running it:** **Settings → Reset → Reset touch calibration**. The screen restarts and shows three
crosshairs one at a time; tap each one three times (lift your finger between taps). The taps are
averaged, the result is saved, and the screen restarts with it.

| `touch_calibrated` | `touch_calibration_coeff` | Effect |
|---|---|---|
| `false` or absent | — | Raw mode, no correction |
| `true` | present | Calibration active |
| `true` | absent | Calibration wizard shown at the next start |

To get back to raw mode or to force the wizard without the screen, edit
`config/nebulascreen/nebulascreen.json` in Mainsail accordingly and restart the screen
(`/etc/init.d/S58nebulascreen restart` over SSH).

## Build-time options

Read by the `Makefile` (and `scripts/build-mips.sh` / `scripts/build-release.sh`), not at runtime:

| Variable | Effect |
|---|---|
| `CROSS_COMPILE` | Toolchain prefix (e.g. `mipsel-buildroot-linux-musl-`). **Unset** = SDL simulator build (`SIMULATOR` define, links `-lSDL2`) |
| `GUPPY_SMALL_SCREEN` | 480×272 layout and the `material_46` icon set (always set for the KE) |
| `GUPPY_ROTATE` | Changes the built-in `display_rotate` default from `0` to `3` (270°) |
| `NEBULASCREEN_VERSION` | Version string shown in the System panel (`GUPPYSCREEN_VERSION` is still accepted) |
| `EVDEV_CALIBRATE` | Enables touch calibration (set by `scripts/build-mips.sh`) |

The `GUPPY_*` names are the upstream GuppyScreen build flags and are kept as they are. The build
scripts also pass `GUPPY_THEME` (from `NEBULASCREEN_THEME`, default `blue`), but the current
`Makefile` and source do not read it; the theme is chosen at runtime.

## Themes

Colour themes are JSON files in `themes/` (`blue`, `green`, `pink`, `purple`, `red`, `yellow`), shipped
next to the executable. The release zip contains them too.
