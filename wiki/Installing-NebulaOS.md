# Installing NebulaOS

NebulaOS installs with Creality's own updater, which is already on every Ender-3 V3 KE. It writes
NebulaOS into the printer's second system slot and starts it. Your current system (stock Creality
firmware or OpenKE) stays in the first slot, so you can go back.

Tested from OpenKE on 2026-10-29. The path through Creality's touchscreen is not tested yet.

## 1. Download

From [NebulaOS releases](https://github.com/NebulaOS-KE/NebulaOS/releases), take
`NebulaOS-Ender3V3KE-<version>.img` and its `.sha256` file.

- **Do not rename the file.** Creality's updater finds the parts inside by the file name. A renamed
  file stops with `failed to get .../ota_config.in`, and nothing is installed.
- Check the download (Linux/macOS): `sha256sum -c NebulaOS-Ender3V3KE-<version>.img.sha256`.

## 2a. From OpenKE (no Creality touchscreen): over SSH

```
scp NebulaOS-Ender3V3KE-<version>.img root@<printer-ip>:/usr/data/
ssh root@<printer-ip> /etc/ota_bin/local_ota_update.sh /usr/data/NebulaOS-Ender3V3KE-<version>.img
```

The password is your printer's root password. The updater unpacks the file, checks every part, and
writes the kernel, the system and the power-on logo, with percentages. It ends with
`ota update ok`. The line `sh: 1.1.0.15: bad number` is normal: it comes from the updater's
version check and stops nothing.

Then restart with a **soft reboot**:

```
ssh root@<printer-ip> reboot
```

The updater does not restart the printer by itself. **Do not switch the printer off and on
instead**: a stock system that starts from power-on can flash old firmware into the mainboard.

## 2b. From the stock Creality firmware: with its touchscreen

Copy the `.img` (name unchanged) to the root of a FAT32 USB stick, plug it into the printer and
start the update from Creality's screen. That screen runs the same updater and restarts the printer
when it is done.

## 3. First start

The first start of NebulaOS takes about a minute (later starts about 15 seconds). Then:

1. Connect to Wi-Fi: **Settings → Connections → Wi-Fi networks**.
2. Calibrate the printer once: [Calibration guide](Calibration-Guide).
3. Delete the copied file if you used SSH (133 MB):
   `ssh root@<printer-ip> rm /usr/data/NebulaOS-Ender3V3KE-<version>.img`

Mainsail is at `http://<printer-ip>` (or `http://<printer-name>.local`).

## If it stops

Every check runs before anything is written. If the updater prints `ota: error` before
`ota: started`, nothing changed: the printer keeps running its old system. Please report the output
([Troubleshooting](Troubleshooting)).

## Going back

The old system stays in the first slot. On NebulaOS: **Settings → Reset → Swap boot slot**
([Resetting](Resetting)). A printer that does not start at all is recovered over USB with the
release's `.ingenic` file ([Recovery](Recovery)).

## Later updates

Once NebulaOS runs, it updates itself from the screen: [Updating NebulaOS](Updating-NebulaOS).
