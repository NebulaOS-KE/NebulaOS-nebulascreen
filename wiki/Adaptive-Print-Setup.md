# Adaptive meshing and purge line

NebulaOS prepares the bed for each print in its `START_PRINT` macro:

- **Adaptive bed mesh** — probes only the area the print covers instead of the whole bed, using
  Klipper's own `BED_MESH_CALIBRATE ADAPTIVE=1`.
- **Smart park** — parks the nozzle next to the print while it heats.
- **Line purge** — draws a purge line just in front of the print area.

All three use the print's object outlines (Klipper `[exclude_object]`), so the slicer must write
object labels. These macros are part of the NebulaOS image (`/etc/nebulaos/klipper/print.cfg`), not
of NebulaScreen; this page describes how to use them.

> **Do [Axis Twist Compensation](Axis-Twist-Compensation) first** if the first layer is uneven left to
> right. A denser mesh does not help when the probe readings are off.

---

## Step 1 — Turn on object labels in the slicer

In **OrcaSlicer**: print settings → **Others** → tick **Label Objects**. Other slicers have a similar
option. NebulaOS's `moonraker.conf` already has `enable_object_processing: True`, so Moonraker passes
the outlines to Klipper.

The same setting enables **Exclude Object** on the print status screen: if one part fails, you can drop
it and let the rest keep printing.

## Step 2 — Call `START_PRINT` from the start G-code

In OrcaSlicer: printer profile → **Machine G-code** → **Machine start G-code**:

```gcode
START_PRINT EXTRUDER_TEMP=[nozzle_temperature_initial_layer] BED_TEMP=[bed_temperature_initial_layer_single]
```

`START_PRINT` then does, in order:

1. Checks the filament sensor; with no filament it stops with a message.
2. Starts heating the nozzle to 150 °C (below oozing) and the bed to `BED_TEMP`.
3. Homes if needed, moves to Z 50 while the bed heats, and waits for the bed.
4. Bed warp stabilisation: an extra wait that depends on how far the bed had to heat (on by default).
5. Clears the old mesh and probes a new one — adaptive, unless the print covers more than 75 % of the
   bed, in which case it probes the full bed.
6. Parks next to the print, waits for the nozzle to reach `EXTRUDER_TEMP`, and purges a line in front
   of the print (or along its left side when there is no room in front).

While it runs, speed and acceleration are capped for probing and restored afterwards.

If the parameters are left out, `BED_TEMP` defaults to 65 and `EXTRUDER_TEMP` to 230 — always pass
both from the slicer.

### Options

| Parameter | Effect |
|---|---|
| `BED_MESH_ADAPTIVE=0` | Probe the full bed instead of the print area |
| `BED_MESH_PROFILE=<name>` | Load a saved mesh instead of probing |

Remove any `BED_MESH_PROFILE LOAD=...` line from older start G-code: loaded after `START_PRINT`, it
would replace the fresh mesh.

Without object labels there is no print area to adapt to: expect a full-bed mesh, and park and purge
near the bed's front-left corner. Turn labels on.

---

## Settings

The purge and park values are variables of the macro `_KAMP_Settings` (for example
`variable_purge_amount`, `variable_purge_margin`, `variable_flow_rate`, `variable_smart_park_height`),
and the start/end behaviour is in `_START_END_PARAMS` (for example
`variable_full_bed_mesh_above_percentage_full`, `variable_start_preheat_nozzle_temp`). Both are in
`/etc/nebulaos/klipper/print.cfg`. Bed warp stabilisation is the switch `Bed_Warp_Stabilisation`
(an `output_pin`), which Mainsail shows with the other outputs.

---

## Credits

The purge and park macros derive from
[pellcorp/creality](https://github.com/pellcorp/creality) ("SimpleAF") and, through it, from
[KAMP](https://github.com/kyleisah/Klipper-Adaptive-Meshing-Purging) (GPL-3.0). The adaptive mesh
itself is Klipper's native `ADAPTIVE=1`.
