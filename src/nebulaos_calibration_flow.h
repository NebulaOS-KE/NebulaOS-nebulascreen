#ifndef __NEBULAOS_CALIBRATION_FLOW_H__
#define __NEBULAOS_CALIBRATION_FLOW_H__

#include "hv/json.hpp"

#include <string>
#include <utility>
#include <vector>

using json = nlohmann::json;

// Pure, LVGL-free logic for the NebulaOS guided calibrations (decisions D3-D5,
// 2026-10-10). The workflows themselves live in NebulaOS-klipper-extensions
// (extras/nebulaos_calibration.py) and drive the operator through Klipper
// action:prompt dialogs, which GuppyScreen's PromptPanel already shows. A
// GuidedCalibrationPanel only starts a workflow, shows its live state from the
// `nebulaos_calibration` object (already in printer_state through the all-objects
// subscription), offers Cancel while the workflow can be cancelled, and - for
// E-steps - takes the one measured length the workflow asks for.
//
// This file may be distributed under the terms of the GNU GPLv3 license.

enum class CalFlow {
  ESTEPS,          // NEBULAOS_ESTEPS_CALIBRATE: heat, mark, extrude, measure, confirm
  AUTO_CALIBRATE,  // NEBULAOS_AUTO_CALIBRATE: nozzle clean, Z offset, bed mesh, one SAVE_CONFIG
  INPUT_SHAPER,    // NEBULAOS_INPUT_SHAPER_CALIBRATE: sensor on toolhead (X), then bed (Y)
  LOAD_CELL,       // NEBULAOS_LOAD_CELL_CALIBRATE: water bottle, 5 steps, save (G86/G90)
  BED_TRAM,        // NEBULAOS_BED_TRAM: four corners, one instruction at a time (G87/G89)
};

struct CalView {
  bool available = false;          // nebulaos_calibration object present
  std::string title;
  std::string text;                // description when idle, otherwise the live state
  bool can_start = false;
  bool can_cancel = false;
  bool needs_measurement = false;  // E-steps: measured length; load cell: grams
  // G89/G90: the step's own buttons on this page (label, G-code), so the
  // screen never needs the Mainsail-style action prompt for these flows.
  std::vector<std::pair<std::string, std::string>> actions;
  // G99 (Run 18): Calibration > Load Cell offers Automatic (default) / Manual.
  // Only with extensions that report 'mode' (older ones: the bottle flow only).
  bool has_modes = false;
  bool show_modes = false;   // the choice is shown (the bottle flow is not running)
  bool mode_auto = false;    // the selected mode: automatic
};

// G99: the load-cell mode commands and the automatic check.
std::string cal_mode_command(bool automatic);   // NEBULAOS_LOAD_CELL_MODE MODE=auto|manual
extern const char *const CAL_AUTO_CHECK_COMMAND; // NEBULAOS_LOAD_CELL_AUTO_CHECK
// While the screen waits for the automatic check's answer (a few seconds,
// nothing moves): what the page says instead.
void cal_auto_check_running(CalView &v);

CalView cal_view(CalFlow flow, const json &status, bool printing);

// The Klipper status object a flow reports in: the load cell and bed
// tramming have their own (nebulaos_load_cell_calibration, nebulaos_bed_tram),
// the others share nebulaos_calibration.
const char *cal_status_object(CalFlow flow);

// G89/G90: action prompts with this title are shown by the flow's own page
// on the screen, not as a popup (Mainsail still shows them as prompts).
bool cal_prompt_hidden_on_screen(const std::string &title);

// The label of the measurement row's button ("Weigh" for the load cell).
const char *cal_input_button(CalFlow flow);

std::string cal_start_command(CalFlow flow);
std::string cal_cancel_command(CalFlow flow);

// E-steps measurement: "" when the text is not a usable length in mm (0 < mm <= 500),
// otherwise the command that submits it.
std::string cal_measure_command(const std::string &text);

// The one number a flow asks for: E-steps mm (above), load cell grams
// (1000 <= g <= 5000, a weighed 1.5 L bottle). "" when not usable or the flow takes no number.
std::string cal_input_command(CalFlow flow, const std::string &text);
// Placeholder / hint for that entry field.
const char *cal_input_hint(CalFlow flow);
// What the screen says when the typed number is not usable.
const char *cal_input_problem(CalFlow flow);

// G94 (Run 18, Tim's Run 17 test): the typed number stays in the field. The
// keypad's OK key commits it (closes the keypad, the value stays for Weigh /
// Submit); the Weigh / Submit button sends the typed value and leaves it in
// the field, so a retry after a problem ("not stable", ...) needs no retyping.
// The field is cleared only when the flow is over (idle, done, cancelled,
// failed): cal_input_keep() is false then.
bool cal_input_keep(const CalView &v);
struct CalKeypadOk {
  bool close = true;    // close the keypad, keep the value
  std::string problem;  // non-empty: the value is not usable; keypad stays open
};
CalKeypadOk cal_keypad_ok(CalFlow flow, const std::string &text);

#endif // __NEBULAOS_CALIBRATION_FLOW_H__
