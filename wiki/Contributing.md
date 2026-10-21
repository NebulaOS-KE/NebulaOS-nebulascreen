# Contributing

Issues and pull requests are welcome. NebulaScreen is part of NebulaOS; bugs seen on a printer are
best reported from the screen (**Settings → System → Report a problem**), which saves the logs to a
USB stick and points to [github.com/coreflake1/NebulaOS/issues](https://github.com/coreflake1/NebulaOS/issues).

## Workflow

1. Develop and test against the **[simulator](Development-and-Simulator)** first, then verify on-device
   where relevant.
2. Keep changes consistent with the existing panel structure and LVGL v8 idioms in `src/` (each screen
   is a `*_panel.cpp`; shared helpers live in `src/utils.cpp`).
3. Build the targets your change affects before opening a PR — at minimum the simulator, and the MIPS
   build for anything that touches on-device behavior or layout.
4. For small-screen work, build with `GUPPY_SMALL_SCREEN=1` and check the 480×272 layout.
5. Run the host tests: `make test` and `python3 tests/test_nebulascreen_release.py` (see [Testing](Testing)).

Issue templates are in `.github/ISSUE_TEMPLATE/`. CI builds and tests every pull request
(`.github/workflows/pull_request.yml`), see [CI](CI).

## License

NebulaScreen is licensed under the **GNU General Public License v3.0** (see `LICENSE` and `NOTICE` at
the repository root). It is based on GuppyScreen by ballaswag, also GPL-3. Contributions are accepted
under the same license.

## Credits & attribution

NebulaScreen builds on the work of:

- [ballaswag/guppyscreen](https://github.com/ballaswag/guppyscreen) — the original GuppyScreen, which
  NebulaScreen is based on
- [probielodan/guppyscreen](https://github.com/probielodan/guppyscreen) — KE-focused UI rework
- [pellcorp/grumpyscreen](https://github.com/pellcorp/grumpyscreen) — bug fixes and improvements
- [prestonbrown/guppyscreen](https://github.com/prestonbrown/guppyscreen) — 3D bed mesh
- [coreflake1/guppyscreen](https://github.com/coreflake1/guppyscreen) (OpenKE) — the KE fork this
  repository grew out of, see [History and credits](History-and-Credits)
- Libraries: [LVGL](https://github.com/lvgl/lvgl), [lv_drivers](https://github.com/lvgl/lv_drivers),
  [libhv](https://github.com/ithewei/libhv), [spdlog](https://github.com/gabime/spdlog),
  `lv_touch_calibration` — each under its own licence
- [Moonraker](https://github.com/Arksine/moonraker) and [Klipper](https://github.com/Klipper3d/klipper),
  which the app talks to
- Icons: [Material Design Icons](https://pictogrammers.com/library/mdi/)

The Klipper macros and add-ons the guides describe (purge and park, TMC autotune, buzzer macros) are
part of NebulaOS-firmware and NebulaOS-klipper-extensions and carry their own credits there.

Upstream GuppyScreen documentation: <https://ballaswag.github.io/docs/guppyscreen/configuration/>
(written for GuppyScreen; paths and file names differ in NebulaScreen).
