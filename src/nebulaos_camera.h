#ifndef __NEBULAOS_CAMERA_H__
#define __NEBULAOS_CAMERA_H__

#include "hv/json.hpp"

#include <string>
#include <vector>

using json = nlohmann::json;

// Pure, LVGL-free logic for Settings > Display > Camera (G121, Run 22): the
// Creality Nebula Camera controls NebulaOS keeps and re-applies
// (nebulaos_osconfig/camera.py):
//   machine.nebulaos.camera.status     {config, defaults, ranges, present, firmware}
//   machine.nebulaos.camera.settings   {"settings": {key: value, ...}}
//   machine.nebulaos.camera.defaults / .reset / .version
// Exposure is one-way: once manual, only Restart camera brings automatic back.
//
// This file may be distributed under the terms of the GNU GPLv3 license.

extern const char *const CAMERA_STATUS_METHOD;
extern const char *const CAMERA_SETTINGS_METHOD;
extern const char *const CAMERA_DEFAULTS_METHOD;
extern const char *const CAMERA_RESET_METHOD;
extern const char *const CAMERA_VERSION_METHOD;

// Dropdown options ("\n"-separated, LVGL) and their values, same order.
extern const char *const CAMERA_IR_OPTIONS;       // Auto / Always colour / Always black-and-white
extern const char *const CAMERA_FLICKER_OPTIONS;  // Off / 50 Hz / 60 Hz
const std::vector<std::string> &camera_ir_values();       // auto color bw
const std::vector<std::string> &camera_flicker_values();  // off 50 60
int camera_index_of(const std::vector<std::string> &values, const std::string &v);

struct CameraSlider {
  const char *key;
  const char *label;
  int min;
  int max;
};
// brightness contrast saturation sharpness zoom wb_temperature exposure_time
const std::vector<CameraSlider> &camera_sliders();

struct CameraView {
  bool ok = false;          // the status call worked
  bool present = false;     // a Nebula Camera is connected
  std::string firmware;     // "" when not known yet
  json config;              // the saved settings (object)
  json defaults;
};
CameraView camera_view(const json &status_reply);
int camera_int(const json &config, const char *key, int fallback);
bool camera_bool(const json &config, const char *key, bool fallback);
std::string camera_str(const json &config, const char *key, const std::string &fallback);

json camera_settings_params(const json &changes);   // {"settings": changes}
// After settings/defaults/reset: what to tell the user ("" = nothing).
std::string camera_result_text(const json &reply);
// The firmware string of a version reply ("" when not there).
std::string camera_version_of(const json &reply);

#endif // __NEBULAOS_CAMERA_H__
