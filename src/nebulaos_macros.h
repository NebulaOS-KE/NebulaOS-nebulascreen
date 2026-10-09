#ifndef __NEBULAOS_MACROS_H__
#define __NEBULAOS_MACROS_H__

#include <string>

// Pure, LVGL-free mapping from GuppyScreen actions to the Klipper macros NebulaOS ships
// (NebulaOS-firmware overlay etc/nebulaos/klipper/*.cfg). GuppyScreen's own
// guppy_cmd.cfg (_GUPPY_* macros) is not shipped on NebulaOS; saved configs still name
// those macros (guppyconfig.json is seeded once per device), so the legacy names are
// treated as aliases here instead of being sent to Klipper as unknown commands.
//
// This file may be distributed under the terms of the GNU GPLv3 license.

extern const char *const NEBULAOS_LOAD_FILAMENT;    // LOAD_FILAMENT
extern const char *const NEBULAOS_UNLOAD_FILAMENT;  // UNLOAD_FILAMENT

// Configured default_macros/{load,unload}_filament -> macro to run: legacy _GUPPY_*
// names and empty values map to the NebulaOS macro; anything else is the user's choice.
std::string filament_macro(const std::string &configured, bool load);

// G-code for the Unload button at `temp` degrees. The NebulaOS macro takes
// EXTRUDER_TEMP; a user-defined macro is sent as configured.
std::string unload_filament_gcode(const std::string &configured, int temp);

#endif // __NEBULAOS_MACROS_H__
