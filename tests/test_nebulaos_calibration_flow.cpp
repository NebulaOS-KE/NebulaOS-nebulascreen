// Offline tests for the NebulaOS guided calibration panel logic (D3-D5) - no
// LVGL, no Klipper. Build/run:
//   make test-nebulaos-calibration-flow
//
// This file may be distributed under the terms of the GNU GPLv3 license.
#include "minitest.h"
#include "../src/nebulaos_calibration_flow.h"

using json = nlohmann::json;

static json idle() {
  return {{"esteps_state", "idle"}, {"auto_calibrate_state", "idle"},
          {"input_shaper_state", "idle"}};
}

TEST(Commands, start_and_cancel_names) {
  ASSERT_EQ(cal_start_command(CalFlow::ESTEPS), std::string("NEBULAOS_ESTEPS_CALIBRATE"));
  ASSERT_EQ(cal_start_command(CalFlow::AUTO_CALIBRATE), std::string("NEBULAOS_AUTO_CALIBRATE"));
  ASSERT_EQ(cal_start_command(CalFlow::INPUT_SHAPER), std::string("NEBULAOS_INPUT_SHAPER_CALIBRATE"));
  ASSERT_EQ(cal_cancel_command(CalFlow::ESTEPS), std::string("_NEBULAOS_ESTEPS_CANCEL"));
  ASSERT_EQ(cal_cancel_command(CalFlow::AUTO_CALIBRATE), std::string("NEBULAOS_CALIBRATION_CANCEL"));
  ASSERT_EQ(cal_cancel_command(CalFlow::INPUT_SHAPER), std::string("_NEBULAOS_INPUT_SHAPER_CANCEL"));
}

TEST(Commands, measurement) {
  ASSERT_EQ(cal_measure_command("98.5"), std::string("NEBULAOS_ESTEPS_CALIBRATE MEASURED=98.50"));
  ASSERT_EQ(cal_measure_command("100"), std::string("NEBULAOS_ESTEPS_CALIBRATE MEASURED=100.00"));
  ASSERT_EQ(cal_measure_command(""), std::string(""));
  ASSERT_EQ(cal_measure_command("0"), std::string(""));
  ASSERT_EQ(cal_measure_command("-5"), std::string(""));
  ASSERT_EQ(cal_measure_command("9.8.1"), std::string(""));
  ASSERT_EQ(cal_measure_command("1e9"), std::string(""));
  ASSERT_EQ(cal_measure_command("abc"), std::string(""));
}

TEST(View, no_object_is_unavailable) {
  auto v = cal_view(CalFlow::ESTEPS, json(), false);
  ASSERT_FALSE(v.available);
  ASSERT_FALSE(v.can_start);
}

TEST(View, idle_offers_start_with_description) {
  for (auto f : {CalFlow::ESTEPS, CalFlow::AUTO_CALIBRATE, CalFlow::INPUT_SHAPER}) {
    auto v = cal_view(f, idle(), false);
    ASSERT_TRUE(v.available);
    ASSERT_TRUE(v.can_start);
    ASSERT_FALSE(v.can_cancel);
    ASSERT_TRUE(v.text.size() > 40);
  }
}

TEST(View, printing_blocks_start) {
  auto v = cal_view(CalFlow::AUTO_CALIBRATE, idle(), true);
  ASSERT_FALSE(v.can_start);
  ASSERT_TRUE(v.text.find("printing") != std::string::npos);
}

TEST(View, esteps_steps) {
  json s = idle();
  s["esteps_state"] = "running";
  ASSERT_FALSE(cal_view(CalFlow::ESTEPS, s, false).can_cancel);  // heating: backend refuses cancel
  s["esteps_state"] = "awaiting_continue";
  ASSERT_TRUE(cal_view(CalFlow::ESTEPS, s, false).can_cancel);
  s["esteps_state"] = "awaiting_measurement";
  auto v = cal_view(CalFlow::ESTEPS, s, false);
  ASSERT_TRUE(v.needs_measurement);
  ASSERT_TRUE(v.can_cancel);
  ASSERT_FALSE(v.can_start);
  s["esteps_state"] = "awaiting_confirmation";
  ASSERT_FALSE(cal_view(CalFlow::ESTEPS, s, false).needs_measurement);
}

