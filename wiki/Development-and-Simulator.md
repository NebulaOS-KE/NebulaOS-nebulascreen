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

## Screenshot tour (`GUPPY_SIM_SHOTS`)

The simulator can walk every page and state and save a screenshot of each, with no display
server. Set `GUPPY_SIM_SHOTS=<dir>` and the app runs the tour, writes one binary PPM per step
into that directory, and exits. The tour also suppresses the normal simulator demo data,
prompts and notifications, so the output is clean.

```bash
mkdir -p shots
cd build/bin
SDL_VIDEODRIVER=offscreen GUPPY_SIM_SHOTS=../../shots ./nebulascreen
```

It captures a copy of the LVGL framebuffer rather than a window, which is why
`SDL_VIDEODRIVER=offscreen` is enough and no X or Wayland session is needed.

At `dee0f6f` this produces **129 images** at 480x272 with `GUPPY_SMALL_SCREEN=1` - not just one
per screen, but per *state*: confirmation dialogs, refusals, in-progress, failed, rolled back,
and each step of the guided calibrations. Steps are named (`07-updates`, `m19-calibration`,
`m40-tram-idle`), so the filenames are stable across runs and can be diffed between commits to
find screenshots that have gone stale.

That diffing works in practice. Re-capturing across `94869f7` -> `3194104` -> `dee0f6f`
changed 1 image and then 5, and at `dee0f6f` all five were print-status or print-start
screens - matching that commit's own subject. A capture run is a cheap way to see what a UI
change actually touched.

### Two things to know before relying on it

**It is reproducible, with one exception.** Running the tour twice against the same build gives
128 of 129 byte-identical images. The exception is `07-updates`: the Updates page disables its
buttons while waiting for a reply from Moonraker's update manager, and with no Moonraker there
is no reply, so the capture lands in the busy or the idle state depending on timing. Treat that
one as unusable for comparison - and do not publish it, because it shows buttons greyed out
that are not greyed out on a real printer.

**The NebulaOS settings pages are not truthful here.** The `machine.nebulaos.*` methods only
exist on a real printer, so those panels show errors or empty values. The tour's settings
captures use the simulator's own state rather than live data.

The boot and init screens are outside the tour, so a printer's very first start cannot be
captured this way.

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
