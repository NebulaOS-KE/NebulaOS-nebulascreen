# Installation and updates

NebulaScreen is not installed on its own. It is part of the **NebulaOS** image for the Ender-3 V3 KE:
[`NebulaOS-firmware`](https://github.com/coreflake1/NebulaOS-firmware) pins a commit of this
repository, cross-compiles it with `scripts/build-release.sh` and puts it into the read-only system
image. Installing NebulaOS installs NebulaScreen. Follow the NebulaOS-firmware documentation for
installing NebulaOS itself.

> **Mounting the screen:** a 3D-printable bracket for the KE display is on Thingiverse —
> [Ender-3 V3 KE screen mount](https://www.thingiverse.com/thing:6617266). For a mount closer to the
> stock position, **@DylanUnofficial** made the
> [Nebula screen mount](https://www.printables.com/model/1770386-creality-ender-3-v3-ke-openke-nebula-screen-mount)
> on Printables.

## Where it lives on the printer

| Path | What |
|---|---|
| `/opt/nebulascreen/nebulascreen` | The app (static MIPS executable, read-only image) |
| `/opt/nebulascreen/nebulabeep` | Compatibility client: forwards buzzer requests to Klipper's `nebulaos_beeper` ([Beeps & songs](Buzzer-and-Sounds)) |
| `/opt/nebulascreen/themes/` | Colour themes |
| `/opt/nebulascreen/nebulascreen.json` | Settings. Bind-mounted from the persistent file `/usr/data/nebulaos/nebulascreen/nebulascreen.json` |
| `config/nebulascreen/nebulascreen.json` | The same settings file, visible and editable in Mainsail's config browser |
| `/usr/share/nebulaos/nebulascreen/nebulascreen.default.json` | The image's default settings (used by **Settings → Reset → Reset GUI settings**) |
| `/opt/printer_data/logs/nebulascreen.log` | Log (rotates at 10 MB, keeps 3 old files) |
| `/etc/init.d/S58nebulascreen` | Service script (`start`, `stop`, `restart`) |

The settings file is described in [Configuration](Configuration).

To restart only the screen, use **Settings → System → Restart GUI**. Over SSH:
`/etc/init.d/S58nebulascreen restart`.

## Updates

- **With NebulaOS images.** A new NebulaOS image brings its own NebulaScreen. Images can be installed
  from the screen: **Settings → Updates → System image** (a `.img` from a USB stick or from
  `config/uploads/system-image`). The image is written to the boot slot (A/B) that is not running,
  and the printer restarts into it.
- **As a release zip.** Each release tag of this repository publishes one asset,
  `nebulascreen-vX.Y.Z.zip` ([Releases and deployment](Releases-and-Deployment)). It is made for
  Moonraker's update manager (`type: zip`), which installs it into
  `/usr/data/nebulaos/apps/nebulascreen`; the NebulaOS update supervisor checks it and rolls it back
  if it fails. The image's `moonraker.conf` has no update-manager entry for NebulaScreen yet, so in
  practice NebulaScreen updates come with NebulaOS images. After a new image is installed, the
  image's own NebulaScreen is used again.

**Settings → Updates** on the screen lists what Moonraker's update manager knows (Klipper, Moonraker,
Mainsail, NebulaOS extensions, ...), with **Check now**, **Update all** and one **Update** button per
entry.

## Coming from a GuppyScreen-era NebulaOS image

Older NebulaOS images ran the same app under the name GuppyScreen, with its settings in
`guppyconfig.json`. On the first boot of a NebulaScreen image, NebulaOS copies that file once to
`nebulascreen.json`. NebulaScreen then maps the old default paths and the old `guppy_init_script` key
to the new names; values you set yourself are not changed. The old file is never written, because the
other boot slot may still use it.

Macro and console favorites are stored in Moonraker's database under the namespace `guppyscreen`.
That name is kept on purpose, so favorites survive the rename.