TEST(View, esteps_complete_shows_values) {
  json s = idle();
  s["esteps_state"] = "complete";
  s["esteps_old_rotation_distance"] = 7.71;
  s["esteps_new_rotation_distance"] = 7.5945;
  auto v = cal_view(CalFlow::ESTEPS, s, false);
  ASSERT_TRUE(v.text.find("7.7100 -> 7.5945") != std::string::npos);
  ASSERT_TRUE(v.can_start);
}

TEST(View, auto_calibrate_running_cancellable_with_stage) {
  json s = idle();
  s["auto_calibrate_state"] = "running";
  s["auto_calibrate_stage"] = "bed_mesh";
  auto v = cal_view(CalFlow::AUTO_CALIBRATE, s, false);
  ASSERT_TRUE(v.can_cancel);
  ASSERT_TRUE(v.text.find("bed_mesh") != std::string::npos);
}

TEST(View, commit_states_offer_nothing) {
  for (auto st : {"commit_requested", "restart_pending", "verification_pending"}) {
    json s = idle();
    s["input_shaper_state"] = st;
    auto v = cal_view(CalFlow::INPUT_SHAPER, s, false);
    ASSERT_FALSE(v.can_start);
    ASSERT_FALSE(v.can_cancel);
  }
}

TEST(View, input_shaper_mount_steps) {
  json s = idle();
  s["input_shaper_state"] = "awaiting_x_mount";
  ASSERT_TRUE(cal_view(CalFlow::INPUT_SHAPER, s, false).text.find("toolhead") != std::string::npos);
  s["input_shaper_state"] = "awaiting_y_mount";
  ASSERT_TRUE(cal_view(CalFlow::INPUT_SHAPER, s, false).text.find("bed") != std::string::npos);
}

TEST(View, error_shows_message_and_allows_retry) {
  json s = idle();
  s["auto_calibrate_state"] = "error";
  s["auto_calibrate_error"] = "probe failed";
  auto v = cal_view(CalFlow::AUTO_CALIBRATE, s, false);
  ASSERT_TRUE(v.text.find("probe failed") != std::string::npos);
  ASSERT_TRUE(v.can_start);
}

TEST(View, other_flows_state_does_not_leak) {
  json s = idle();
  s["esteps_state"] = "awaiting_measurement";
  ASSERT_FALSE(cal_view(CalFlow::INPUT_SHAPER, s, false).needs_measurement);
  ASSERT_TRUE(cal_view(CalFlow::INPUT_SHAPER, s, false).can_start);
}

TEST(LoadCell, commands_and_input) {
  ASSERT_EQ(cal_start_command(CalFlow::LOAD_CELL), std::string("NEBULAOS_LOAD_CELL_CALIBRATE"));
  ASSERT_EQ(cal_cancel_command(CalFlow::LOAD_CELL), std::string("_NEBULAOS_LOAD_CELL_CANCEL"));
  ASSERT_EQ(cal_input_command(CalFlow::LOAD_CELL, "1531"),
            std::string("NEBULAOS_LOAD_CELL_CALIBRATE GRAMS=1531.0"));
  // G86: only a weighed bottle-sized weight; a typo like 150 is refused here
  ASSERT_EQ(cal_input_command(CalFlow::LOAD_CELL, "150"), std::string(""));
  ASSERT_EQ(cal_input_command(CalFlow::LOAD_CELL, "999"), std::string(""));
  ASSERT_EQ(cal_input_command(CalFlow::LOAD_CELL, "5001"), std::string(""));
  ASSERT_EQ(cal_input_command(CalFlow::LOAD_CELL, "abc"), std::string(""));
  ASSERT_EQ(cal_input_command(CalFlow::ESTEPS, "98.5"),
            std::string("NEBULAOS_ESTEPS_CALIBRATE MEASURED=98.50"));
  ASSERT_EQ(cal_input_command(CalFlow::INPUT_SHAPER, "5"), std::string(""));
  ASSERT_EQ(std::string(cal_input_hint(CalFlow::LOAD_CELL)), std::string("weight in grams"));
}

TEST(LoadCell, own_status_object) {
  ASSERT_EQ(std::string(cal_status_object(CalFlow::LOAD_CELL)),
            std::string("nebulaos_load_cell_calibration"));
  ASSERT_EQ(std::string(cal_status_object(CalFlow::ESTEPS)),
            std::string("nebulaos_calibration"));
}

