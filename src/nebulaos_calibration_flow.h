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
};

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

#endif // __NEBULAOS_CALIBRATION_FLOW_H__
