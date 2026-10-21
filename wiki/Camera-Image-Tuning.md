# Camera image tuning

This page described persistent camera image tuning (`CAM_BRIGHTNESS`, `CAM_CONTRAST`,
`CAM_SATURATION`, `CAM_HUE` macros) installed by the old standalone screen installer or the Creality
Helper Script. NebulaOS does not ship those macros, and NebulaScreen has no camera panel.

NebulaOS ships three webcam quality presets instead, as Klipper macros you can run from Mainsail or
the screen's Macros tab:

| Macro | Webcam setting |
|---|---|
| `SET_CAMERA_QUALITY_LOW` | 640×480 |
| `SET_CAMERA_QUALITY_MED` | 1280×720 |
| `SET_CAMERA_QUALITY_HIGH` | 1920×1080 at 30 fps (the default) |

They are defined in the NebulaOS image (`/etc/nebulaos/klipper/camera.cfg`), not in this repository.