TEST(LoadCell, steps) {
  json s = json::object();
  s["state"] = "idle";
  auto v = cal_view(CalFlow::LOAD_CELL, s, false);
  ASSERT_EQ(v.title, std::string("Load Cell Calibration"));
  ASSERT_TRUE(v.can_start);
  ASSERT_TRUE(v.text.find("1.5 L bottle") != std::string::npos);
  const char *steps[][2] = {{"awaiting_empty", "Step 1 of 5"},
                            {"awaiting_weight", "Step 2 of 5"},
                            {"awaiting_replace", "Step 3 of 5"},
                            {"awaiting_remove", "Step 4 of 5"},
                            {"awaiting_finger", "Step 5 of 5"}};
  for (auto &st : steps) {
    s["state"] = st[0];
    v = cal_view(CalFlow::LOAD_CELL, s, false);
    ASSERT_TRUE(v.text.find(st[1]) != std::string::npos);
    ASSERT_TRUE(v.can_cancel);
    ASSERT_EQ(v.needs_measurement, std::string(st[0]) == "awaiting_weight");
    // G90: a page flow - its buttons are on the page, there is no prompt
    ASSERT_TRUE(v.text.find("prompt") == std::string::npos);
  }
  s["state"] = "awaiting_weight";
  ASSERT_TRUE(cal_view(CalFlow::LOAD_CELL, s, false).text.find("front-left screw head")
              != std::string::npos);
  ASSERT_TRUE(cal_view(CalFlow::LOAD_CELL, s, false).text.find("2 cm") == std::string::npos);
  s["problem"] = "The reading is not stable.";
  ASSERT_EQ(cal_view(CalFlow::LOAD_CELL, s, false).text.rfind("Problem: The reading", 0), 0u);
  s["state"] = "complete";
  s["counts_per_gram"] = 94.9;
  v = cal_view(CalFlow::LOAD_CELL, s, false);
  ASSERT_TRUE(v.text.find("94.900 counts per gram") != std::string::npos);
  ASSERT_TRUE(v.text.find("Problem") == std::string::npos);
  ASSERT_FALSE(cal_view(CalFlow::ESTEPS, s, false).needs_measurement);
}

static bool has_action(const CalView &v, const std::string &label, const std::string &cmd) {
  for (auto &a : v.actions) {
    if (a.first == label && a.second == cmd) return true;
  }
  return false;
}

TEST(LoadCell, step_buttons_on_the_page) {
  json s = json::object();
  s["state"] = "awaiting_empty";
  ASSERT_TRUE(has_action(cal_view(CalFlow::LOAD_CELL, s, false), "Check",
                         "NEBULAOS_LOAD_CELL_CALIBRATE CHECK=1"));
  s["state"] = "awaiting_weight";
  ASSERT_TRUE(cal_view(CalFlow::LOAD_CELL, s, false).actions.empty());   // the grams field
  ASSERT_EQ(std::string(cal_input_button(CalFlow::LOAD_CELL)), std::string("Weigh"));
  s["state"] = "awaiting_remove";
  s["replace_grams"] = 1560.0;
  s["grams"] = 1537.0;
  auto v = cal_view(CalFlow::LOAD_CELL, s, false);
  ASSERT_TRUE(v.text.find("Printer measures 1560 g, your scale said 1537 g.") != std::string::npos);
  ASSERT_TRUE(has_action(v, "Continue", "NEBULAOS_LOAD_CELL_CALIBRATE NEXT=1"));
  s["state"] = "awaiting_finger";
  s["finger_grams"] = 316.0;
  v = cal_view(CalFlow::LOAD_CELL, s, false);
  ASSERT_TRUE(has_action(v, "Measure", "NEBULAOS_LOAD_CELL_CALIBRATE MEASURE=1"));
  ASSERT_TRUE(v.text.find("Printer measures 316 g.") != std::string::npos);
  s["state"] = "awaiting_confirmation";
  s["counts_per_gram"] = 87.906;
  s["old_counts_per_gram"] = 85.72;
  s["change_percent"] = 2.55;
  s["change_warning"] = false;
  v = cal_view(CalFlow::LOAD_CELL, s, false);
  ASSERT_TRUE(v.text.find("87.91 counts per gram") != std::string::npos);
  ASSERT_TRUE(v.text.find("(+3%)") != std::string::npos);
  ASSERT_TRUE(v.text.find("very different") == std::string::npos);
  ASSERT_TRUE(has_action(v, "Apply & Save", "NEBULAOS_LOAD_CELL_CALIBRATE ACCEPT=1"));
  ASSERT_TRUE(v.can_cancel);
  s["change_warning"] = true;
  ASSERT_TRUE(cal_view(CalFlow::LOAD_CELL, s, false).text.find("very different") != std::string::npos);
}

