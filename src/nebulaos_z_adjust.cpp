// Screen Z offset adjustment, saved by NebulaOS (G92). See nebulaos_z_adjust.h.
//
// This file may be distributed under the terms of the GNU GPLv3 license.
#include "nebulaos_z_adjust.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

const char *const Z_ADJUST_OBJECT = "nebulaos_z_adjust";

bool z_adjust_available(const json &objects) {
  const json *list = &objects;
  if (objects.is_object()) {
    if (objects.contains("objects")) {
      list = &objects["objects"];
    } else if (objects.contains("result") && objects["result"].is_object() &&
               objects["result"].contains("objects")) {
      list = &objects["result"]["objects"];
    } else {
      return false;
    }
  }
  if (!list->is_array()) {
    return false;
  }
  for (const auto &o : *list) {
    if (o.is_string() && o.get<std::string>() == Z_ADJUST_OBJECT) {
      return true;
    }
  }
  return false;
}

static bool positive_number(const std::string &s) {
  if (s.empty()) {
    return false;
  }
  char *end = nullptr;
  double v = std::strtod(s.c_str(), &end);
  return end != nullptr && *end == '\0' && std::isfinite(v) && v > 0;
}

std::string z_adjust_step_command(bool available, bool raise, const std::string &step) {
  if (!positive_number(step)) {
    return "";
  }
  const char *sign = raise ? "+" : "-";
  if (available) {
    return std::string("NEBULAOS_Z_ADJUST DELTA=") + sign + step;
  }
  return std::string("SET_GCODE_OFFSET Z_ADJUST=") + sign + step + " MOVE=1";
}

std::string z_adjust_reset_command(bool available) {
  return available ? "NEBULAOS_Z_ADJUST RESET=1" : "SET_GCODE_OFFSET Z=0 MOVE=1";
}

std::string z_adjust_text(bool available, const json &status) {
  if (!available || !status.is_object() || !status.contains("value") ||
      !status["value"].is_number()) {
    return "";
  }
  double v = status["value"].get<double>();
  if (std::fabs(v) < 0.0005) {
    v = 0.0;  // never "-0.000"
  }
  char buf[64];
  std::snprintf(buf, sizeof(buf), "Z offset adjustment: %.3f mm", v);
  std::string text = buf;
  std::string error;
  if (status.contains("error") && status["error"].is_string()) {
    error = status["error"].get<std::string>();
  }
  if (!error.empty()) {
    return text + ", NOT saved: " + error;
  }
  return text + ", saved";
}
