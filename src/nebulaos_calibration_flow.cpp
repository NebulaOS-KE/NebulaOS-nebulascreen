#include "nebulaos_calibration_flow.h"

#include <cctype>
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
    case CalFlow::LOAD_CELL: return "";   // own object, plain keys (G86)
    case CalFlow::BED_TRAM: return "";    // own object, plain keys (G87)
  }
  return "";
}

const char *cal_status_object(CalFlow flow) {
  if (flow == CalFlow::LOAD_CELL) return "nebulaos_load_cell_calibration";
  if (flow == CalFlow::BED_TRAM) return "nebulaos_bed_tram";
  return "nebulaos_calibration";
}

bool cal_prompt_hidden_on_screen(const std::string &title) {
  // G109: "E-Steps Calibration" and "E-Steps Calibration — Confirm"
  return title == "Load Cell Calibration" || title == "Bed Tramming"
         || title == "Input Shaper Calibration"
         || title.rfind("E-Steps Calibration", 0) == 0;
}

std::string cal_shaper_start_command(bool graphs) {
  return std::string("NEBULAOS_INPUT_SHAPER_CALIBRATE GRAPHS=") + (graphs ? "1" : "0");
}

std::string cal_shaper_axis_line(const json &status, const char *axis) {
  if (!status.is_object() || !status.contains("input_shaper_axes")) return "";
  const json &axes = status["input_shaper_axes"];
  if (!axes.is_object() || !axes.contains(axis) || !axes[axis].is_object()) return "";
  const json &a = axes[axis];
  std::string type = str(a, "type");
  double freq = 0;
  if (type.empty() || !num(a, "freq", freq)) return "";
  std::string up(axis);
  for (auto &c : up) c = (char)std::toupper((unsigned char)c);
  char buf[160];
  std::snprintf(buf, sizeof(buf), "%s: %s %.1f Hz", up.c_str(), type.c_str(), freq);
  std::string line = buf;
  // short lines on purpose: LVGL wraps at '.', which would split 0.061
  double vib = 0, smooth = 0, accel = 0;
  if (num(a, "vibrations", vib)) {
    std::snprintf(buf, sizeof(buf), "\n  vibrations %.1f %%", vib);
    line += buf;
  }
  if (num(a, "smoothing", smooth)) {
    std::snprintf(buf, sizeof(buf), "\n  smoothing %.3f", smooth);
    line += buf;
  }
  if (num(a, "max_accel", accel)) {
    std::snprintf(buf, sizeof(buf), "\n  max accel %.0f", accel);
    line += buf;
  }
  return line;
}

// G109: the graphs of the measured axes that have one (X after X, both at the end)
static std::vector<std::pair<std::string, std::string>> shaper_graphs(const json &status,
                                                                      bool only_x) {
  std::vector<std::pair<std::string, std::string>> out;
  if (!status.is_object() || !status.contains("input_shaper_axes")) return out;
  const json &axes = status["input_shaper_axes"];
  if (!axes.is_object()) return out;
  for (const char *axis : {"x", "y"}) {
    if (only_x && std::string(axis) != "x") break;
    if (!axes.contains(axis) || !axes[axis].is_object()) continue;
    std::string png = str(axes[axis], "graph");
    if (!png.empty()) out.push_back({std::string(1, (char)std::toupper((unsigned char)axis[0])), png});
  }
  return out;
}

// G109: why a graph is missing, one line per axis ("" when all are there or
// graphs were off)
std::string cal_shaper_graph_problems(const json &status, bool only_x) {
  std::string t;
  if (!status.is_object() || !status.contains("input_shaper_axes")) return t;
  const json &axes = status["input_shaper_axes"];
  if (!axes.is_object()) return t;
  for (const char *axis : {"x", "y"}) {
    if (only_x && std::string(axis) != "x") break;
    if (!axes.contains(axis) || !axes[axis].is_object()) continue;
    std::string err = str(axes[axis], "graph_error");
    if (!err.empty()) {
      t += std::string(t.empty() ? "" : "\n") + (char)std::toupper((unsigned char)axis[0])
           + " graph not drawn: " + err;
    }
  }
  return t;
}

