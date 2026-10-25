#include "nebulaos_calibration_status.h"

#include <cmath>
#include <cstdio>
#include <ctime>

// This file may be distributed under the terms of the GNU GPLv3 license.

static std::string str(const json &j, const char *key) {
  if (j.is_object() && j.contains(key) && j[key].is_string()) {
    return j[key].template get<std::string>();
  }
  return "";
}

static bool num(const json &j, const char *key, double &out) {
  if (j.is_object() && j.contains(key) && j[key].is_number()) {
    out = j[key].template get<double>();
    return std::isfinite(out);
  }
  return false;
}

// ---- G105: menu ------------------------------------------------------------

const std::vector<CalMenuEntry> &cal_menu_in_order() {
  static const std::vector<CalMenuEntry> rows = {
    {"bed_tram", "Bed Tramming",
     "Levels the four corners, one at a time - about 10 min"},
    {"pid", "Heaters (PID)",
     "Tunes the bed and hotend heating - about 10 min"},
    {"axis_twist", "Axis Twist",
     "Corrects a twisted X gantry for probing - about 5 min"},
    {"auto_calibrate", "Auto Calibration",
     "Load cell check, nozzle clean, Z offset, bed mesh - about 7 min, no tools needed"},
    {"input_shaper", "Input Shaper",
     "Less ringing at speed; needs the accelerometer - about 5 min"},
    {"esteps", "E-Steps",
     "Checks how much filament the extruder pushes - about 5 min"},
    {"skew", "Skew Correction",
     "Squares X and Y from a printed test part - about 30 min"},
  };
  return rows;
}

const std::vector<CalMenuEntry> &cal_menu_other() {
  static const std::vector<CalMenuEntry> rows = {
    {"load_cell", "Load Cell Calibration",
     "Automatic re-check, or manual with a bottle - 1-2 min"},
    {"tmc_autotune", "TMC Autotune",
     "Optional: quieter motors - about 1 min"},
    {"bed_mesh", "Bed Mesh",
     "View the saved mesh or measure a new one"},
  };
  return rows;
}

std::string cal_short_date(double epoch) {
  if (!std::isfinite(epoch) || epoch <= 0) return "";
  static const char *const months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                       "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  time_t t = (time_t)epoch;
  struct tm tm_buf;
  if (localtime_r(&t, &tm_buf) == nullptr) return "";
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%d %s", tm_buf.tm_mday, months[tm_buf.tm_mon % 12]);
  return buf;
}

CalRowStatus cal_row_status(const json &history, const CalMenuEntry &e) {
  CalRowStatus r;
  r.text = e.description;
  if (!history.is_object() || !history.contains(e.key) || !history[e.key].is_object()) {
    return r;
  }
  const json &h = history[e.key];
  std::string redo = str(h, "redo");
  if (!redo.empty()) {
    r.state = CalRowState::REDO;
    r.text = "Redo: " + redo;
    return r;
  }
  double done_at = 0;
  if (num(h, "done_at", done_at) && done_at > 0) {
    r.state = CalRowState::DONE;
    std::string date = cal_short_date(done_at);
    std::string summary = str(h, "summary");
    r.text = "Done" + (date.empty() ? std::string() : " " + date)
             + (summary.empty() ? std::string() : " - " + summary);
  }
  return r;
}

bool cal_history_supported(const json &status) {
  return status.is_object() && status.contains("history") && status["history"].is_object();
}

std::string cal_record_command(const std::string &key, const std::string &summary) {
  std::string clean;
  for (char c : summary) {
    if (c == '"' || c == '\'' || c == '\\') continue;
    if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    clean += c;
    if (clean.size() >= 80) break;
  }
  std::string cmd = "NEBULAOS_CALIBRATION_RECORD FLOW=" + key;
  if (!clean.empty()) cmd += " SUMMARY=\"" + clean + "\"";
  return cmd;
}

std::string cal_record_prefix(const json &status, const std::string &key,
                              const std::string &summary) {
  if (!cal_history_supported(status)) return "";
  return cal_record_command(key, summary) + "\n";
}

std::string cal_axis_twist_save_script(const json &status) {
  return "BED_MESH_PROFILE REMOVE=default\n"
         + cal_record_prefix(status, "axis_twist", "Axis twist compensation saved")
         + "SAVE_CONFIG";
}

