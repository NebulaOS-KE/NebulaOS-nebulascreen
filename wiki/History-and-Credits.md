# History and credits

NebulaScreen is the touchscreen app of NebulaOS. It is based on
[GuppyScreen](https://github.com/ballaswag/guppyscreen) by ballaswag, licensed GPL-3, and stays under
the same licence (see `LICENSE` and `NOTICE` in the repository root).

## Lineage

```
ballaswag/guppyscreen            original GuppyScreen
  └── probielodan/guppyscreen    KE-focused UI rework
        └── coreflake1/guppyscreen (OpenKE, earlier "GuppyKE")
              └── NebulaOS-guppyscreen → NebulaScreen (renamed October 2026)
```

Along the way it took fixes and improvements from
[pellcorp/grumpyscreen](https://github.com/pellcorp/grumpyscreen) and the 3D bed mesh from
[prestonbrown/guppyscreen](https://github.com/prestonbrown/guppyscreen). See
[Contributing](Contributing#credits--attribution) for the full credit list.

## OpenKE

[OpenKE](https://github.com/coreflake1/guppyscreen) is a separate project with its own SSH installer
and releases for **stock Creality firmware**. It shares history with this repository but is not part
of NebulaOS. This wiki used to describe OpenKE's installer, upgrade and uninstall flows; those pages
were rewritten for NebulaOS or reduced to short notes. If you want OpenKE, use that repository.

## Names kept on purpose

The rename changed every device path and file name (see `src/nebulascreen_paths.h`). A few internal
names keep the upstream spelling:

- C++ identifiers such as the `GuppyScreen` class (`src/guppyscreen.cpp`) and the `GUPPY_*` build flags
  (`GUPPY_SMALL_SCREEN`, `GUPPY_ROTATE`, ...).
- The Moonraker database namespace `guppyscreen`, which holds macro and console favorites. It is shared
  by both boot slots and by Mainsail; renaming it would lose the favorites.
- The buzzer source `k1/k1_mods/buzzer/guppybeep.c` (built as `nebulabeep`), and the Klipper shell
  command alias `guppybeep` in NebulaOS's `beeper.cfg`.
- `GUPPYSCREEN_VERSION` is still accepted by the `Makefile` as a fallback for `NEBULASCREEN_VERSION`.

See also: [Integration with NebulaOS](Integration-with-NebulaOS),
[Vendored dependencies](Vendored-Dependencies).
