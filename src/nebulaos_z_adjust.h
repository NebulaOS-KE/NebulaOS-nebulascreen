#ifndef __NEBULAOS_Z_ADJUST_H__
#define __NEBULAOS_Z_ADJUST_H__

#include "hv/json.hpp"

#include <string>

using json = nlohmann::json;

// Pure, LVGL-free logic for the screen's Z offset adjustment (G92, Run 17).
//
// NebulaOS-klipper-extensions owns the saved adjustment ("like OpenKE": one number in one
// file, applied live on every tap, re-applied at every Klipper start, reset to 0 when the
// Z-offset calibration saves a new probe z_offset):
//   NEBULAOS_Z_ADJUST DELTA=<+/-mm>   adjust live (relative) and save
//   NEBULAOS_Z_ADJUST RESET=1          set the saved adjustment and the live one to 0
//   status object nebulaos_z_adjust  {value, applied, error}
// Older extensions have no such object: the screen then falls back to the plain Klipper
// commands (SET_GCODE_OFFSET, memory only, lost on a Klipper restart) and never says
// "saved".
//
// This file may be distributed under the terms of the GNU GPLv3 license.

extern const char *const Z_ADJUST_OBJECT;  // "nebulaos_z_adjust"

// True when printer.objects.list (its "objects" array, or the whole {"objects": [...]}
// reply) contains the nebulaos_z_adjust object.
bool z_adjust_available(const json &objects);

// Command for one step. raise = nozzle farther from the bed (+). step is the selector's
// text ("0.01"); a step that is not a positive number gives "".
// Both forms use Klipper's normalized gcode frame (+ = farther), never invert_z_direction.
std::string z_adjust_step_command(bool available, bool raise, const std::string &step);

// Command for the Reset button.
std::string z_adjust_reset_command(bool available);

// The line under the readout. status = printer_state's nebulaos_z_adjust object (null when
// unknown). Empty when the object is missing (older extensions: nothing is saved, so the
// screen says nothing) or has no value yet.
//   "Z offset adjustment: 0.050 mm, saved"
//   "Z offset adjustment: 0.050 mm, NOT saved: <error>"
std::string z_adjust_text(bool available, const json &status);

#endif  // __NEBULAOS_Z_ADJUST_H__
