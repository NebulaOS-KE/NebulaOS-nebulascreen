# Integration with NebulaOS

[`NebulaOS-firmware`](https://github.com/coreflake1/NebulaOS-firmware) pins an exact commit of this
repository in `manifests/dependencies.conf`. Its image build fetches that commit, applies the submodule
patches, cross-compiles it with this repository's `scripts/build-release.sh` (the same recipe the
release CI uses) and installs the result read-only under `/opt/nebulascreen`. You do not need to clone
or build this repository to build NebulaOS. Note that the pinned commit is not necessarily this
repository's newest commit.

Every NebulaOS build records the exact commit it used in its build manifest.

What the image provides around the app:

- The service script `/etc/init.d/S58nebulascreen`, started after Klipper and Moonraker.
- The persistent settings file and its bind mounts, plus the default copy in
  `/usr/share/nebulaos/nebulascreen/` (see [Config and theme](Config-and-Theme)).
- The `machine.nebulaos.*` Moonraker methods the Settings pages use, and the Klipper extensions behind
  the guided calibrations, power-loss recovery and TMC autotune.
- `/opt/nebulaos-version.json` with the `nebulaos_ui_api` level; NebulaScreen refuses to start when
  the level does not match its own (currently 1).

A NebulaScreen release zip can also run on top of the image, see
[Releases and deployment](Releases-and-Deployment).

See also: [CI](CI), [Vendored dependencies](Vendored-Dependencies),
[History and credits](History-and-Credits).