// ---- G105: PID -------------------------------------------------------------

int pid_default_target(PidHeater h) { return h == PidHeater::BED ? 65 : 230; }

std::string pid_command(PidHeater h) {
  char buf[96];
  std::snprintf(buf, sizeof(buf), "PID_CALIBRATE HEATER=%s TARGET=%d",
                h == PidHeater::BED ? "heater_bed" : "extruder", pid_default_target(h));
  return buf;
}

std::string pid_summary(PidHeater h) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%s tuned at %d C",
                h == PidHeater::BED ? "Bed" : "Hotend", pid_default_target(h));
  return buf;
}

std::string pid_save_script(const json &status, PidHeater h) {
  return cal_record_prefix(status, "pid", pid_summary(h)) + "SAVE_CONFIG";
}

const char *const PID_DESCRIPTION =
  "Tunes how the bed and the hotend heat, so their temperatures stay steady. "
  "Run it once at setup and after changing a heater, a thermistor or the "
  "hotend.\n"
  "Each heater takes about 5 minutes and saves with one Klipper restart. "
  "Start with the bed; keep the bed empty and the printer cool.";

std::string pid_running_text(PidHeater h, const json &heater_status) {
  bool bed = h == PidHeater::BED;
  char buf[160];
  std::snprintf(buf, sizeof(buf),
                "Tuning the %s at %d C - about 5 minutes. The temperature rises and "
                "falls a few times; this is normal.", bed ? "bed" : "hotend",
                pid_default_target(h));
  std::string t = buf;
  double cur = 0, target = 0;
  if (num(heater_status, "temperature", cur)) {
    if (num(heater_status, "target", target)) {
      std::snprintf(buf, sizeof(buf), "\n\n%s: %.0f / %.0f C", bed ? "Bed" : "Hotend", cur, target);
    } else {
      std::snprintf(buf, sizeof(buf), "\n\n%s: %.0f C", bed ? "Bed" : "Hotend", cur);
    }
    t += buf;
  }
  return t;
}

// ---- G104: progress ---------------------------------------------------------

const char *const CAL_RESTART_NOTICE =
  "Klipper restarts now to save the result. The screen reconnects by itself "
  "- do not switch the printer off.";
const char *const CAL_RECONNECT_NOTICE =
  "Klipper is restarting - waiting for it to come back...";

const char *cal_flow_key(CalFlow flow) {
  switch (flow) {
    case CalFlow::ESTEPS: return "esteps";
    case CalFlow::AUTO_CALIBRATE: return "auto_calibrate";
    case CalFlow::INPUT_SHAPER: return "input_shaper";
    case CalFlow::LOAD_CELL: return "load_cell";
    case CalFlow::BED_TRAM: return "bed_tram";
  }
  return "";
}

const std::vector<std::string> &cal_flow_steps(CalFlow flow) {
  static const std::vector<std::string> autocal = {
    "Load cell check", "Homing", "Nozzle clean", "Heating up", "Settling",
    "Load cell check (hot)", "Measuring the Z offset", "Bed mesh",
    "Saving (Klipper restarts)", "Verifying"};
  static const std::vector<std::string> shaper = {
    "Homing", "Mount the sensor on the toolhead", "Measuring X",
    "Move the sensor to the bed", "Measuring Y", "Saving (Klipper restarts)", "Verifying"};
  static const std::vector<std::string> esteps = {
    "Heating up", "Mark the filament", "Measure and enter the length",
    "Confirm the new value", "Saving (Klipper restarts)"};
  static const std::vector<std::string> none;
  switch (flow) {
    case CalFlow::AUTO_CALIBRATE: return autocal;
    case CalFlow::INPUT_SHAPER: return shaper;
    case CalFlow::ESTEPS: return esteps;
    default: return none;
  }
}

