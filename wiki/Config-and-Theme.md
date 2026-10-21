# Config and theme

How the settings file is stored on a NebulaOS printer. The fields themselves are on
[Configuration](Configuration).

- NebulaScreen reads `nebulascreen.json` from the directory of its executable,
  `/opt/nebulascreen/` — part of the read-only squashfs image.
- At boot, NebulaOS bind-mounts the persistent file `/usr/data/nebulaos/nebulascreen/nebulascreen.json`
  over `/opt/nebulascreen/nebulascreen.json`, and the same file onto
  `config/nebulascreen/nebulascreen.json` so Mainsail can show and edit it. Both boot slots (A/B) use
  the same data partition.
- The first time, the file is seeded from the image's default (or copied once from a GuppyScreen-era
  `guppyconfig.json`).
- Because the target is a single-file bind mount, a save cannot rename a new file over it.
  `Config::save()` first writes a complete copy to `nebulascreen.json.saving` next to the persistent
  file, then rewrites the config in place; at the next start an interrupted save is completed from that
  copy or dropped.
- If the file exists but cannot be read or parsed, `Config::init()` uses the built-in defaults instead
  of crashing (`ThemeConfig::init()` does the same for themes). Covered by
  `make test-config-theme-parse-safety`.

Themes are read-only files in `/opt/nebulascreen/themes/`; the chosen theme name is stored in the
config (`theme`).

See also: [Integration with NebulaOS](Integration-with-NebulaOS).
