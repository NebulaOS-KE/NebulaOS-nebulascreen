# Resetting

All reset options are on the screen under **Settings → Reset**. NebulaScreen cannot be uninstalled
on its own: it is part of the NebulaOS image.

| Button | What it does |
|---|---|
| **Reset GUI settings** | Overwrites the screen's settings file (`nebulascreen.json`: theme, sleep, sensors shown, ...) with the image's default copy, then restarts the screen. Klipper keeps running; printer config and print files are not touched. |
| **Reset touch calibration** | Clears the saved touch calibration. The screen restarts and shows the calibration wizard. |
| **Clean NebulaOS reset** (red frame) | Erases all NebulaOS data and restarts as a new install: printer configuration and calibration, Wi-Fi networks, network settings, root password, print history and screen settings. Your G-code files and the other boot slot (A/B) are not touched. Afterwards, connect to Wi-Fi again on the screen. |
| **Swap boot slot (A/B)** (orange frame) | Restarts the printer into the other boot slot. The confirmation shows what is installed there. |

Each button asks for confirmation first. **Clean NebulaOS reset** and **Swap boot slot** are blocked
while a print is running.

## Swapping boot slots

The printer has two system slots, A and B. A system image installed from
**Settings → Updates → System image** goes into the slot that is not running.

- If the other slot runs NebulaOS, you come back the same way: **Settings → Reset → Swap boot slot**
  there.
- If the other slot runs the Creality system, coming back needs SSH on that system, installing a
  NebulaOS `.img` with Creality's own updater, or USB recovery (see the NebulaOS-firmware
  documentation). Do not switch the printer off and on while the Creality system runs: it can
  re-flash the printer board.

## Freeing space from the Creality system

**Settings → System → Storage → Clean up Creality files** removes the Creality system's data from the
data drive. It shows what it would free first, and can also delete Creality's timelapse videos.
NebulaOS, your print files and your settings are not affected. If the other slot still holds the
Creality system, that system starts like after a factory reset afterwards.
