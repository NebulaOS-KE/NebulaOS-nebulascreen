# Troubleshooting

Common problems with the screen and with print quality. For anything else, use
**Settings → System → Report a problem** on the printer: it sets the log level, saves the logs to a
USB stick and shows where to report the issue
([github.com/NebulaOS-KE/NebulaOS/issues](https://github.com/NebulaOS-KE/NebulaOS/issues)).

---

## Screen and touch

| Symptom | Fix |
|---|---|
| Screen upside down or rotated | Set `display_rotate` to `2` in `nebulascreen.json` (the KE default). See [Configuration](Configuration#display-rotation). |
| Taps land in the wrong place, e.g. after changing `display_rotate` | Calibration is rotation-specific. Redo it: **Settings → Reset → Reset touch calibration**. |
| Touch calibration came out wrong and you cannot reach the button | Edit `config/nebulascreen/nebulascreen.json` in Mainsail: set `"touch_calibrated": true` and remove `touch_calibration_coeff`, then restart the screen. The wizard runs again. See [Configuration](Configuration#touch-calibration). |
| Sensors shown with raw names like `Heater Bed` | None of the `id`s in `monitored_sensors` matches a Klipper object, so the screen shows every heater and sensor under its Klipper name. Fix the `id`s (an empty or missing list is replaced by Extruder, Bed and MCU Temp at start). See [Configuration](Configuration). |
| Screen seems stuck | **Settings → System → Restart GUI**, or over SSH `/etc/init.d/S58nebulascreen restart`. |
| Settings do not stick after a reboot | Check that `config/nebulascreen/nebulascreen.json` exists and is valid JSON. A file that cannot be parsed is ignored and the built-in defaults are used. |

---

## Printing and first layer

| Symptom | Fix |
|---|---|
| First layer uneven left to right, bed mesh does not help | [Axis Twist Compensation](Axis-Twist-Compensation). |
| One corner of the bed high or low | [Bed tramming](Bed-Tramming). |
| First layer uneven everywhere | Re-run the bed mesh: **Tune → Bed Mesh → Calibrate**. Clean the bed (IPA) and check nothing is loose. |
| First layer too high or too low everywhere | Baby-step Z offset: **Tune → Z Offset**, or tap the Z readout on the print status screen. |
| Parts not square | [Skew Correction](Skew-Correction). |
| Ghosting / echoes after sharp corners | [Input shaper](Calibration-Explained#step-5--input-shaper-kill-ringingghosting): **Tune → Calibration → Input Shaper**. |
| Adaptive mesh not used, full bed meshed instead | The print covers more than 75 % of the bed, the file has no object labels, or the start G-code passes `BED_MESH_ADAPTIVE=0` or `BED_MESH_PROFILE=`. See [Adaptive meshing and purge](Adaptive-Print-Setup). |
| Corner bulges, blobs or gaps | Pressure advance and flow: [Calibration walkthrough](Calibration-Explained#step-7--pressure-advance-flow--temperature-slicer-side). |
| A panel says "locked during print" | Expected: calibration, homing, extruding and similar actions are blocked while printing. See [Screen reference](Using-NebulaScreen). |

---

## Installing and updating

| Symptom | Fix |
|---|---|
| Creality's updater stops with `failed to get .../ota_config.in` | The `.img` was renamed. Download it again or give it back its release name `NebulaOS-Ender3V3KE-<version>.img`. Nothing was installed. |
| Creality's updater ends with `ota update ok`, but the printer still runs the old system | It does not restart by itself: soft reboot (`ssh root@<printer-ip> reboot`), never power off and on. See [Installing NebulaOS](Installing-NebulaOS). |
| **Online** says the clock is not set | The printer sets its clock from the internet after Wi-Fi connects. Wait a minute and try again. |
| **Online** says it could not reach the release channel | No internet, or GitHub was not reachable. Check Wi-Fi; try again later. |
| A download fails with "does not match the release record" | The file arrived damaged. It was deleted; start the download again. |
| An update says "not possible while a print is active" | Expected: updates wait until the printer is idle (no print, heaters off). |
| The new system went back to the old one by itself | The new system did not start properly (trial start). The screen showed why; the old system runs as before. See [Recovery](Recovery#1-automatic-the-trial-start). |

---

## Network

| Symptom | Fix |
|---|---|
| Not connected to Wi-Fi | **Settings → Connections → Wi-Fi networks**. After a clean NebulaOS reset, Wi-Fi has to be set up again here. |
| A static IP setting does not work | NebulaOS checks a new static address (gateway reachable) and switches back automatically when it does not work; this can take a couple of minutes. The setting applies to all Wi-Fi networks. |
| Wi-Fi events | **Settings → System → Logs** shows the Wi-Fi lines of `system.log`. |

---

## Logs

- Screen log: `/opt/printer_data/logs/nebulascreen.log` (rotates at 10 MB, keeps 3 old files). It is in
  Mainsail's log list too.
- Raise the detail: **Settings → System → Report a problem → Log level → Debug**. Debug switches back
  to Info by itself after 24 hours.
- **Settings → System → Logs** shows logs on the screen; **Save logs to USB** (in Report a problem)
  writes them to a USB stick.

---

## Simulator and development

| Symptom | Fix |
|---|---|
| Simulator crashes at start with `spdlog_ex: Failed opening file` | The directory in `log_path` does not exist. Create it, or set `log_path` in `build/bin/nebulascreen.json`. |
| Websocket reconnects every ~2 s | Expected when no Moonraker is reachable. |
| Layout looks wrong in the simulator | Build with `GUPPY_SMALL_SCREEN=1` for the 480×272 layout. |
| Build fails on GCC 14+ in `lv_touch_calibration` | Add `-Wno-incompatible-pointer-types` to `CFLAGS`. See [Building from source](Building-from-Source). |

