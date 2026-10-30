# NebulaScreen

NebulaScreen is the touchscreen app of **NebulaOS** for the **Creality Ender-3 V3 KE**. It runs
directly on the printer's 480×272 display (framebuffer, no X11 or Wayland) and talks to Klipper
through Moonraker.

NebulaScreen is based on [GuppyScreen](https://github.com/ballaswag/guppyscreen) by ballaswag
(GPL-3). See [History and credits](History-and-Credits) for the full lineage.

NebulaScreen is not installed on its own. It is part of every NebulaOS image, built by
[`NebulaOS-firmware`](https://github.com/NebulaOS-KE/NebulaOS-firmware) from a pinned commit of this
repository. See [Installation](Installation).

## Start here

1. **[Installing NebulaOS](Installing-NebulaOS)** — from stock Creality firmware or OpenKE, with
   Creality's own updater. Later: **[Updating NebulaOS](Updating-NebulaOS)** from the screen.
2. **[Screen reference](Using-NebulaScreen)** — every tab and panel.
3. **[Calibration walkthrough](Calibration-Explained)** — the order to calibrate a KE in.

## Print-quality guides

- First layer uneven left to right → [Axis Twist Compensation](Axis-Twist-Compensation)
- One bed corner high or low → [Bed tramming](Bed-Tramming)
- Automatic Z offset needs teaching once → [Load cell calibration](Load-Cell-Calibration)
- Mesh only the print area, purge next to it → [Adaptive meshing and purge](Adaptive-Print-Setup)
- Parts not square → [Skew Correction](Skew-Correction)
- Motors loud or hot → [TMC Autotune](TMC-Autotune)
- Something else wrong → [Troubleshooting](Troubleshooting)

## What the screen does

- Print status and control: temperatures, fans, LED, homing and jogging, file browser (including USB
  sticks), Exclude Object, Spoolman
- Interactive 3D bed mesh (rotate, zoom) plus a table view
- A **Calibration** menu with guided screens, in order: Bed Tramming, Heaters (PID), Axis Twist,
  Auto Calibration, Input Shaper, E-Steps, Skew Correction (then Load Cell Calibration, TMC Autotune
  and Bed Mesh), each with a status line (done / not done / redo)
- Live tuning during a print: speed, flow, Z offset, pressure advance; firmware retraction and machine
  limits on their own panels
- Power-loss recovery through NebulaOS
- NebulaOS settings on the screen: Wi-Fi and static IP, printer name, Spoolman server, display
  brightness and sleep, boot logo, Nebula Camera settings ([Camera](Camera-Image-Tuning)), updates
  (apps, and system images from USB, upload or online), logs, reset and A/B boot slot swap
- Buzzer: touch click, `M300` with real pitch, `PLAY_TUNE` songs ([Beeps & songs](Buzzer-and-Sounds))
- Print-state locks: actions that could ruin a running print are blocked or ask first

## Hardware

| | |
|---|---|
| Printer | Creality Ender-3 V3 KE, running NebulaOS |
| SoC / arch | Ingenic X2000 — MIPS (mipsel) |
| Display | 480×272, resistive touch |

The code also builds as an SDL simulator for desktop development
([Development and simulator](Development-and-Simulator)).

## Developers

Start with [Building from source](Building-from-Source) and [Architecture](Architecture).

## License

GPL-3.0. NebulaScreen is based on GuppyScreen by ballaswag; the KE work builds on
[probielodan/guppyscreen](https://github.com/probielodan/guppyscreen) and
[pellcorp/grumpyscreen](https://github.com/pellcorp/grumpyscreen), and the 3D bed mesh comes from
[prestonbrown/guppyscreen](https://github.com/prestonbrown/guppyscreen). Full credits:
[Contributing](Contributing#credits--attribution).