TEST(Prompts, hidden_on_screen_for_page_flows) {
  ASSERT_TRUE(cal_prompt_hidden_on_screen("Load Cell Calibration"));
  ASSERT_TRUE(cal_prompt_hidden_on_screen("Bed Tramming"));
  ASSERT_FALSE(cal_prompt_hidden_on_screen("Load cell not calibrated"));   // the guard popup stays
  // G109 (Run 20): Input Shaper and E-Steps are page flows now too
  ASSERT_TRUE(cal_prompt_hidden_on_screen("Input Shaper Calibration"));
  ASSERT_TRUE(cal_prompt_hidden_on_screen("E-Steps Calibration"));
  ASSERT_TRUE(cal_prompt_hidden_on_screen("E-Steps Calibration \u2014 Confirm"));
  ASSERT_FALSE(cal_prompt_hidden_on_screen("M600 - Filament Change - UNLOAD Filament"));
  ASSERT_FALSE(cal_prompt_hidden_on_screen("RESUME aborted !!!"));
}

TEST(BedTram, page) {
  ASSERT_EQ(cal_start_command(CalFlow::BED_TRAM), std::string("NEBULAOS_BED_TRAM"));
  ASSERT_EQ(cal_cancel_command(CalFlow::BED_TRAM), std::string("_NEBULAOS_BED_TRAM_CANCEL"));
  ASSERT_EQ(std::string(cal_status_object(CalFlow::BED_TRAM)), std::string("nebulaos_bed_tram"));
  json s = json::object();
  s["state"] = "idle";
  s["mount"] = "rigid";
  auto v = cal_view(CalFlow::BED_TRAM, s, false);
  ASSERT_EQ(v.title, std::string("Bed Tramming"));
  ASSERT_TRUE(v.can_start);
  ASSERT_TRUE(v.text.find("rigid spacers") != std::string::npos);
  ASSERT_TRUE(has_action(v, "Switch to rubber/springs", "NEBULAOS_BED_TRAM MOUNT=adjustable"));
  ASSERT_TRUE(cal_view(CalFlow::BED_TRAM, s, true).actions.empty());     // not while printing

  s["state"] = "awaiting_mount";
  v = cal_view(CalFlow::BED_TRAM, s, false);
  ASSERT_TRUE(has_action(v, "Rigid spacers", "NEBULAOS_BED_TRAM MOUNT=rigid"));
  ASSERT_TRUE(v.can_cancel);

  s["state"] = "awaiting_adjustment";
  s["deviations"] = {0.15, 0.0, 0.12, 0.03};
  s["accepted"] = {false, true, false, true};
  s["target"] = "front left";
  s["message"] = "Front left: add a 0.10 mm shim.";
  v = cal_view(CalFlow::BED_TRAM, s, false);
  // older extensions (unsigned deviations), rigid: shown as below the highest corner
  ASSERT_TRUE(v.text.find("Front:  left -0.15 ADJUST    right  0.00 highest") != std::string::npos);
  ASSERT_TRUE(v.text.find("Rear:   left -0.03 OK    right -0.12 later") != std::string::npos);
  ASSERT_TRUE(v.text.find("Rear:") < v.text.find("Front:"));            // rear row on top
  ASSERT_TRUE(v.text.find("add a 0.10 mm shim") != std::string::npos);
  ASSERT_TRUE(has_action(v, "Done - measure again", "NEBULAOS_BED_TRAM CONFIRM=1"));

  s["state"] = "complete";
  s["message"] = "Bed tramming complete. No further screw or spacer adjustments needed.";
  s["load_cell_corner_changed"] = true;
  v = cal_view(CalFlow::BED_TRAM, s, false);
  ASSERT_TRUE(v.text.find("Bed tramming complete") != std::string::npos);
  ASSERT_TRUE(v.text.find("calibrate the load cell next") != std::string::npos);
  ASSERT_TRUE(v.can_start);
}

