# Recovery

NebulaOS has four ways back, from automatic to last resort. Use the first one that fits.

| Situation | What to do |
|---|---|
| A new system image does not come up properly | Nothing: NebulaOS goes back by itself (below) |
| NebulaOS runs, but you want the other slot | **Settings → Reset → Swap boot slot** |
| The screen is dead but the printer is on the network | SSH, then switch the slot (below) |
| Nothing starts at all | USB recovery from a computer (below) |

## 1. Automatic: the trial start

After a system image install from the screen, the new slot starts as a trial. If Klipper and the
screen do not come up within about 5 minutes, or the new system resets before it is confirmed
three times (for example the watchdog after a freeze), NebulaOS switches back to the previous slot
once, by itself. The screen then says what happened.

Not covered: an install through Creality's updater (that one has no trial), and a kernel that does
not start at all.

## 2. Swap the boot slot on the screen

**Settings → Reset → Swap boot slot** restarts into the other slot. The confirmation says what is
in it. Coming back from a stock or OpenKE slot needs SSH or USB (below), because those systems have
no NebulaOS swap button.

**Do not power-cycle a printer that runs the stock system**: a stock power-on can flash old
firmware into the mainboard. Use a soft reboot.

## 3. Over SSH

Both NebulaOS and stock/OpenKE accept SSH as `root`. The slot is chosen by a marker; the steps are
in NebulaOS-firmware's
[slot switching guide](https://github.com/NebulaOS-KE/NebulaOS-firmware/blob/main/docs/HOW_TO_SWITCH_STOCK_AND_CUSTOM.md).

## 4. USB recovery from a computer

For a printer that does not start. The printer's board has a USB recovery mode (mask ROM): power
off, hold both small buttons next to the screen's micro-USB port for 3 seconds, release the reset
button first. A computer then talks to the board over a USB cable.

- **Switch the boot slot back** (Linux, tested 2026-10-14): NebulaOS-firmware's
  `tools/recovery/usb-set-boot-slot.sh A` or `B`, with the pinned `usbboot` tool. Nothing else is
  written. Steps: [Developer recovery](https://github.com/NebulaOS-KE/NebulaOS-firmware/blob/main/docs/DEVELOPER_RECOVERY.md#3-usb-recovery--the-panic-button).
- **Write a whole system again**: each release has a `.ingenic` file for the Ingenic USB Cloner.
  It rewrites the printer from scratch: NebulaOS in slot 2, Creality's stock system in slot 1.
  **It erases your settings, prints and calibration**; only the printer's factory serial number
  and network address are kept. Not tested by NebulaOS on a printer yet - use it only when nothing
  else works.

## What survives a slot switch

Your `printer.cfg`, macros, `moonraker.conf`, Wi-Fi networks, uploaded files and screen settings
live on the shared data partition and stay. The two systems keep their own settings apart.
