# Architecture

NebulaScreen is a single C++17 program on LVGL 8. It is based on GuppyScreen by ballaswag (GPL-3);
see [History and credits](History-and-Credits). Some internal names keep the upstream spelling (for
example the `GuppyScreen` class in `src/guppyscreen.cpp`).

## Components

- **UI** — LVGL **v8** widgets. Each screen is a `*_panel.cpp` in `src/` (e.g. `print_status_panel`,
  `bedmesh_panel`, `calibration_menu_panel`, `finetune_panel`). `src/main.cpp` sets up the display and
  input drivers and reads the config; `src/guppyscreen.cpp` sets up logging and the theme and wires
  the app together. The Settings pages share helpers in `src/settings_ui.cpp`.
- **Display / input** — `lv_drivers` (submodule, `coreflake1/lv_drivers`): framebuffer and evdev on
  the printer, SDL2 in the simulator.
- **Moonraker** — `libhv` provides the websocket client (`src/websocket_client.cpp`, JSON-RPC).
  `src/notify_consumer.cpp` dispatches Moonraker notifications to the panels; `src/state.cpp` holds the
  merged printer state.
- **NebulaOS contract** — everything that touches the operating system goes through Moonraker
  JSON-RPC methods provided by NebulaOS (`machine.nebulaos.*`: Wi-Fi, network, display, image, reset,
  boot logo, ...). The app does not run system commands for these itself. The logic for each is in
  plain, LVGL-free files that the host tests cover: `src/nebulaos_*.cpp`, `src/plr_status.cpp`.
- **Klipper workflows** — guided calibrations, power-loss recovery and TMC autotune call commands from
  NebulaOS's Klipper extensions (`NEBULAOS_*`, object `nebulaos_calibration`,
  `nebulaos_power_loss_recovery`).
- **Paths and image contract** — `src/nebulascreen_paths.h` holds every device path and name
  (`/opt/nebulascreen/...`, `S58nebulascreen`, the log path) and the `nebulaos_ui_api` level (1) this
  build needs. The app reads `/opt/nebulaos-version.json` and refuses to run on an image that states a
  different level. Once connected it reports its version with `machine.nebulaos.screen.report`.
- **Config** — `src/config.cpp` reads and saves `nebulascreen.json` (power-cut safe, via a journal
  file); `src/theme.cpp` reads the theme JSON. See [Configuration](Configuration).
- **Logging** — `spdlog`, rotating file (10 MB × 3).
- **Print-state safety** — `src/utils.cpp`: `is_printing()`, `notify_locked()`,
  `confirm_if_printing()`.
- **Buzzer** — owned by Klipper's `nebulaos_beeper` extra (kernel PWM channel); the touch click is a
  request to it over the Klippy API socket (`src/touch_beep_client.*`). `k1/k1_mods/buzzer/guppybeep.c`
  builds `nebulabeep`, now only a compatibility client that forwards to Klipper.
- **Packaging** — both programs are **statically linked** (musl on MIPS), so there are no shared-library
  dependencies on the printer.

## Print-state safety model

Panels are gated by print state (`print_stats/state` is `printing` or `paused`):

- **Blocked with a message** mid-print: homing, extrude, the calibration rows, bed mesh changes, TMC
  autotune, clean reset and slot swap.
- **Confirm first** mid-print: console commands, macros, Klipper/MCU restart, limits, power.
- **Always available**: Fine Tune, Z offset, fans, LED, print status, file browsing.

## Repository layout

```
.
├── src/                  Application source (panels, NebulaOS contract logic, config, networking)
├── tests/                Host tests (make test, tests/test_nebulascreen_release.py)
├── assets/               Compiled fonts and icon sets (material, material_46, svg sources)
├── themes/               Colour theme JSON files
├── lvgl/                 LVGL v8 (submodule; patched)
├── lv_drivers/           Display/input drivers (submodule)
├── libhv/                Network library (submodule)
├── spdlog/               Logging library (submodule; patched)
├── lv_touch_calibration/ Touch calibration
├── patches/              Patches applied to submodules before building
├── k1/k1_mods/buzzer/    Buzzer player source (nebulabeep)
├── scripts/
│   ├── build-release.sh       The release build (CI and NebulaOS-firmware use it)
│   ├── build-mips.sh          MIPS cross-build, called by build-release.sh
│   ├── nebulascreen_release.py  Packages and checks the release zip
│   └── build-*-mipsel.sh, vendor/, lib/   Inherited vendoring scripts, see Vendored-Dependencies
├── docker/               Standalone toolchain image and k1-bash-build reconstruction
├── docs/                 Developer notes (partly historical)
├── wiki/                 This wiki
├── lv_conf.h, lv_drv_conf.h   LVGL configuration
├── Makefile              Build entry point
└── .github/workflows/    build.yml, pull_request.yml, wiki.yml
```

## On the printer

| Path | Purpose |
|---|---|
| `/opt/nebulascreen/` | `nebulascreen`, `nebulabeep`, `themes/` (read-only image) |
| `/opt/nebulascreen/nebulascreen.json` | Config, bind-mounted from `/usr/data/nebulaos/nebulascreen/nebulascreen.json` |
| `/opt/printer_data/logs/nebulascreen.log` | Log |
| `/etc/init.d/S58nebulascreen` | Service script (`start-stop-daemon`) |
| `/usr/data/nebulaos/apps/nebulascreen` | A downloaded release, bound over the image's files at start (see [Releases and deployment](Releases-and-Deployment)) |
