# Updating NebulaOS

Two kinds of updates, both from **Settings → Updates** on the screen:

| What | Where | How often |
|---|---|---|
| **The whole system** (kernel, system, screen app, everything in the image) | **Updates → System image** | Each NebulaOS release |
| **Apps** (Klipper, Moonraker, Mainsail, NebulaOS extensions, the screen app) | **Updates**, the list | When a component has a newer version |

Updates are refused while the printer is busy (printing, heating, calibrating). The screen says why.

## The whole system: System image

A system image is written into the boot slot (A/B) that is **not** running, so the running system
is never touched. The printer then restarts into the new slot. If the new system does not come up
properly, NebulaOS goes back to the previous slot by itself ([Recovery](Recovery#1-automatic-the-trial-start)).

### Online

1. **Settings → Updates → System image → Online.** The printer asks GitHub for the latest
   NebulaOS release.
2. If a newer version exists, it shows the version and the download size. **Download** fetches it
   (about 140 MB). The printer downloads only over HTTPS from github.com and keeps the file only
   when its size and sha256 match the release.
3. The download appears in the list as **Downloaded**. Tap **Install** next to it.
4. NebulaOS checks the file again, asks to confirm, installs it (a few minutes) and restarts.
   After the new system has started properly, the downloaded file is deleted.

The printer needs a correct clock for HTTPS; it sets it from the internet once Wi-Fi is connected.
Right after the first start this can take a minute.

### From a USB stick or Mainsail

Put a `.img` on a USB stick (FAT32), or upload it in Mainsail to **Machine → config → uploads →
system-image**. **Settings → Updates → System image** lists it; tap **Install**.

NebulaOS and Creality `.img` files are both accepted. Installing a Creality image replaces the
other slot with Creality's system; there is no automatic way back from it ([Recovery](Recovery)).

## Apps

**Settings → Updates** lists what Moonraker's update manager knows, with version and state.
**Check now** refreshes the list, **Update all** installs every available update, each row has its
own **Update** button. Klipper stops while Klipper or the extensions update. The same list is in
Mainsail (**Machine → Update Manager**).

- Klipper follows upstream Klipper, and the NebulaOS extensions follow their `production` branch.
  The extensions check at every Klipper start that Klipper still offers what they need, and stop
  with a clear message if not. The system image keeps the tested versions as a fallback.
- The screen app (NebulaScreen) updates from its own releases. A new system image brings its own
  screen app; the image's copy is used again after a system image install.

## What stays

Settings, calibration, `printer.cfg`, macros, uploaded files and Wi-Fi networks live on the data
partition and stay through every update.
