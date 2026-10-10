#include "nebulaos_calibration_flow.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

static std::string str(const json &j, const std::string &key) {
  if (j.is_object() && j.contains(key) && j[key].is_string()) {
    return j[key].template get<std::string>();
  }
  return "";
}

static bool num(const json &j, const std::string &key, double &out) {
  if (j.is_object() && j.contains(key) && j[key].is_number()) {
    out = j[key].template get<double>();
    return std::isfinite(out);
  }
  return false;
}

static std::string prefix(CalFlow flow) {
  switch (flow) {
    case CalFlow::ESTEPS: return "esteps_";
    case CalFlow::AUTO_CALIBRATE: return "auto_calibrate_";
    case CalFlow::INPUT_SHAPER: return "input_shaper_";
  }
  return "";
}

std::string cal_start_command(CalFlow flow) {
  switch (flow) {
    case CalFlow::ESTEPS: return "NEBULAOS_ESTEPS_CALIBRATE";
    case CalFlow::AUTO_CALIBRATE: return "NEBULAOS_AUTO_CALIBRATE";
    case CalFlow::INPUT_SHAPER: return "NEBULAOS_INPUT_SHAPER_CALIBRATE";
  }
  return "";
}

std::string cal_cancel_command(CalFlow flow) {
  switch (flow) {
    case CalFlow::ESTEPS: return "_NEBULAOS_ESTEPS_CANCEL";
    case CalFlow::AUTO_CALIBRATE: return "NEBULAOS_CALIBRATION_CANCEL";
    case CalFlow::INPUT_SHAPER: return "_NEBULAOS_INPUT_SHAPER_CANCEL";
  }
  return "";
}

std::string cal_measure_command(const std::string &text) {
  if (text.empty()) return "";
  char *end = nullptr;
  double mm = std::strtod(text.c_str(), &end);
  if (end == text.c_str() || *end != '\0' || !std::isfinite(mm) || mm <= 0.0 || mm > 500.0) {
    return "";
  }
  char buf[64];
  std::snprintf(buf, sizeof(buf), "NEBULAOS_ESTEPS_CALIBRATE MEASURED=%.2f", mm);
  return buf;
}

static const char *const DESCRIPTION_ESTEPS =
  "Checks how much filament the extruder really pushes and corrects it.\n"
  "The hotend heats; you mark the filament at the extruder entry, the printer "
  "extrudes a fixed length, and you enter how far the mark moved.";
static const char *const DESCRIPTION_AUTO =
  "Cleans the nozzle, measures the Z offset with the load cell and probes a "
  "bed mesh, then saves everything with one Klipper restart.\n"
  "Takes several minutes. The bed must be empty.";
static const char *const DESCRIPTION_SHAPER =
  "Measures resonances with the accelerometer: first mounted on the toolhead "
  "(X), then on the bed (Y). Saves the result with one Klipper restart.\n"
  "Have the accelerometer ready.";

CalView cal_view(CalFlow flow, const json &status, bool printing) {
  CalView v;
  switch (flow) {
    case CalFlow::ESTEPS: v.title = "E-Steps Calibration"; break;
    case CalFlow::AUTO_CALIBRATE: v.title = "Z-Offset & Bed Mesh"; break;
    case CalFlow::INPUT_SHAPER: v.title = "Input Shaper"; break;
  }
  if (!status.is_object()) {
    v.text = "NebulaOS calibration is not available on this printer.";
    return v;
  }
  v.available = true;
  const std::string p = prefix(flow);
  std::string state = str(status, p + "state");
  std::string stage = str(status, p + "stage");
  std::string error = str(status, p + "error");
  const char *description = flow == CalFlow::ESTEPS ? DESCRIPTION_ESTEPS
    : flow == CalFlow::AUTO_CALIBRATE ? DESCRIPTION_AUTO : DESCRIPTION_SHAPER;

  if (state.empty() || state == "idle") {
    v.text = description;
    v.can_start = !printing;
  } else if (state == "running") {
    v.text = stage.empty() ? std::string("Running...") : "Running: " + stage;
    // E-steps cannot be cancelled while it heats; the others can.
    v.can_cancel = flow != CalFlow::ESTEPS;
  } else if (state == "awaiting_continue") {
    v.text = "Mark the filament where it enters the extruder, then tap "
             "Start Extrusion in the prompt.";
    v.can_cancel = true;
  } else if (state == "awaiting_measurement") {
    v.text = "Measure how far the mark moved and enter it in mm.";
    v.can_cancel = true;
    v.needs_measurement = true;
  } else if (state == "awaiting_confirmation") {
    v.text = "Check the new value in the prompt and tap Apply & Save.";
    v.can_cancel = true;
  } else if (state == "awaiting_x_mount") {
    v.text = "Mount the accelerometer firmly on the toolhead, then tap "
             "Continue in the prompt.";
    v.can_cancel = true;
  } else if (state == "awaiting_y_mount") {
    v.text = "Move the accelerometer firmly onto the bed, then tap "
             "Continue in the prompt.";
    v.can_cancel = true;
  } else if (state == "commit_requested" || state == "restart_pending"
             || state == "verification_pending") {
    v.text = "Saving and restarting Klipper...";
  } else if (state == "complete") {
    v.text = "Done - the result is saved.";
    if (flow == CalFlow::ESTEPS) {
      double before = 0, after = 0;
      if (num(status, "esteps_old_rotation_distance", before)
          && num(status, "esteps_new_rotation_distance", after)) {
        char buf[128];
        std::snprintf(buf, sizeof(buf),
                      "Done - rotation_distance %.4f -> %.4f, saved.", before, after);
        v.text = buf;
      }
    }
    v.can_start = !printing;
  } else if (state == "cancelled") {
    v.text = "Cancelled - nothing was changed.";
    v.can_start = !printing;
  } else if (state == "error") {
    v.text = "Failed: " + (error.empty() ? std::string("see the console") : error);
    v.can_start = !printing;
  } else {
    v.text = "State: " + state;
  }
  if (printing && (v.can_start || state.empty() || state == "idle")) {
    v.text += "\n\nNot available while printing.";
  }
  return v;
}