// Run 17 (Tim): signed values from signed_deviations + reference.
TEST(BedTram, signed_grid_rigid) {
  json s = json::object();
  s["state"] = "awaiting_adjustment";
  s["mount"] = "rigid";
  s["deviations"] = {0.23, 0.12, 0.0, 0.05};
  s["signed_deviations"] = {-0.23, -0.12, 0.0, -0.05};
  s["reference"] = "rear right";
  s["accepted"] = {false, false, true, true};
  s["target"] = "front left";
  s["message"] = "Front left: -0.23 mm. Add a 0.20 mm shim to raise it.";
  auto v = cal_view(CalFlow::BED_TRAM, s, false);
  ASSERT_TRUE(v.text.find("Rear:   left -0.05 OK    right  0.00 highest") != std::string::npos);
  ASSERT_TRUE(v.text.find("Front:  left -0.23 ADJUST    right -0.12 later") != std::string::npos);
  ASSERT_TRUE(v.text.find("Corners in mm vs the highest corner.\nFront = the side with the screen.") != std::string::npos);
  ASSERT_TRUE(v.text.find("Add a 0.20 mm shim") != std::string::npos);
}

TEST(BedTram, signed_grid_adjustable) {
  json s = json::object();
  s["state"] = "awaiting_adjustment";
  s["mount"] = "adjustable";
  s["deviations"] = {0.0, 0.08, 0.1, 0.02};
  s["signed_deviations"] = {0.0, 0.08, -0.1, 0.02};
  s["reference"] = 0;                                    // an index works too
  s["accepted"] = {true, false, false, true};
  s["target"] = "rear right";
  auto v = cal_view(CalFlow::BED_TRAM, s, false);
  ASSERT_TRUE(v.text.find("Rear:   left +0.02 OK    right -0.10 ADJUST") != std::string::npos);
  ASSERT_TRUE(v.text.find("Front:  left  0.00 base    right +0.08 later") != std::string::npos);
  ASSERT_TRUE(v.text.find("+ higher") != std::string::npos);
}

TEST(BedTram, unsigned_adjustable_fallback_has_no_sign) {
  json s = json::object();
  s["state"] = "complete";
  s["mount"] = "adjustable";
  s["deviations"] = {0.0, 0.01, 0.02, 0.0};
  s["accepted"] = {true, true, true, true};
  s["message"] = "Bed tramming complete.";
  auto v = cal_view(CalFlow::BED_TRAM, s, false);
  ASSERT_TRUE(v.text.find("Rear:   left 0.00 OK    right 0.02 OK") != std::string::npos);
  ASSERT_TRUE(v.text.find("Front:  left 0.00 OK    right 0.01 OK") != std::string::npos);
  ASSERT_TRUE(v.text.find("Corners (mm), front = the side with the screen:") != std::string::npos);
}

TEST(BedTram, bad_signed_values_fall_back) {
  json s = json::object();
  s["state"] = "awaiting_adjustment";
  s["mount"] = "rigid";
  s["deviations"] = {0.1, 0.0, 0.0, 0.0};
  s["signed_deviations"] = {-0.1, 0.0};                  // wrong size: use deviations
  s["reference"] = "nowhere";
  s["accepted"] = {false, true, true, true};
  s["target"] = "front left";
  auto v = cal_view(CalFlow::BED_TRAM, s, false);
  ASSERT_TRUE(v.text.find("Front:  left -0.10 ADJUST    right  0.00 highest") != std::string::npos);
  // only one corner is "highest"
  ASSERT_TRUE(v.text.find("0.00 highest") == v.text.rfind("0.00 highest"));
}

// G94 (Run 18, Tim's Run 17 test): the typed grams were lost on OK / Weigh.
TEST(Input, load_cell_grams_command_uses_the_typed_value) {
  ASSERT_EQ(cal_input_command(CalFlow::LOAD_CELL, "1531"),
            std::string("NEBULAOS_LOAD_CELL_CALIBRATE GRAMS=1531.0"));
  ASSERT_EQ(cal_input_command(CalFlow::LOAD_CELL, "1531.5"),
            std::string("NEBULAOS_LOAD_CELL_CALIBRATE GRAMS=1531.5"));
  ASSERT_EQ(cal_input_command(CalFlow::LOAD_CELL, ""), std::string(""));
  ASSERT_EQ(cal_input_command(CalFlow::LOAD_CELL, "153"), std::string(""));
  ASSERT_EQ(cal_input_command(CalFlow::LOAD_CELL, "."), std::string(""));
}

TEST(Input, keypad_ok_commits_a_usable_value) {
  auto ok = cal_keypad_ok(CalFlow::LOAD_CELL, "1531");
  ASSERT_TRUE(ok.close);
  ASSERT_TRUE(ok.problem.empty());
  ok = cal_keypad_ok(CalFlow::ESTEPS, "98.5");
  ASSERT_TRUE(ok.close);
}

