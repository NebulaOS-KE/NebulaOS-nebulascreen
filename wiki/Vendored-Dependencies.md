# Vendored dependencies

Two different things in this repository are called "vendored".

## The app's own dependencies

`lvgl` (LVGL v8), `lv_drivers` (from `coreflake1/lv_drivers`), `libhv` and `spdlog` are git submodules,
fetched with `git clone --recurse-submodules`. Two patches in `patches/` are applied before building
(`0002-spdlog_fmt_initializer_list.patch`, `0003-lvgl-dpi-text-scale.patch`);
`scripts/build-release.sh` applies them itself when needed. `lv_touch_calibration/` is in the tree.
Every build uses these, in CI and in NebulaOS-firmware.

There is no `wpa_supplicant` in the tree any more: Wi-Fi goes through NebulaOS (`machine.nebulaos.wifi.*`),
and `make wpaclient` is kept only as a no-op for older build scripts.

## Inherited OpenKE-era build scripts

The repository still contains tooling from its OpenKE history (see
[History and credits](History-and-Credits)):

- `scripts/build-{nginx,pillow,streaming-form-data,curl,ft2font,ax88179}-mipsel.sh` and their sources
  and outputs under `scripts/vendor/` — rebuilds of target-side packages and kernel modules for the
  stock Creality system.
- `docker/k1-bash-build/` — a reconstruction of Pellcorp's `k1-bash-build` toolchain image those scripts
  use (credit and details in [`docker/README.md`](https://github.com/coreflake1/NebulaOS-guppyscreen/blob/main/docker/README.md)).
- `docker/Dockerfile` — a standalone toolchain image (Bootlin mips32el musl and an aarch64 toolchain),
  the replacement for upstream `ballaswag/guppydev`.
- `docs/VENDORING.md` — describes the Klipper-mod vendoring of the old installer. That machinery was
  deleted from this repository; the document is kept for reference only.

**Neither CI nor the NebulaOS build uses any of these.** The Klipper add-ons NebulaOS ships (autotune,
calibration workflows, power-loss recovery, ...) live in NebulaOS-klipper-extensions and
NebulaOS-firmware, not here.

See also: [CI](CI), [Building from source](Building-from-Source).
