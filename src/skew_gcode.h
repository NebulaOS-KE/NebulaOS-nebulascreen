#ifndef __SKEW_GCODE_H__
#define __SKEW_GCODE_H__

// #31 (2026-11-22): the G-code the skew screen sends, kept pure for tests.
// Upstream Klipper's skew_correction: SET_SKEW sets the values in memory only,
// SKEW_PROFILE SAVE stores [skew_correction nebulaos] for SAVE_CONFIG, and
// calibration.cfg's delayed_gcode loads that profile at every Klipper start.
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "hv/json.hpp"

#include <cctype>
#include <string>

static const char *SKEW_PROFILE_NAME = "nebulaos";

// Apply: values, save the profile, the calibration-history record, persist.
inline std::string skew_apply_gcode(const std::string &ac, const std::string &bd,
                                    const std::string &ad, const std::string &record) {
  return "SET_SKEW XY=" + ac + "," + bd + "," + ad + "\n"
         "SKEW_PROFILE SAVE=" + SKEW_PROFILE_NAME + "\n" +
         record + "SAVE_CONFIG";
}

// Clear: removes the stored profile too, so a Klipper restart before the
// calibration print can't load the old correction again.
inline std::string skew_clear_gcode() {
  return std::string("SET_SKEW CLEAR=1\n"
                     "SKEW_PROFILE REMOVE=") + SKEW_PROFILE_NAME + "\n"
         "SAVE_CONFIG";
}

// On when Klipper's skew_correction reports our profile as loaded.
inline bool skew_is_on(const nlohmann::json &skew_status) {
  if (!skew_status.is_object()) return false;
  auto it = skew_status.find("current_profile_name");
  return it != skew_status.end() && it->is_string() &&
         it->get<std::string>() == SKEW_PROFILE_NAME;
}

// The three measurements behind the active correction, from the calibration
// history's skew summary ("A-C 141.12, B-D 141.29, A-D 99.71 mm", written by
// Apply). Klipper keeps only the skew factor, so this is the one place the
// screen can show them again. False (out untouched) when missing/malformed.
inline bool skew_saved_values(const nlohmann::json &calibration_status, std::string out[3]) {
  if (!calibration_status.is_object()) return false;
  auto h = calibration_status.find("history");
  if (h == calibration_status.end() || !h->is_object()) return false;
  auto e = h->find("skew");
  if (e == h->end() || !e->is_object()) return false;
  auto s = e->find("summary");
  if (s == e->end() || !s->is_string()) return false;
  const std::string sum = s->get<std::string>();
  static const char *KEYS[3] = {"A-C ", "B-D ", "A-D "};
  std::string v[3];
  for (int i = 0; i < 3; i++) {
    size_t at = sum.find(KEYS[i]);
    if (at == std::string::npos) return false;
    at += 4;
    size_t end = at;
    while (end < sum.size() && (std::isdigit((unsigned char)sum[end]) || sum[end] == '.')) end++;
    if (end == at) return false;
    v[i] = sum.substr(at, end - at);
  }
  for (int i = 0; i < 3; i++) out[i] = v[i];
  return true;
}

#endif  // __SKEW_GCODE_H__