TEST(Input, keypad_ok_on_an_empty_field_just_closes) {
  auto ok = cal_keypad_ok(CalFlow::LOAD_CELL, "");
  ASSERT_TRUE(ok.close);
  ASSERT_TRUE(ok.problem.empty());
}

TEST(Input, keypad_ok_keeps_the_keypad_for_an_unusable_value) {
  auto ok = cal_keypad_ok(CalFlow::LOAD_CELL, "15");
  ASSERT_FALSE(ok.close);
  ASSERT_TRUE(ok.problem.find("1000-5000") != std::string::npos);
  ok = cal_keypad_ok(CalFlow::ESTEPS, "600");
  ASSERT_FALSE(ok.close);
}

TEST(Input, value_kept_while_the_flow_runs_cleared_when_over) {
  json s = json::object();
  s["state"] = "awaiting_weight";
  ASSERT_TRUE(cal_input_keep(cal_view(CalFlow::LOAD_CELL, s, false)));
  s["problem"] = "The reading is not stable.";                  // retry: keep the grams
  ASSERT_TRUE(cal_input_keep(cal_view(CalFlow::LOAD_CELL, s, false)));
  s["problem"] = nullptr;
  for (const char *st : {"awaiting_replace", "awaiting_remove", "awaiting_finger",
                         "awaiting_confirmation"}) {
    s["state"] = st;
    ASSERT_TRUE(cal_input_keep(cal_view(CalFlow::LOAD_CELL, s, false)));
  }
  for (const char *st : {"idle", "complete", "cancelled", "error"}) {
    s["state"] = st;
    ASSERT_FALSE(cal_input_keep(cal_view(CalFlow::LOAD_CELL, s, false)));
  }
  json e = idle();
  e["esteps_state"] = "awaiting_measurement";
  ASSERT_TRUE(cal_input_keep(cal_view(CalFlow::ESTEPS, e, false)));
}

// G99 (Run 18): Calibration > Load Cell, Automatic (default) / Manual.
static json lc_status(const char *state, const char *mode, const char *check = "never",
                      bool ready = false, const char *reason = "") {
  json s = {{"state", state}, {"auto_check", check}, {"auto_ready", ready},
            {"auto_check_reason", reason}, {"mode_choice", mode ? json(mode) : json(nullptr)}};
  s["mode"] = mode ? json(mode) : json(nullptr);
  return s;
}

TEST(LoadCellMode, older_extensions_show_only_the_bottle_flow) {
  json old = {{"state", "idle"}};
  auto v = cal_view(CalFlow::LOAD_CELL, old, false);
  ASSERT_FALSE(v.has_modes);
  ASSERT_FALSE(v.show_modes);
  ASSERT_TRUE(v.can_start);
  ASSERT_TRUE(v.text.find("bottle") != std::string::npos);
  // no load cell probe: the extensions report the fields as null
  v = cal_view(CalFlow::LOAD_CELL, lc_status("idle", nullptr), false);
  ASSERT_FALSE(v.has_modes);
  ASSERT_TRUE(v.can_start);
}

TEST(LoadCellMode, automatic_offers_run_not_start) {
  auto v = cal_view(CalFlow::LOAD_CELL, lc_status("idle", "auto"), false);
  ASSERT_TRUE(v.has_modes);
  ASSERT_TRUE(v.show_modes);
  ASSERT_TRUE(v.mode_auto);
  ASSERT_FALSE(v.can_start);
  ASSERT_EQ(v.actions.size(), (size_t)1);
  ASSERT_EQ(v.actions[0].first, std::string("Run"));
  ASSERT_EQ(v.actions[0].second, std::string("NEBULAOS_LOAD_CELL_AUTO_CHECK"));
  ASSERT_TRUE(v.text.find("Not checked yet") != std::string::npos);
}

TEST(LoadCellMode, automatic_check_result) {
  auto v = cal_view(CalFlow::LOAD_CELL, lc_status("idle", "auto", "ok", true), false);
  ASSERT_TRUE(v.text.find("Ready - Automatic needs no weight calibration; Auto Calibration uses it.") != std::string::npos);
  ASSERT_EQ(v.actions.size(), (size_t)1);
  v = cal_view(CalFlow::LOAD_CELL,
               lc_status("idle", "auto", "failed", false, "the load cell reading is too close to its limit"),
               false);
  ASSERT_TRUE(v.text.find("FAILED: the load cell reading is too close to its limit.") != std::string::npos);
  ASSERT_EQ(v.actions.size(), (size_t)2);
  ASSERT_EQ(v.actions[1].first, std::string("Use Manual"));
  ASSERT_EQ(v.actions[1].second, std::string("NEBULAOS_LOAD_CELL_MODE MODE=manual"));
}

