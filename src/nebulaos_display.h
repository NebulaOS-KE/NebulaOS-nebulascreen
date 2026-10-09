#ifndef __NEBULAOS_DISPLAY_H__
#define __NEBULAOS_DISPLAY_H__

#include "hv/json.hpp"

#include <string>
#include <vector>

using json = nlohmann::json;

// Pure, LVGL-free logic for the NebulaOS display contract (Moonraker component
// [nebulaos], NebulaOS-firmware overlay usr/lib/nebulaos/moonraker/nebulaos.py):
//   machine.nebulaos.display.status      -> {backlight{available, asleep, level, ...},
//                                             config{brightness, touch_wake}, levels[...]}
//   machine.nebulaos.display.brightness  {"level": N}, N in levels; persisted by NebulaOS
//   machine.nebulaos.display.sleep       backlight off; refused (SLEEP_UNAVAILABLE) when no
//                                        touch-wake path can run - the backlight stays on
//   machine.nebulaos.display.wake        idempotent
// NebulaOS owns the backlight, its persistence and the boot restore; GuppyScreen never
// touches /sys/class/backlight. Touch-wake of the backlight is NebulaOS's S98 watcher, so
// the screen comes back even if the wake call is late or Moonraker is down.
//
// This file may be distributed under the terms of the GNU GPLv3 license.

extern const char *const DISPLAY_STATUS_METHOD;
extern const char *const DISPLAY_BRIGHTNESS_METHOD;
extern const char *const DISPLAY_SLEEP_METHOD;
extern const char *const DISPLAY_WAKE_METHOD;

struct DisplayStatus {
  bool available = false;     // a reply with a usable backlight and at least one level
  std::vector<int> levels;    // percent, as offered by NebulaOS (today 25/50/75/100)
  int brightness = -1;        // persisted level (config.brightness), -1 if unknown
};

// Parses a JSON-RPC reply ({"result": {...}} or {"error": ...}). Anything unusable gives
// available=false, and the caller hides the brightness control.
DisplayStatus parse_display_status(const json &reply);

// Dropdown options, one per level: "25%\n50%\n75%\n100%".
std::string brightness_dropdown_options(const DisplayStatus &s);

// Index of the persisted level in levels; the highest level when it is not one of them.
int brightness_dropdown_index(const DisplayStatus &s);

// Error message of a JSON-RPC reply, or "" when it succeeded.
std::string jsonrpc_error_message(const json &reply);

#endif // __NEBULAOS_DISPLAY_H__
