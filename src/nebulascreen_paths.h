#ifndef __NEBULASCREEN_PATHS_H__
#define __NEBULASCREEN_PATHS_H__

#include "hv/json.hpp"

// NebulaScreen (Run 15, the GuppyScreen -> NebulaScreen rename): the device
// paths and names this app uses on a NebulaOS image, in one place. They form
// the nebulaos_ui_api 1 path contract (see the firmware's G82 review).
//
// Internal C++ identifiers (class GuppyScreen, GUPPY_* build flags) keep the
// upstream names on purpose: this app is based on GuppyScreen by ballaswag
// (GPL-3), see NOTICE.
//
// This file may be distributed under the terms of the GNU GPLv3 license.

namespace nebulascreen {

constexpr const char *kProductName = "NebulaScreen";
constexpr const char *kService = "nebulascreen";
constexpr const char *kConfigFile = "nebulascreen.json";
constexpr const char *kConfigPath = "/opt/nebulascreen/nebulascreen.json";
constexpr const char *kDefaultConfig =
  "/usr/share/nebulaos/nebulascreen/nebulascreen.default.json";
constexpr const char *kInitScript = "/etc/init.d/S58nebulascreen";
constexpr const char *kLogPath = "/opt/printer_data/logs/nebulascreen.log";
constexpr const char *kThumbnailPath = "/opt/nebulascreen/thumbnails";
constexpr const char *kBeepPath = "/opt/nebulascreen/nebulabeep";
constexpr const char *kBeepName = "nebulabeep";

// Moonraker database namespace holding the user's macro and console
// favorites. It stays "guppyscreen" on purpose: the database is shared by
// both A/B slots and by Mainsail, and renaming it would lose the favorites.
constexpr const char *kLegacyDbNamespace = "guppyscreen";

// The config key for the screen's own init script (was "guppy_init_script").
constexpr const char *kInitScriptKey = "nebulascreen_init_script";

// A config migrated from the GuppyScreen file carries the old absolute paths
// and the old init-script key. Map them to the new names, in memory; the next
// save persists them in the NEW file only (the old file belongs to the other
// image slot and is never written). Only exact legacy values are changed;
// anything the user set themselves is left alone. Returns true if anything
// changed.
bool normalize_legacy_config(nlohmann::json &config);

}  // namespace nebulascreen

#endif  // __NEBULASCREEN_PATHS_H__
