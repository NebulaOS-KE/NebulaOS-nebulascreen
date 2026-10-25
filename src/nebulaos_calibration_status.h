#ifndef __NEBULAOS_CALIBRATION_STATUS_H__
#define __NEBULAOS_CALIBRATION_STATUS_H__

#include "hv/json.hpp"
#include "nebulaos_calibration_flow.h"

#include <string>
#include <vector>

using json = nlohmann::json;

// Pure, LVGL-free logic for the Run 19 calibration feedback (G104), the
// Calibration menu status lines (G105) and the saved bed mesh view (G106).
// Interfaces (PLAN Run 19, fixed): nebulaos_calibration.get_status() adds
//   progress:    null | {flow, step, total, key, label, detail}
//   last_result: null | {flow, ok, finished_at, summary[], error_step, error, advice}
//   history:     {key: {done_at: epoch|null, summary, redo: reason|null}}
// and the command NEBULAOS_CALIBRATION_RECORD FLOW=<key> SUMMARY="<text>".
// Older extensions report none of these: the screen then shows descriptions
// only and never sends the record command.
//
// This file may be distributed under the terms of the GNU GPLv3 license.

// ---- G105: Calibration menu ---------------------------------------------
struct CalMenuEntry {
  const char *key;          // history key
  const char *title;        // row label
  const char *description;  // one line + duration, shown when not done
};
// 1-7, the order an initial setup follows
const std::vector<CalMenuEntry> &cal_menu_in_order();
// the OTHER section
const std::vector<CalMenuEntry> &cal_menu_other();

enum class CalRowState { DESCRIPTION, DONE, REDO };
struct CalRowStatus {
  CalRowState state = CalRowState::DESCRIPTION;
  std::string text;
};
// The row's status line from nebulaos_calibration.history (a missing or
// malformed history / entry -> the description).
CalRowStatus cal_row_status(const json &history, const CalMenuEntry &e);
// "24 Oct" (local time), "" for a non-positive epoch
std::string cal_short_date(double epoch);

// Newer extensions (with history) accept NEBULAOS_CALIBRATION_RECORD.
bool cal_history_supported(const json &calibration_status);
// NEBULAOS_CALIBRATION_RECORD FLOW=<key> SUMMARY="<text>" (quotes and line
// breaks removed from the text, at most 80 characters).
std::string cal_record_command(const std::string &key, const std::string &summary);
// The G-code lines to put before a screen-driven SAVE_CONFIG: the record
// command with newer extensions, "" with older ones (an unknown command would
// stop the script before SAVE_CONFIG).
std::string cal_record_prefix(const json &calibration_status, const std::string &key,
                              const std::string &summary);
// Axis Twist "Save" (G105): the saved mesh was probed without the new twist
// correction - remove it in the same restart.
std::string cal_axis_twist_save_script(const json &calibration_status);

// ---- G105: Heaters (PID) page --------------------------------------------
enum class PidHeater { BED, HOTEND };
int pid_default_target(PidHeater h);                     // 65 / 230
std::string pid_command(PidHeater h);                    // PID_CALIBRATE HEATER=... TARGET=...
// After a successful PID run: the record (newer extensions) and SAVE_CONFIG.
std::string pid_save_script(const json &calibration_status, PidHeater h);
std::string pid_summary(PidHeater h);                    // "Bed tuned at 65 C"
extern const char *const PID_DESCRIPTION;
// While PID_CALIBRATE runs: what, how long, the live temperature from the
// heater's status object (heater_bed / extruder; {} -> no temperature line).
std::string pid_running_text(PidHeater h, const json &heater_status);

// ---- G104: guided page feedback ------------------------------------------
// "auto_calibrate" / "input_shaper" / "esteps" / "load_cell" / "bed_tram"
const char *cal_flow_key(CalFlow flow);
// The flow's user-facing steps (mirror of the extensions' *_STEPS tables).
const std::vector<std::string> &cal_flow_steps(CalFlow flow);

enum class CalStepState { DONE, CURRENT, NEXT };
struct CalStepRow {
  std::string label;
  CalStepState state;
};
struct CalProgressView {
  bool active = false;
  int step = 0;
  int total = 0;
  std::string counter;     // "Step 7 of 10"
  std::string label;       // "Measuring the Z offset"
  std::string detail;      // "touch 2 of 3" / temperatures / ""
  bool restarting = false; // the save step: Klipper restarts
  std::vector<CalStepRow> steps;
};
// From nebulaos_calibration.progress, only when it belongs to this flow.
CalProgressView cal_progress_view(CalFlow flow, const json &calibration_status);
extern const char *const CAL_RESTART_NOTICE;
extern const char *const CAL_RECONNECT_NOTICE;

struct CalResultView {
  bool present = false;
  bool ok = false;
  std::string headline;              // "Done 24 Oct" / "Failed: <step>"
  std::vector<std::string> lines;    // summary lines, or why + what to do
};
// From nebulaos_calibration.last_result, only when it belongs to this flow.
CalResultView cal_result_view(CalFlow flow, const json &calibration_status);

// What the guided page shows for a NebulaOS flow (E-steps, Auto Calibration,
// Input Shaper): the step list while it runs, the end / failure screen after
// it. Adjusts v.text: empty while a plain "running" step is shown by the
// step list, the restart notice during the save (the reconnect notice while
// Klipper is away), the result lines once it is over. Load cell and bed
// tramming have their own pages: nothing changes.
struct CalFeedback {
  CalProgressView progress;
  CalResultView result;   // present only when it is shown
};
CalFeedback cal_feedback(CalFlow flow, const json &status, bool klippy_ready, CalView &v);

// ---- G106: Bed Mesh page -------------------------------------------------
// The saved profile to show when no mesh is active: "default" if it has
// points, otherwise the first (by name) with points; "" when none.
std::string bedmesh_saved_profile(const json &profiles);
// profiles.<name>.points as rows, empty when malformed
std::vector<std::vector<double>> bedmesh_profile_points(const json &profiles, const std::string &name);
std::string bedmesh_saved_notice(const std::string &name);
extern const char *const BEDMESH_NONE_TEXT;

#endif // __NEBULAOS_CALIBRATION_STATUS_H__
