# Calibration guide (the short version)

Four steps on the screen, in this order. All of them are in **Tune → Calibration**.
For background, every other calibration and troubleshooting, see the
[full calibration walkthrough](Calibration-Explained).

| # | Screen | You need | Time |
|---|---|---|---|
| 1 | **Bed Tramming** | an empty bed (and maybe a printed shim) | 5-15 min |
| 2 | **Load Cell** | nothing (Automatic) or a 1.5 L bottle + kitchen scale (Manual) | 1-2 min |
| 3 | **Z-Offset & Bed Mesh** | an empty bed | several minutes |
| 4 | Fine-tune on the first print: **Tune → Z Offset** or **Fine Tune** | a test print | while printing |

---

## 1. Bed Tramming

The printer probes the four corners and shows each one compared with a reference corner, with
a sign: **minus is lower, plus is higher**. Example (rigid spacers, compared with the highest
corner):

```
Rear:   left -0.05 OK    right  0.00 highest
Front:  left -0.23 ADJUST    right -0.12 later
```

and below it what to do, one corner at a time:

> Front left: -0.23 mm. Add a 0.20 mm shim to raise it. Turn the printer off first, lift the
> bed plate at that corner and put the shim on the spacer.

With rubber or spring mounts the screen tells you which screw to turn and how far instead.
Correct the corner, tap **Done - measure again**, repeat until every corner says OK.
Details: [Bed tramming](Bed-Tramming).

## 2. Load Cell

The load cell under the front-left bed screw is the sensor that feels the nozzle touch the bed.
The Load Cell page has two choices at the top:

- **Automatic** (the default). The printer checks the sensor by itself on an empty bed: no
  bottle, nothing moves or heats. Tap **Run**. The page then says *Last check: passed - ready*.
  You do not have to run it by hand: Z-Offset & Bed Mesh (step 3) runs the same check first.
  If the check fails, the page says why and offers **Run** again or **Use Manual**.
- **Manual**. The calibration with a 1.5 L bottle of water and a kitchen scale (about 2 minutes,
  nothing moves or heats): [Load cell calibration (water bottle)](Load-Cell-Calibration).
  The value you measure is used **until you choose Automatic again**.

The choice is saved at once and survives restarts.

## 3. Z-Offset & Bed Mesh

One button does everything: with the load cell in Automatic mode it checks the load cell (cold
and hot, no bottle), cleans the nozzle, measures the Z offset with the load cell, probes a bed
mesh and saves it all with **one** Klipper restart. Remove everything from the bed and tap
**Start**.

It also sets the Z offset adjustment from step 4 back to **0**, because that adjustment was made
for the old Z offset.

## 4. Fine-tune on the first print

Print something with a large first layer and watch it. On the screen, **Tune → Z Offset** (or
the Z buttons in **Fine Tune**, or tap the Z value on the print screen):

- **Raise** (farther from the bed) when the line is rough or scraped, **Lower** (closer) when it
  is round and does not stick. Steps of 0.01 mm are usually right; 0.005 / 0.001 for the last bit.
- Every tap is **saved immediately**. The page says *Z offset adjustment: -0.040 mm, saved*.
  The value comes back after a Klipper restart, a pause/resume and a power cut.
- **Reset** sets the adjustment back to 0.
- If the line ever says *NOT saved* (orange), the adjustment works for this print but is lost at
  the next restart.

Mainsail: its own Z offset arrows change the offset for the running print only (not saved by
NebulaOS). Its **Save** button folds the current offset into the probe `z_offset` in
`printer.cfg` and sets the NebulaOS adjustment to 0, so nothing is counted twice.

---

## When to redo what

| What changed | Redo |
|---|---|
| New nozzle, new build plate | 3 (then 4 on the next print) |
| Shim at the front-left corner, or bed hardware (spacers, springs, the bed itself) | 2 and 3 |
| Corners uneven (first layer thick on one side) | 1, then 2, then 3 |

The front-left corner is special: the load cell sits under it, so changing that corner changes
how the load cell is mounted. That is why step 2 follows step 1.

## "Not now"

Some actions are refused while the printer is busy - for example **Restart Klipper** or an update
while a heater is on. The screen then shows a **Not now** popup with the reason and nothing
happens:

| Reason | Popup text |
|---|---|
| a print is running | Not possible while a print is running. |
| a print is paused | Not possible while a print is paused. |
| heating or moving (a heater target above 0 counts) | Not while the printer is heating or moving. |
| a calibration is running | Not possible while a calibration is running. |
| an update or MCU flash is running | Another update is running. |
| low memory | Not possible right now: the printer is low on memory. |
| state unknown | Printer state unknown - not possible right now. |

Wait until the print or the calibration ends, or set the heaters to 0, then tap again. Calibration
pages do the same on their own: while a print runs, their **Run**/**Start** buttons are not offered.