const char *cal_input_button(CalFlow flow) {
  return flow == CalFlow::LOAD_CELL ? "Weigh" : "Submit";
}

std::string cal_start_command(CalFlow flow) {
  switch (flow) {
    case CalFlow::ESTEPS: return "NEBULAOS_ESTEPS_CALIBRATE";
    case CalFlow::AUTO_CALIBRATE: return "NEBULAOS_AUTO_CALIBRATE";
    case CalFlow::INPUT_SHAPER: return "NEBULAOS_INPUT_SHAPER_CALIBRATE";
    case CalFlow::LOAD_CELL: return "NEBULAOS_LOAD_CELL_CALIBRATE";
    case CalFlow::BED_TRAM: return "NEBULAOS_BED_TRAM";
  }
  return "";
}

std::string cal_cancel_command(CalFlow flow) {
  switch (flow) {
    case CalFlow::ESTEPS: return "_NEBULAOS_ESTEPS_CANCEL";
    case CalFlow::AUTO_CALIBRATE: return "NEBULAOS_CALIBRATION_CANCEL";
    case CalFlow::INPUT_SHAPER: return "_NEBULAOS_INPUT_SHAPER_CANCEL";
    case CalFlow::LOAD_CELL: return "_NEBULAOS_LOAD_CELL_CANCEL";
    case CalFlow::BED_TRAM: return "_NEBULAOS_BED_TRAM_CANCEL";
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

std::string cal_input_command(CalFlow flow, const std::string &text) {
  if (flow == CalFlow::ESTEPS) return cal_measure_command(text);
  if (flow != CalFlow::LOAD_CELL || text.empty()) return "";
  char *end = nullptr;
  double g = std::strtod(text.c_str(), &end);
  if (end == text.c_str() || *end != '\0' || !std::isfinite(g) || g < 1000.0 || g > 5000.0) {
    return "";
  }
  char buf[64];
  std::snprintf(buf, sizeof(buf), "NEBULAOS_LOAD_CELL_CALIBRATE GRAMS=%.1f", g);
  return buf;
}

const char *cal_input_hint(CalFlow flow) {
  return flow == CalFlow::LOAD_CELL ? "weight in grams" : "mm the mark moved";
}

const char *cal_input_problem(CalFlow flow) {
  return flow == CalFlow::LOAD_CELL
    ? "Enter the weight from the scale in grams (1000-5000), e.g. 1531"
    : "Enter the distance in mm, e.g. 98.5";
}

bool cal_input_keep(const CalView &v) {
  return v.needs_measurement || v.can_cancel;
}

CalKeypadOk cal_keypad_ok(CalFlow flow, const std::string &text) {
  CalKeypadOk r;
  // An empty field: OK just closes the keypad.
  if (!text.empty() && cal_input_command(flow, text).empty()) {
    r.close = false;
    r.problem = cal_input_problem(flow);
  }
  return r;
}

static const char *const DESCRIPTION_LOAD_CELL =
  "Teaches the nozzle touch sensor (load cell) how much force it measures. "
  "Takes about 2 minutes.\n"
  "You need a 1.5 L bottle of water and a kitchen scale. Nothing moves or "
  "heats.";
static const char *const DESCRIPTION_BED_TRAM =
  "Measures the four bed corners and tells you which one to correct, one "
  "at a time: a printed shim on rigid spacers, or a screw turn on rubber or "
  "spring mounts. The printer homes and probes; the bed must be empty.";
static const char *const DESCRIPTION_ESTEPS =
  "Checks how much filament the extruder really pushes and corrects it.\n"
  "The hotend heats; you mark the filament at the extruder entry, the printer "
  "extrudes a fixed length, and you enter how far the mark moved.";
static const char *const DESCRIPTION_AUTO =
  "Load cell check, nozzle clean, Z offset, bed mesh - about 7 min, no tools "
  "needed. With the load cell in Automatic mode it checks the load cell "
  "first (no bottle needed). Saves everything with one Klipper restart and "
  "resets the Z adjustment to 0.\n"
  "The bed must be empty.";
static const char *const DESCRIPTION_SHAPER =
  "Measures resonances with the accelerometer: first mounted on the toolhead "
  "(X), then on the bed (Y). Saves the result with one Klipper restart.\n"
  "Have the accelerometer ready.";

static std::string fmt(const char *f, double a) {
  char buf[96];
  std::snprintf(buf, sizeof(buf), f, a);
  return buf;
}

// The four corners as the bed lies in front of you (rear on top), G89.
// Run 17 (Tim): SIGNED values, so a rigid mount reads as "lower than the
// highest corner" and not as a height:
//   Rear:   left -0.05 OK    right  0.00 highest
//   Front:  left -0.23 ADJUST    right -0.12 later
// The extensions report signed_deviations + reference (rigid: vs the highest
// corner; adjustable: vs the base corner, + higher, - lower). Older extensions
// only have the unsigned deviations: for a rigid mount those are "how far below
// the highest corner", so they are shown negated; for an adjustable mount the
// direction is unknown and the plain distance is shown.
static int corner_index(const json &ref, const char *const names[]) {
  if (ref.is_number_integer()) {
    int i = ref.get<int>();
    return i >= 0 && i < 4 ? i : -1;
  }
  if (!ref.is_string()) return -1;
  std::string r = ref.get<std::string>();
  for (auto &c : r) c = (char)std::tolower((unsigned char)c);
  for (int i = 0; i < 4; i++) {
    std::string n = names[i];
    for (auto &c : n) c = (char)std::tolower((unsigned char)c);
    if (n == r) return i;
  }
  return -1;
}

static std::string tram_corners(const json &st) {
  static const char *const names[] = {"Front left", "Front right", "Rear right", "Rear left"};
  const json &ok = st.contains("accepted") ? st["accepted"] : json();
  std::string target = str(st, "target");
  bool rigid = str(st, "mount") != "adjustable";
  const json empty;
  const json &sdev = st.contains("signed_deviations") ? st["signed_deviations"] : empty;
  const json &udev = st.contains("deviations") ? st["deviations"] : empty;
  bool is_signed = sdev.is_array() && sdev.size() == 4;
  const json &dev = is_signed ? sdev : udev;
  if (!dev.is_array() || dev.size() != 4) return "";
  double v[4];
  for (int i = 0; i < 4; i++) {
    v[i] = dev[i].is_number() ? dev[i].get<double>() : 0;
    if (!is_signed && rigid) v[i] = -v[i];    // unsigned rigid = below the highest
  }
  bool show_sign = is_signed || rigid;
  int ref = is_signed && st.contains("reference") ? corner_index(st["reference"], names) : -1;
  if (ref < 0 && show_sign && rigid) {
    // older extensions: the highest corner is the one at 0
    for (int i = 0; i < 4 && ref < 0; i++) {
      if (std::fabs(v[i]) < 0.0005) ref = i;
    }
  }
  auto lower = [](std::string x) {
    for (auto &c : x) c = (char)std::tolower((unsigned char)c);
    return x;
  };
  auto value = [&](double d) {
    if (!show_sign) return fmt("%.2f", d);
    if (std::fabs(d) < 0.005) return std::string(" 0.00");
    return fmt("%+.2f", d);
  };
  auto cell = [&](int i) {
    std::string state;
    if (i == ref) {
      state = rigid ? "highest" : "base";
    } else if (ok.is_array() && ok.size() == 4 && ok[i].is_boolean() && ok[i].get<bool>()) {
      state = "OK";
    } else if (!target.empty()) {
      state = lower(names[i]) == lower(target) ? "ADJUST" : "later";
    }
    return std::string(i == 0 || i == 3 ? "left " : "right ") + value(v[i])
           + (state.empty() ? "" : " " + state);
  };
  std::string head = !show_sign ? "Corners (mm), front = the side with the screen:"
    : rigid ? "Corners in mm vs the highest corner.\nFront = the side with the screen."
    : "Corners in mm vs the base corner (+ higher).\nFront = the side with the screen.";
  return head + "\n"
         "Rear:   " + cell(3) + "    " + cell(2) + "\n"
         "Front:  " + cell(0) + "    " + cell(1);
}

static CalView bed_tram_view(CalView v, const json &st, bool printing) {
  std::string state = str(st, "state");
  std::string mount = str(st, "mount");
  std::string message = str(st, "message");
  const char *rigid = "NEBULAOS_BED_TRAM MOUNT=rigid";
  const char *adjustable = "NEBULAOS_BED_TRAM MOUNT=adjustable";
  std::string mount_line = mount == "rigid" ? "Bed mount: rigid spacers (shims)."
    : mount == "adjustable" ? "Bed mount: rubber or springs (screw turns)." : "";
  if (state == "awaiting_mount") {
    v.text = "How is the bed mounted?\nRigid spacers: the stock KE (no knobs).\n"
             "Rubber or springs: a common upgrade with screws you turn.";
    v.actions = {{"Rigid spacers", rigid}, {"Rubber or springs", adjustable}};
    v.can_cancel = true;
  } else if (state == "awaiting_adjustment") {
    v.text = tram_corners(st) + "\n\n" + message;
    v.actions = {{"Done - measure again", "NEBULAOS_BED_TRAM CONFIRM=1"}};
    v.can_cancel = true;
  } else if (state == "complete" || state == "stopped") {
    std::string head = state == "complete" ? message : "Stopped: " + message;
    v.text = head + "\n\n" + tram_corners(st);
    if (state == "complete" && st.contains("load_cell_corner_changed")
        && st["load_cell_corner_changed"].is_boolean() && st["load_cell_corner_changed"].get<bool>()) {
      v.text += "\n\nThe front-left corner (load cell) was changed: calibrate the load cell next.";
    }
    v.can_start = !printing;
  } else {
    v.text = std::string(DESCRIPTION_BED_TRAM) + (mount_line.empty() ? "" : "\n" + mount_line);
    if (state == "cancelled") v.text = "Cancelled.\n" + v.text;
    if (state == "error") v.text = "Failed: " + str(st, "error") + "\n" + v.text;
    v.can_start = !printing;
    if (!mount.empty() && !printing) {
      // change the remembered mount (only between runs)
      v.actions = {{mount == "rigid" ? "Switch to rubber/springs" : "Switch to rigid spacers",
                    mount == "rigid" ? adjustable : rigid}};
    }
  }
  if (printing && v.can_start) {
    v.text += "\n\nNot available while printing.";
  }
  return v;
}

// ---- G99 (Run 18): load cell Automatic (default) / Manual ----------------
// Extensions 2c099f1: nebulaos_load_cell_calibration reports mode (effective),
// mode_choice (the persisted choice), auto_check never|ok|failed,
// auto_check_reason, auto_ready. Without 'mode' (older extensions, or no load
// cell probe: all None) the page is the bottle flow as before.

const char *const CAL_AUTO_CHECK_COMMAND = "NEBULAOS_LOAD_CELL_AUTO_CHECK";

std::string cal_mode_command(bool automatic) {
  return std::string("NEBULAOS_LOAD_CELL_MODE MODE=") + (automatic ? "auto" : "manual");
}

static const char *const AUTO_INTRO =
  "Automatic (default): the printer checks the load cell by itself on an "
  "empty bed - no bottle, nothing moves or heats. Auto Calibration runs "
  "this check too.";

static void load_cell_modes(CalView &v, const json &status, bool printing) {
  std::string mode = str(status, "mode");
  if (mode != "auto" && mode != "manual") {
    return;   // older extensions: the bottle flow only
  }
  v.has_modes = true;
  std::string choice = str(status, "mode_choice");
  if (choice != "auto" && choice != "manual") choice = mode;
  v.mode_auto = choice == "auto";
  // the choice is part of the idle page; a running bottle flow shows its steps
  v.show_modes = !v.can_cancel;
  if (!v.show_modes) {
    return;
  }
  if (!v.mode_auto) {
    // Manual: today's bottle flow page
    if (mode != choice) {
      v.text += "\n\nNote: the printer configuration forces automatic mode.";
    }
    return;
  }
  v.can_start = false;
  v.actions.clear();
  std::string check = str(status, "auto_check");
  bool ready = status.contains("auto_ready") && status["auto_ready"].is_boolean()
               && status["auto_ready"].get<bool>();
  std::string reason = str(status, "auto_check_reason");
  std::string t = AUTO_INTRO;
  bool failed = check == "failed";
  if (failed) {
    t += "\n\nAutomatic check FAILED";
    t += reason.empty() ? std::string(".") : ": " + reason + ".";
    t += "\nRemove everything from the bed and tap Run again, or use Manual "
         "(bottle calibration).";
  } else if (check == "ok" && ready) {
    t += "\n\nReady - Automatic needs no weight calibration; Auto Calibration uses it.";
  } else if (check == "ok") {
    t += "\n\nLast check passed, but automatic contact is not available "
         "right now. Tap Run to check again.";
  } else {
    t += "\n\nNot checked yet. Remove everything from the bed, then tap Run.";
  }
  if (mode != choice) {
    t += "\n\nNote: the printer configuration forces manual mode.";
  }
  v.text = t;
  if (!printing) {
    v.actions = {{"Run", CAL_AUTO_CHECK_COMMAND}};
    if (failed) {
      v.actions.push_back({"Use Manual", cal_mode_command(false)});
    }
  }
}

void cal_auto_check_running(CalView &v) {
  v.text = std::string(AUTO_INTRO) + "\n\nChecking the load cell (a few seconds) - "
           "do not touch the printer...";
  v.actions.clear();
  v.can_start = false;
}

// G118: the extensions keep input_shaper_state at awaiting_x_mount /
// awaiting_y_mount while that axis is measured (only stage / phase move on),
// so the mount step's text and Continue must not show then.
static bool shaper_measuring(const json &status, const std::string &state,
                             const std::string &stage) {
  std::string phase = str(status, "input_shaper_phase");
  if (phase == "measuring" || phase == "calculating" || phase == "graph") return true;
  if (state == "awaiting_x_mount") return stage == "measure_x";
  if (state == "awaiting_y_mount") {
    return stage == "measure_y" || stage == "final_validation" || stage == "commit"
           || stage == "restart";
  }
  return false;
}

CalView cal_view(CalFlow flow, const json &status, bool printing) {
  CalView v;
  switch (flow) {
    case CalFlow::ESTEPS: v.title = "E-Steps Calibration"; break;
    case CalFlow::AUTO_CALIBRATE: v.title = "Auto Calibration"; break;
    case CalFlow::INPUT_SHAPER: v.title = "Input Shaper"; break;
    case CalFlow::LOAD_CELL: v.title = "Load Cell Calibration"; break;
    case CalFlow::BED_TRAM: v.title = "Bed Tramming"; break;
  }
  if (!status.is_object()) {
    v.text = "NebulaOS calibration is not available on this printer.";
    return v;
  }
  v.available = true;
  if (flow == CalFlow::BED_TRAM) {
    return bed_tram_view(v, status, printing);
  }
  const std::string p = prefix(flow);
  std::string state = str(status, p + "state");
  std::string stage = str(status, p + "stage");
  std::string error = str(status, p + "error");
  const char *description = flow == CalFlow::ESTEPS ? DESCRIPTION_ESTEPS
    : flow == CalFlow::AUTO_CALIBRATE ? DESCRIPTION_AUTO
    : flow == CalFlow::LOAD_CELL ? DESCRIPTION_LOAD_CELL : DESCRIPTION_SHAPER;

  if (state.empty() || state == "idle") {
    v.text = description;
    v.can_start = !printing;
  } else if (state == "running") {
    v.text = stage.empty() ? std::string("Running...") : "Running: " + stage;
    // E-steps cannot be cancelled while it heats; the others can.
    v.can_cancel = flow != CalFlow::ESTEPS;
  } else if (state == "awaiting_empty") {
    // G90: the load cell is a page flow - its buttons are on this page
    v.text = "Step 1 of 5: remove everything from the bed (the nozzle must not "
             "touch it), then tap Check.";
    v.can_cancel = true;
  } else if (state == "awaiting_weight") {
    v.text = flow == CalFlow::LOAD_CELL
      ? "Step 2 of 5: put the weighed 1.5 L bottle with its centre roughly "
        "above the front-left screw head of the bed, then enter the weight "
        "from the kitchen scale and tap Weigh."
      : "Enter the weight in grams.";
    v.can_cancel = true;
    v.needs_measurement = true;
  } else if (state == "awaiting_replace") {
    v.text = "Step 3 of 5: lift the bottle and put it back in the same place, "
             "then tap Continue.";
    v.can_cancel = true;
  } else if (state == "awaiting_remove") {
    v.text = "Step 4 of 5: remove the bottle from the bed, then tap Continue.";
    v.can_cancel = true;
  } else if (state == "awaiting_finger") {
    v.text = "Step 5 of 5 (optional): press the front-left corner gently with "
             "a finger and tap Measure, or tap Continue.";
    v.can_cancel = true;
  } else if (state == "awaiting_continue") {
    // G109: the step's button is on this page (no popup on the screen)
    v.text = "The hotend is hot. Mark the filament where it enters the "
             "extruder, then tap Start Extrusion.";
    v.actions = {{"Start Extrusion", "_NEBULAOS_ESTEPS_CALIBRATE CONTINUE=1"}};
    v.can_cancel = true;
  } else if (state == "awaiting_measurement") {
    v.text = "Measure how far the mark moved and enter it in mm.";
    v.can_cancel = true;
    v.needs_measurement = true;
  } else if (state == "awaiting_confirmation") {
    double before = 0, after = 0, measured = 0, commanded = 0;
    if (flow == CalFlow::ESTEPS && num(status, "esteps_old_rotation_distance", before)
        && num(status, "esteps_new_rotation_distance", after)) {
      char buf[200];
      if (num(status, "esteps_measured_length", measured)
          && num(status, "esteps_commanded_length", commanded)) {
        std::snprintf(buf, sizeof(buf), "Measured %.1f mm of %.0f mm.\n", measured, commanded);
        v.text = buf;
      }
      std::snprintf(buf, sizeof(buf), "rotation_distance %.4f -> %.4f\n\n", before, after);
      v.text += buf;
      v.text += "Apply & Save restarts Klipper.";
    } else {
      v.text = "Check the new value and tap Apply & Save.";
    }
    if (flow == CalFlow::ESTEPS) {
      v.actions = {{"Apply & Save", "_NEBULAOS_ESTEPS_CONFIRM"}};
    }
    v.can_cancel = true;
  } else if (flow == CalFlow::INPUT_SHAPER && shaper_measuring(status, state, stage)) {
    // the step list shows the live phase; a running SHAPER_CALIBRATE cannot
    // be interrupted, so no Cancel either
    v.text = "";
  } else if (state == "awaiting_x_mount") {
    v.text = "Mount the accelerometer firmly on the toolhead, then tap "
             "Continue.";
    v.actions = {{"Continue", "_NEBULAOS_INPUT_SHAPER_CALIBRATE CONTINUE=1"}};
    v.can_cancel = true;
  } else if (state == "awaiting_y_mount") {
    std::string x = cal_shaper_axis_line(status, "x");
    std::string problem = cal_shaper_graph_problems(status, true);
    v.text = (x.empty() ? std::string() : x + "\n")
             + (problem.empty() ? std::string() : problem + "\n")
             + (x.empty() && problem.empty() ? "" : "\n")
             + "Move the accelerometer firmly onto the bed, then tap Continue.";
    v.actions = {{"Continue", "_NEBULAOS_INPUT_SHAPER_CALIBRATE CONTINUE=1"}};
    v.graphs = shaper_graphs(status, true);
    v.can_cancel = true;
  } else if (state == "commit_requested" || state == "restart_pending"
             || state == "verification_pending") {
    v.text = "Saving and restarting Klipper...";
  } else if (state == "complete") {
    v.text = "Done - the result is saved.";
    double cpg = 0;
    if (flow == CalFlow::LOAD_CELL && num(status, "counts_per_gram", cpg)) {
      char buf[128];
      std::snprintf(buf, sizeof(buf), "Done - %.3f counts per gram, saved.", cpg);
      v.text = buf;
    }
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
  if (flow == CalFlow::LOAD_CELL) {
    const char *cmd = "NEBULAOS_LOAD_CELL_CALIBRATE";
    double a = 0, b = 0;
    if (state == "awaiting_empty") {
      v.actions = {{"Check", std::string(cmd) + " CHECK=1"}};
    } else if (state == "awaiting_replace") {
      v.actions = {{"Continue", std::string(cmd) + " NEXT=1"}};
    } else if (state == "awaiting_remove") {
      if (num(status, "replace_grams", a) && num(status, "grams", b)) {
        v.text = fmt("Printer measures %.0f g, ", a) + fmt("your scale said %.0f g.\n\n", b) + v.text;
      }
      v.actions = {{"Continue", std::string(cmd) + " NEXT=1"}};
    } else if (state == "awaiting_finger") {
      if (num(status, "finger_grams", a)) {
        v.text += fmt("\nPrinter measures %.0f g.", a);
      }
      v.actions = {{"Measure", std::string(cmd) + " MEASURE=1"},
                   {"Continue", std::string(cmd) + " NEXT=1"}};
    } else if (state == "awaiting_confirmation") {
      v.text = "Calibration done";
      if (num(status, "counts_per_gram", a)) v.text += fmt(": %.2f counts per gram", a);
      v.text += ".";
      if (num(status, "old_counts_per_gram", a) && num(status, "change_percent", b)) {
        v.text += fmt("\nPrevious calibration: %.2f counts per gram", a) + fmt(" (%+.0f%%).", b);
      }
      if (status.contains("change_warning") && status["change_warning"].is_boolean()
          && status["change_warning"].get<bool>()) {
        v.text += "\nThis is very different from before. Only save if the bottle stood "
                  "right and its weight is correct.";
      }
      v.text += "\nApply & Save restarts Klipper.";
      v.actions = {{"Apply & Save", std::string(cmd) + " ACCEPT=1"}};
    }
  }
  if (flow == CalFlow::LOAD_CELL) {
    load_cell_modes(v, status, printing);
  }
  std::string problem = str(status, p + "problem");
  if (flow == CalFlow::LOAD_CELL && v.can_cancel && !problem.empty()) {
    v.text = "Problem: " + problem + "\n\n" + v.text;
  }
  if (flow == CalFlow::INPUT_SHAPER) {
    // G109: the switch only between runs; the last run's graphs on its end page
    v.graph_switch = v.can_start;
    if (state == "complete") {
      v.graphs = shaper_graphs(status, false);
    }
  }
  if (printing && (v.can_start || state.empty() || state == "idle"
                   || (v.show_modes && v.mode_auto))) {
    v.text += "\n\nNot available while printing.";
  }
  return v;
}