TEST(LoadCellMode, manual_is_the_bottle_flow) {
  auto v = cal_view(CalFlow::LOAD_CELL, lc_status("idle", "manual"), false);
  ASSERT_TRUE(v.show_modes);
  ASSERT_FALSE(v.mode_auto);
  ASSERT_TRUE(v.can_start);
  ASSERT_TRUE(v.actions.empty());
  ASSERT_TRUE(v.text.find("bottle") != std::string::npos);
}

TEST(LoadCellMode, running_bottle_flow_hides_the_choice) {
  auto v = cal_view(CalFlow::LOAD_CELL, lc_status("awaiting_weight", "manual"), false);
  ASSERT_TRUE(v.has_modes);
  ASSERT_FALSE(v.show_modes);
  ASSERT_TRUE(v.needs_measurement);
}

TEST(LoadCellMode, choice_wins_over_forced_mode) {
  json s = lc_status("idle", "manual");
  s["mode_choice"] = "auto";               // contact_detection_mode forces manual
  auto v = cal_view(CalFlow::LOAD_CELL, s, false);
  ASSERT_TRUE(v.mode_auto);
  ASSERT_TRUE(v.text.find("forces manual") != std::string::npos);
}

TEST(LoadCellMode, printing_blocks_the_check) {
  auto v = cal_view(CalFlow::LOAD_CELL, lc_status("idle", "auto", "failed"), true);
  ASSERT_TRUE(v.actions.empty());
  ASSERT_TRUE(v.text.find("Not available while printing.") != std::string::npos);
}

TEST(LoadCellMode, commands_and_running_text) {
  ASSERT_EQ(cal_mode_command(true), std::string("NEBULAOS_LOAD_CELL_MODE MODE=auto"));
  ASSERT_EQ(cal_mode_command(false), std::string("NEBULAOS_LOAD_CELL_MODE MODE=manual"));
  auto v = cal_view(CalFlow::LOAD_CELL, lc_status("idle", "auto"), false);
  cal_auto_check_running(v);
  ASSERT_TRUE(v.actions.empty());
  ASSERT_TRUE(v.text.find("Checking the load cell") != std::string::npos);
}

TEST(LoadCellMode, auto_calibrate_text_mentions_the_check) {
  json s = {{"auto_calibrate_state", "idle"}};
  auto v = cal_view(CalFlow::AUTO_CALIBRATE, s, false);
  ASSERT_TRUE(v.text.find("Automatic mode it checks the load cell") != std::string::npos);
}

// ---- G109 (Run 20): Input Shaper / E-Steps on the page, graphs ---------------
static json shaper_axes() {
  return {{"x", {{"type", "mzv"}, {"freq", 52.4}, {"vibrations", 2.1}, {"smoothing", 0.061},
                 {"max_accel", 7000.0}, {"graph", "/cal/input_shaper_x.png"}, {"graph_error", ""}}},
          {"y", {{"type", "ei"}, {"freq", 38.7}, {"vibrations", 1.0}, {"smoothing", 0.09},
                 {"max_accel", 4400.0}, {"graph", ""}, {"graph_error", "took longer than 240 s"}}}};
}

TEST(G109, start_command_carries_the_graph_choice) {
  ASSERT_EQ(cal_shaper_start_command(true), std::string("NEBULAOS_INPUT_SHAPER_CALIBRATE GRAPHS=1"));
  ASSERT_EQ(cal_shaper_start_command(false), std::string("NEBULAOS_INPUT_SHAPER_CALIBRATE GRAPHS=0"));
}

TEST(G109, axis_line) {
  json s = idle();
  s["input_shaper_axes"] = shaper_axes();
  ASSERT_EQ(cal_shaper_axis_line(s, "x"),
            std::string("X: mzv 52.4 Hz\n  vibrations 2.1 %\n  smoothing 0.061\n  max accel 7000"));
  ASSERT_EQ(cal_shaper_axis_line(s, "z"), std::string(""));
  ASSERT_EQ(cal_shaper_axis_line(idle(), "x"), std::string(""));
  json old = idle();   // only type + freq
  old["input_shaper_axes"] = {{"x", {{"type", "zv"}, {"freq", 60.0}}}};
  ASSERT_EQ(cal_shaper_axis_line(old, "x"), std::string("X: zv 60.0 Hz"));
}

