# CI

Three workflows in `.github/workflows/`:

| Workflow | Runs on | What it does |
|---|---|---|
| `build.yml` | push to `main`, tags `v*`, manual | Host tests, then `scripts/build-release.sh` and the zip check. A push to `main` builds `v0.0.0-main.<sha>` and uploads it as a workflow artifact; a tag `vX.Y.Z` also publishes `nebulascreen-vX.Y.Z.zip` as a normal (stable) GitHub release. |
| `pull_request.yml` | pull requests to `main` | The same build and tests, without publishing. |
| `wiki.yml` | push to `main` that touches `wiki/**` | Copies `wiki/` to the GitHub Wiki, see [Wiki publishing](Wiki-Publishing). |

The build jobs run inside the digest-pinned NebulaOS build image (`ghcr.io/coreflake1/nebulaos-build`),
the same one NebulaOS-firmware builds with, using the Bootlin `mips32el--musl--stable-2024.02-1`
toolchain (`CROSS_COMPILE=mipsel-buildroot-linux-musl-`). Two details are set in the workflow:

- `HOME=/tmp`, because the container user has no home directory.
- The toolchain directory is added to `PATH` by the workflow; the image keeps it off the global `PATH`
  on purpose.

The host tests in CI are `make test-nebulascreen-paths` and `python3 tests/test_nebulascreen_release.py`.
The full `make test` set runs locally, see [Testing](Testing).

There is no nightly or pre-release channel.