CalProgressView cal_progress_view(CalFlow flow, const json &status) {
  CalProgressView v;
  if (!status.is_object() || !status.contains("progress")) return v;
  const json &p = status["progress"];
  if (!p.is_object() || str(p, "flow") != cal_flow_key(flow)) return v;
  double step = 0, total = 0;
  if (!num(p, "step", step) || !num(p, "total", total)) return v;
  v.step = (int)step;
  v.total = (int)total;
  if (v.total < 1 || v.total > 30 || v.step < 1 || v.step > v.total) return v;
  v.active = true;
  v.label = str(p, "label");
  v.detail = str(p, "detail");
  std::string key = str(p, "key");
  v.restarting = key == "commit";
  char buf[48];
  std::snprintf(buf, sizeof(buf), "Step %d of %d", v.step, v.total);
  v.counter = buf;
  const std::vector<std::string> &known = cal_flow_steps(flow);
  bool use_known = (int)known.size() == v.total;
  for (int i = 1; i <= v.total; i++) {
    CalStepRow row;
    row.state = i < v.step ? CalStepState::DONE
      : i == v.step ? CalStepState::CURRENT : CalStepState::NEXT;
    if (i == v.step && !v.label.empty()) {
      row.label = v.label;
    } else if (use_known) {
      row.label = known[i - 1];
    } else {
      std::snprintf(buf, sizeof(buf), "Step %d", i);
      row.label = buf;
    }
    v.steps.push_back(row);
  }
  if (v.label.empty()) v.label = v.steps[v.step - 1].label;
  return v;
}

CalResultView cal_result_view(CalFlow flow, const json &status) {
  CalResultView r;
  if (!status.is_object() || !status.contains("last_result")) return r;
  const json &l = status["last_result"];
  if (!l.is_object() || str(l, "flow") != cal_flow_key(flow)) return r;
  if (!l.contains("ok") || !l["ok"].is_boolean()) return r;
  r.present = true;
  r.ok = l["ok"].get<bool>();
  double at = 0;
  std::string date = num(l, "finished_at", at) ? cal_short_date(at) : "";
  if (r.ok) {
    r.headline = "Done" + (date.empty() ? std::string() : " " + date);
    if (l.contains("summary") && l["summary"].is_array()) {
      for (auto &s : l["summary"]) {
        if (s.is_string() && !s.get<std::string>().empty()) r.lines.push_back(s.get<std::string>());
      }
    }
    if (flow == CalFlow::AUTO_CALIBRATE) {
      bool said = false;
      for (auto &s : r.lines) said = said || s.find("Z adjustment") != std::string::npos;
      if (!said) r.lines.push_back("Your Z adjustment was reset to 0.");
    }
  } else {
    std::string step = str(l, "error_step");
    r.headline = step.empty() ? std::string("Failed") : "Failed: " + step;
    std::string error = str(l, "error");
    std::string advice = str(l, "advice");
    r.lines.push_back("Why: " + (error.empty() ? std::string("see the console") : error));
    if (!advice.empty()) r.lines.push_back("What to do: " + advice);
    r.lines.push_back("Nothing was saved.");
  }
  return r;
}

// ---- G106: saved mesh --------------------------------------------------------

const char *const BEDMESH_NONE_TEXT =
  "No bed mesh yet - run Calibration > Auto Calibration.";

std::vector<std::vector<double>> bedmesh_profile_points(const json &profiles, const std::string &name) {
  std::vector<std::vector<double>> out;
  if (!profiles.is_object() || !profiles.contains(name)) return out;
  const json &p = profiles[name];
  if (!p.is_object() || !p.contains("points") || !p["points"].is_array()) return out;
  size_t cols = 0;
  for (auto &row : p["points"]) {
    if (!row.is_array() || row.empty()) return {};
    std::vector<double> r;
    for (auto &x : row) {
      if (!x.is_number()) return {};
      double d = x.get<double>();
      if (!std::isfinite(d)) return {};
      r.push_back(d);
    }
    if (cols == 0) cols = r.size();
    if (r.size() != cols) return {};
    out.push_back(r);
  }
  return out;
}

std::string bedmesh_saved_profile(const json &profiles) {
  if (!profiles.is_object()) return "";
  if (!bedmesh_profile_points(profiles, "default").empty()) return "default";
  for (auto &el : profiles.items()) {   // nlohmann objects iterate sorted by key
    if (!bedmesh_profile_points(profiles, el.key()).empty()) return el.key();
  }
  return "";
}

std::string bedmesh_saved_notice(const std::string &name) {
  return "Saved mesh '" + name + "' - each print measures its own mesh at the start";
}