TEST(G109, mount_steps_have_continue_on_the_page) {
  json s = idle();
  s["input_shaper_state"] = "awaiting_x_mount";
  auto v = cal_view(CalFlow::INPUT_SHAPER, s, false);
  ASSERT_EQ(v.actions.size(), (size_t)1);
  ASSERT_EQ(v.actions[0].second, std::string("_NEBULAOS_INPUT_SHAPER_CALIBRATE CONTINUE=1"));
  ASSERT_TRUE(v.text.find("in the prompt") == std::string::npos);
  ASSERT_FALSE(v.graph_switch);   // locked during a run
  ASSERT_TRUE(v.graphs.empty());
}

TEST(G109, after_x_the_result_and_its_graph) {
  json s = idle();
  s["input_shaper_state"] = "awaiting_y_mount";
  s["input_shaper_axes"] = shaper_axes();
  auto v = cal_view(CalFlow::INPUT_SHAPER, s, false);
  ASSERT_TRUE(v.text.find("X: mzv 52.4 Hz") == 0);
  ASSERT_TRUE(v.text.find("onto the bed") != std::string::npos);
  ASSERT_EQ(v.graphs.size(), (size_t)1);
  ASSERT_EQ(v.graphs[0].first, std::string("X"));
  ASSERT_EQ(v.graphs[0].second, std::string("/cal/input_shaper_x.png"));
  ASSERT_EQ(v.actions[0].first, std::string("Continue"));
}

TEST(G109, x_graph_problem_is_said) {
  json s = idle();
  s["input_shaper_state"] = "awaiting_y_mount";
  json axes = shaper_axes();
  axes["x"]["graph"] = "";
  axes["x"]["graph_error"] = "ImportError: no matplotlib";
  s["input_shaper_axes"] = axes;
  auto v = cal_view(CalFlow::INPUT_SHAPER, s, false);
  ASSERT_TRUE(v.graphs.empty());
  ASSERT_TRUE(v.text.find("X graph not drawn: ImportError: no matplotlib") != std::string::npos);
}

TEST(G109, end_page_shows_both_graphs_and_the_switch) {
  json s = idle();
  s["input_shaper_state"] = "complete";
  json axes = shaper_axes();
  axes["y"]["graph"] = "/cal/input_shaper_y.png";
  s["input_shaper_axes"] = axes;
  auto v = cal_view(CalFlow::INPUT_SHAPER, s, false);
  ASSERT_EQ(v.graphs.size(), (size_t)2);
  ASSERT_EQ(v.graphs[1].first, std::string("Y"));
  ASSERT_TRUE(v.graph_switch);
  ASSERT_TRUE(v.can_start);
}

TEST(G109, idle_page_has_the_switch_not_while_printing) {
  ASSERT_TRUE(cal_view(CalFlow::INPUT_SHAPER, idle(), false).graph_switch);
  ASSERT_FALSE(cal_view(CalFlow::INPUT_SHAPER, idle(), true).graph_switch);
  ASSERT_FALSE(cal_view(CalFlow::ESTEPS, idle(), false).graph_switch);
}

TEST(G109, esteps_steps_have_their_buttons_on_the_page) {
  json s = idle();
  s["esteps_state"] = "awaiting_continue";
  auto v = cal_view(CalFlow::ESTEPS, s, false);
  ASSERT_EQ(v.actions.size(), (size_t)1);
  ASSERT_EQ(v.actions[0].first, std::string("Start Extrusion"));
  ASSERT_EQ(v.actions[0].second, std::string("_NEBULAOS_ESTEPS_CALIBRATE CONTINUE=1"));
  s["esteps_state"] = "awaiting_confirmation";
  s["esteps_old_rotation_distance"] = 7.53;
  s["esteps_new_rotation_distance"] = 7.38;
  s["esteps_measured_length"] = 98.0;
  s["esteps_commanded_length"] = 100.0;
  v = cal_view(CalFlow::ESTEPS, s, false);
  ASSERT_EQ(v.actions[0].second, std::string("_NEBULAOS_ESTEPS_CONFIRM"));
  ASSERT_TRUE(v.text.find("7.5300 -> 7.3800") != std::string::npos);
  ASSERT_TRUE(v.text.find("Measured 98.0 mm of 100 mm") != std::string::npos);
  ASSERT_TRUE(v.can_cancel);
}

int main() {
  return minitest::run_all("nebulaos_calibration_flow") > 0 ? 1 : 0;
}
