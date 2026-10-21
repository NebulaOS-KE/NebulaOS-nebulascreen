# Development and simulator

The SDL simulator runs NebulaScreen on a desktop, without a printer. It is the quickest way to work
on the UI.

## Run the simulator

1. Build it (see [Building from source](Building-from-Source) → simulator build). Add
   `GUPPY_SMALL_SCREEN=1` for the KE's 480×272 layout; without it the window is 800×480.
2. NebulaScreen reads `nebulascreen.json` from the directory of its executable, so create
   `build/bin/nebulascreen.json`. Point it at a reachable Moonraker, and set `log_path` and
   `thumbnail_path` to directories that exist:
   ```json
   {
     "log_path": "/tmp/nebulascreen/nebulascreen.log",
     "thumbnail_path": "/tmp/nebulascreen/thumbnails",
     "default_printer": "sim",
     "printers": { "sim": { "moonraker_host": "127.0.0.1", "moonraker_port": 7125,
                            "moonraker_api_key": false } }
   }
   ```
3. Copy the themes next to it (`cp -r themes build/bin/`) and run:
   ```bash
   ./build/bin/nebulascreen
   ```

> The program **crashes at start if the directory in `log_path` does not exist**
> (`spdlog_ex: Failed opening file ... for writing`). Create it first. Without a config file the
> built-in default log path is `/opt/printer_data/logs/nebulascreen.log`.

## Notes

- Without Moonraker, the websocket reconnects about every 2 s — expected.
- The `machine.nebulaos.*` methods only exist on a NebulaOS printer; against another Moonraker the
  NebulaOS Settings pages show errors or empty values.
- Simulator builds define `SIMULATOR`; some panels have simulator-only demo data and screenshot helpers
  under that define.

## No printer? Use a virtual one

[virtual-klipper-printer](https://github.com/mainsail-crew/virtual-klipper-printer) runs Klipper and
Moonraker in Docker. Set `moonraker_host` to `127.0.0.1` and `moonraker_port` to `7125` in
`build/bin/nebulascreen.json`.

## Iterating

After a full build, edit files under `src/` and re-run `make` with the same flags; only changed files
are recompiled. Each screen is a `*_panel.cpp`; shared helpers (print-state checks, toasts,
confirmations) live in `src/utils.cpp` and `src/settings_ui.cpp`. Logic that does not need LVGL goes
into a plain `src/nebulaos_*.cpp` file with a host test ([Testing](Testing)).

## Further reading

- `DEVELOPMENT.md` — upstream GuppyScreen development notes (toolchains, environment variables).
- `docs/dev-notes.md` — notes from the original KE port (hardware facts, display orientation, font and
  layout tuning). A historical snapshot: the install and packaging parts describe the old OpenKE
  installer.
