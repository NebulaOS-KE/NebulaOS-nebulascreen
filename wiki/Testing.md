# Testing

Host tests — no cross-compile, no LVGL/SDL2 — run with:

```sh
make test
python3 tests/test_nebulascreen_release.py
```

`make test` builds and runs these targets (each a `tests/test_*.cpp` with the small framework in
`tests/minitest.h`):

| Target | Covers |
|---|---|
| `test-nebulascreen-paths` | Device paths, legacy (GuppyScreen-era) config mapping, `nebulaos_ui_api` check |
| `test-config-theme-parse-safety` | `Config::init()` / `ThemeConfig::init()` fall back to defaults on an unreadable or broken file, and the power-cut-safe save |
| `test-subscription-baseline-ordering` | Moonraker subscription baseline ordering in `State` |
| `test-plr-status` | Power-loss recovery panel states and commands |
| `test-nebulaos-display` | Brightness levels and display status |
| `test-nebulaos-network` | Static IP logic |
| `test-nebulaos-macros` | Mapping of screen actions to NebulaOS macros |
| `test-nebulaos-calibration-flow` | Guided calibration screens |
| `test-nebulaos-system` | Power & System and Logs logic |
| `test-nebulaos-settings` | Connections, Wi-Fi and Updates logic |
| `test-nebulaos-image` | Settings → Updates → System image logic |

`tests/test_nebulascreen_release.py` tests the release packager (`scripts/nebulascreen_release.py`).

For visual work, use the SDL simulator ([Development and simulator](Development-and-Simulator)).
Hardware testing on a real printer is still the final check before a NebulaOS build ships.
