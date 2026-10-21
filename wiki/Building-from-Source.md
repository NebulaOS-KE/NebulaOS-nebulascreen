# Building from source

You only need this to work on NebulaScreen itself. A NebulaOS image build compiles it automatically
([Integration with NebulaOS](Integration-with-NebulaOS)).

NebulaScreen uses a plain `Makefile`. There are two practical builds: the **SDL simulator** (desktop)
and the **MIPS** build for the Ender-3 V3 KE. The MIPS build is always done by
`scripts/build-release.sh`.

## Prerequisites

- A C++17 toolchain (GCC/G++ 7.2+), `make`, `cmake`, `git`
- For the simulator: SDL2 development headers
- For the MIPS build: a `mipsel` musl cross toolchain — normally the NebulaOS build container
- `python3` (release packaging and its tests)

## 1. Clone with submodules

```bash
git clone --recurse-submodules https://github.com/coreflake1/NebulaOS-guppyscreen.git
cd NebulaOS-guppyscreen
```

Submodules: `lvgl` (LVGL **v8**), `lv_drivers`, `libhv`, `spdlog`.

## 2. Apply the submodule patches

```bash
(cd spdlog && git apply ../patches/0002-spdlog_fmt_initializer_list.patch)
(cd lvgl   && git apply ../patches/0003-lvgl-dpi-text-scale.patch)
```

| Patch | Purpose |
|---|---|
| `0002-spdlog_fmt_initializer_list.patch` | spdlog/fmt compatibility fix |
| `0003-lvgl-dpi-text-scale.patch` | DPI text-scaling adjustment |

`scripts/build-release.sh` applies both itself (only if not applied yet).

## 3. Build the bundled libraries (host)

```bash
make libhv.a
make libspdlog.a
```

(`make wpaclient` still exists but does nothing; Wi-Fi goes through NebulaOS.)

## 4a. Simulator build (desktop)

Install SDL2 headers, then build without `CROSS_COMPILE`:

```bash
# Fedora / Nobara:   sudo dnf install sdl2-compat-devel
# Debian / Ubuntu:   sudo apt install libsdl2-dev
# Arch:              sudo pacman -S sdl2

make -j"$(nproc)" GUPPY_SMALL_SCREEN=1 \
  CFLAGS="-O3 -g0 -MD -MP -I./ -I./lvgl/ -I/usr/include/SDL2 -D_GNU_SOURCE=1 -Wno-incompatible-pointer-types" \
  LDFLAGS="-lm -Llibhv/lib -Lspdlog/build -l:libhv.a -latomic -lpthread -lstdc++fs -l:libspdlog.a"
```

The `LDFLAGS` override drops the Makefile's default `-static`, which a desktop SDL2 build cannot use.
`-Wno-incompatible-pointer-types` is needed on GCC 14+ for `lv_touch_calibration`.
`GUPPY_SMALL_SCREEN=1` gives the KE's 480×272 layout.

Output: `build/bin/nebulascreen`. See [Development and simulator](Development-and-Simulator) to run it.

## 4b. MIPS build for the KE (the release build)

```bash
scripts/build-release.sh v0.9.0
```

This is the one build recipe: this repository's CI and NebulaOS-firmware's image build both use it.
Run it inside the NebulaOS build container (`ghcr.io/coreflake1/nebulaos-build`, pinned by digest in
NebulaOS-firmware's `manifests/dependencies.conf`), with the toolchain on `PATH`. CI uses
`CROSS_COMPILE=mipsel-buildroot-linux-musl-` from Bootlin `mips32el--musl--stable-2024.02-1`.

What it does:

1. Applies the two submodule patches if needed.
2. Builds the host `libhv.a` / `libspdlog.a`, then runs `scripts/build-mips.sh`, which rebuilds libhv
   and spdlog for MIPS, builds NebulaScreen with `GUPPY_SMALL_SCREEN=1` and `EVDEV_CALIBRATE=1`, builds
   `nebulabeep` from `k1/k1_mods/buzzer/guppybeep.c`, and restores the host libraries.
3. Strips both programs and checks that the binary embeds the commit date (`SOURCE_DATE_EPOCH`
   defaults to the commit time of `HEAD`, so the build does not depend on the wall clock).
4. Packages and checks `dist/nebulascreen-<version>.zip` with `scripts/nebulascreen_release.py`.

Environment: `CROSS_COMPILE` (default `mipsel-linux-`), `NEBULASCREEN_THEME` (default `blue`),
`SOURCE_DATE_EPOCH`.

Output: `build/bin/nebulascreen` and `build/bin/nebulabeep` (static ELF32 little-endian MIPS) and the
release zip. See [Releases and deployment](Releases-and-Deployment).

### Without the NebulaOS build container

`docker/Dockerfile` defines a standalone toolchain image with the same Bootlin MIPS toolchain (plus an
aarch64 one), the replacement for upstream `ballaswag/guppydev`. Neither CI nor NebulaOS-firmware uses
it; if you build with it, the result is not the qualified NebulaOS build environment.

## Build internals

- `make build` runs the full sequence: clean and rebuild `libhv.a`, `libspdlog.a`, then the app.
- `make clean` removes `build/`.
- Without `CROSS_COMPILE` the Makefile adds `-DSIMULATOR` and links `-lSDL2`.
- `GUPPY_SMALL_SCREEN` selects the `material_46` icon set and the small-screen layout.
- `NEBULASCREEN_VERSION` sets the version string (`GUPPYSCREEN_VERSION` is still accepted).
- `make test` runs the host tests ([Testing](Testing)).
