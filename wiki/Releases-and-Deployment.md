# Releases & deployment

*(Developer doc.)* NebulaScreen (based on GuppyScreen) ships in two ways.
Both are built by the **same recipe**, `scripts/build-release.sh`:

1. **Inside the NebulaOS image.** NebulaOS-firmware pins a commit of this repo,
   builds it in its digest-pinned build container and installs it under
   `/opt/nebulascreen` (read-only).
2. **As a stand-alone release.** A tag `vX.Y.Z` makes the CI publish
   exactly one asset, `nebulascreen-vX.Y.Z.zip`, as a normal (stable) GitHub
   release. It is meant for Moonraker's update manager (`type: zip`,
   `channel: stable`), which installs it into `/usr/data/nebulaos/apps/nebulascreen`;
   `S58nebulascreen` then binds it over the image's files at start. The
   NebulaOS update supervisor checks it before use and rolls it back if it
   fails. After a new system image is installed, the image's own NebulaScreen
   wins. The image's default `moonraker.conf` does not yet contain an update
   manager entry for NebulaScreen, so this path is not active out of the box.

## The release zip

| Entry | What |
|---|---|
| `nebulascreen`, `nebulabeep` | static ELF32 little-endian MIPS executables (mode 0755) |
| `themes/` | themes |
| `release_info.json` | `project_name` NebulaOS-nebulascreen, `project_owner` NebulaOS-KE, `version` (= tag = release name), `nebulaos_ui_api` |
| `LICENSE`, `NOTICE` | GPL-3; based on GuppyScreen by ballaswag |

Never in the zip: symlinks or the user's config (`nebulascreen.json`, or the
old `guppyconfig.json`). The zip
is deterministic: sorted entries, fixed modes, and every timestamp is the
commit date. `scripts/nebulascreen_release.py check <zip>` validates a zip.

`nebulaos_ui_api` is the path/name contract with the image (see
`src/nebulascreen_paths.h`). A release only runs on an image that provides
the same level. The app itself refuses to start on a mismatch.

## Building locally

Inside the NebulaOS build container (toolchain on `PATH`):

```
scripts/build-release.sh v0.9.0
python3 tests/test_nebulascreen_release.py      # host tests of the packager
make test                                        # host unit tests
```

## CI

- `.github/workflows/build.yml`:
  - a push to main builds and checks the zip, without publishing;
  - a tag `vX.Y.Z` builds, checks and publishes the release.
- `.github/workflows/pull_request.yml`: the same build for pull requests.

There is no nightly or pre-release channel.
