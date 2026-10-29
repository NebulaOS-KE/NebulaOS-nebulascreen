// Offline tests for the Run 19 calibration status logic (G104 feedback, G105
// menu status lines, G106 saved mesh) - no LVGL, no Klipper. Build/run:
//   make test-nebulaos-calibration-status
//
// This file may be distributed under the terms of the GNU GPLv3 license.
#include "minitest.h"
#include "../src/nebulaos_calibration_status.h"

#include <cstdlib>
#include <ctime>

using json = nlohmann::json;

// 2026-10-24 12:00 UTC
static const double OCT24 = 1792843200.0;

static void utc() {
  setenv("TZ", "UTC", 1);
  tzset();
}

static const CalMenuEntry &entry(const char *key) {
  for (auto &e : cal_menu_in_order()) if (std::string(e.key) == key) return e;
  for (auto &e : cal_menu_other()) if (std::string(e.key) == key) return e;
  static CalMenuEntry none = {"", "", ""};
  return none;
}

TEST(Menu, order_is_tims_one_to_seven) {
  auto &r = cal_menu_in_order();
  ASSERT_EQ(r.size(), (size_t)7);
  const char *keys[] = {"bed_tram", "pid", "axis_twist", "auto_calibrate",
                        "input_shaper", "esteps", "skew"};
  for (int i = 0; i < 7; i++) ASSERT_EQ(std::string(r[i].key), std::string(keys[i]));
  ASSERT_EQ(std::string(r[1].title), std::string("Heaters (PID)"));
  ASSERT_EQ(std::string(r[3].title), std::string("Auto Calibration"));
  ASSERT_EQ(std::string(r[3].description),
            std::string("Load cell check, nozzle clean, Z offset, bed mesh - about 7 min, no tools needed"));
}

TEST(Menu, other_section) {
  auto &r = cal_menu_other();
  ASSERT_EQ(r.size(), (size_t)3);
  ASSERT_EQ(std::string(r[0].title), std::string("Load Cell Calibration"));
  ASSERT_EQ(std::string(r[1].key), std::string("tmc_autotune"));
  ASSERT_EQ(std::string(r[2].key), std::string("bed_mesh"));
}

TEST(Menu, short_date) {
  utc();
  ASSERT_EQ(cal_short_date(OCT24), std::string("24 Oct"));
  ASSERT_EQ(cal_short_date(OCT24 - 23 * 86400.0), std::string("1 Oct"));
  ASSERT_EQ(cal_short_date(0), std::string(""));
  ASSERT_EQ(cal_short_date(-5), std::string(""));
}

TEST(Menu, done_line) {
  utc();
  json h = {{"auto_calibrate", {{"done_at", OCT24}, {"summary", "Z offset 1.752 mm, mesh range 0.16 mm"},
                                {"redo", nullptr}}}};
  auto s = cal_row_status(h, entry("auto_calibrate"));
  ASSERT_TRUE(s.state == CalRowState::DONE);
  ASSERT_EQ(s.text, std::string("Done 24 Oct - Z offset 1.752 mm, mesh range 0.16 mm"));
  h["auto_calibrate"]["summary"] = "";
  ASSERT_EQ(cal_row_status(h, entry("auto_calibrate")).text, std::string("Done 24 Oct"));
}

TEST(Menu, not_done_is_the_description) {
  json h = {{"pid", {{"done_at", nullptr}, {"summary", ""}, {"redo", nullptr}}}};
  auto s = cal_row_status(h, entry("pid"));
  ASSERT_TRUE(s.state == CalRowState::DESCRIPTION);
  ASSERT_EQ(s.text, std::string(entry("pid").description));
}

TEST(Menu, redo_wins_over_done) {
  json h = {{"auto_calibrate", {{"done_at", OCT24}, {"summary", "x"}, {"redo", "Axis Twist changed"}}}};
  auto s = cal_row_status(h, entry("auto_calibrate"));
  ASSERT_TRUE(s.state == CalRowState::REDO);
  ASSERT_EQ(s.text, std::string("Redo: Axis Twist changed"));
}

TEST(Menu, older_extensions_descriptions_only) {
  for (json h : {json(), json::object(), json("x"), json({{"skew", 5}})}) {
    auto s = cal_row_status(h, entry("skew"));
    ASSERT_TRUE(s.state == CalRowState::DESCRIPTION);
    ASSERT_EQ(s.text, std::string(entry("skew").description));
  }
}

TEST(Record, command_and_quotes) {
  ASSERT_EQ(cal_record_command("skew", "XY 0.01"),
            std::string("NEBULAOS_CALIBRATION_RECORD FLOW=skew SUMMARY=\"XY 0.01\""));
  ASSERT_EQ(cal_record_command("pid", "a\"b'c\nd"),
            std::string("NEBULAOS_CALIBRATION_RECORD FLOW=pid SUMMARY=\"abc d\""));
  ASSERT_EQ(cal_record_command("bed_mesh", ""), std::string("NEBULAOS_CALIBRATION_RECORD FLOW=bed_mesh"));
  ASSERT_EQ(cal_record_command("x", std::string(200, 'a')).size(),
            std::string("NEBULAOS_CALIBRATION_RECORD FLOW=x SUMMARY=\"\"").size() + 80);
}

TEST(Record, only_with_newer_extensions) {
  json older = {{"auto_calibrate_state", "idle"}};
  json newer = {{"auto_calibrate_state", "idle"}, {"history", json::object()}};
  ASSERT_FALSE(cal_history_supported(older));
  ASSERT_TRUE(cal_history_supported(newer));
  ASSERT_EQ(cal_record_prefix(older, "skew", "s"), std::string(""));
  ASSERT_EQ(cal_record_prefix(newer, "skew", "s"),
            std::string("NEBULAOS_CALIBRATION_RECORD FLOW=skew SUMMARY=\"s\"\n"));
}

TEST(Record, axis_twist_removes_the_saved_mesh_first) {
  json newer = {{"history", json::object()}};
  std::string s = cal_axis_twist_save_script(newer);
  ASSERT_EQ(s.rfind("BED_MESH_PROFILE REMOVE=default\nNEBULAOS_CALIBRATION_RECORD FLOW=axis_twist", 0), (size_t)0);
  ASSERT_TRUE(s.size() > 12 && s.substr(s.size() - 12) == "\nSAVE_CONFIG");
  ASSERT_EQ(cal_axis_twist_save_script(json()), std::string("BED_MESH_PROFILE REMOVE=default\nSAVE_CONFIG"));
}

TEST(Pid, commands) {
  ASSERT_EQ(pid_command(PidHeater::BED), std::string("PID_CALIBRATE HEATER=heater_bed TARGET=65"));
  ASSERT_EQ(pid_command(PidHeater::HOTEND), std::string("PID_CALIBRATE HEATER=extruder TARGET=230"));
  json newer = {{"history", json::object()}};
  ASSERT_EQ(pid_save_script(newer, PidHeater::BED),
            std::string("NEBULAOS_CALIBRATION_RECORD FLOW=pid SUMMARY=\"Bed tuned at 65 C\"\nSAVE_CONFIG"));
  ASSERT_EQ(pid_save_script(json(), PidHeater::HOTEND), std::string("SAVE_CONFIG"));
}

TEST(Pid, running_text) {
  std::string t = pid_running_text(PidHeater::BED, {{"temperature", 61.4}, {"target", 65.0}});
  ASSERT_TRUE(t.find("Tuning the bed at 65 C") != std::string::npos);
  ASSERT_TRUE(t.find("Bed: 61 / 65 C") != std::string::npos);
  t = pid_running_text(PidHeater::HOTEND, json());
  ASSERT_TRUE(t.find("hotend at 230 C") != std::string::npos);
  ASSERT_TRUE(t.find("Hotend:") == std::string::npos);
}

static json prog(const char *flow, int step, int total, const char *key, const char *label,
                 const char *detail) {
  return {{"progress", {{"flow", flow}, {"step", step}, {"total", total}, {"key", key},
                        {"label", label}, {"detail", detail}}}};
}

TEST(Progress, step_list_done_current_next) {
  auto v = cal_progress_view(CalFlow::AUTO_CALIBRATE,
    prog("auto_calibrate", 7, 10, "localized_z_offset", "Measuring the Z offset", "touch 2 of 3"));
  ASSERT_TRUE(v.active);
  ASSERT_EQ(v.counter, std::string("Step 7 of 10"));
  ASSERT_EQ(v.label, std::string("Measuring the Z offset"));
  ASSERT_EQ(v.detail, std::string("touch 2 of 3"));
  ASSERT_FALSE(v.restarting);
  ASSERT_EQ(v.steps.size(), (size_t)10);
  ASSERT_TRUE(v.steps[0].state == CalStepState::DONE);
  ASSERT_TRUE(v.steps[5].state == CalStepState::DONE);
  ASSERT_TRUE(v.steps[6].state == CalStepState::CURRENT);
  ASSERT_TRUE(v.steps[7].state == CalStepState::NEXT);
  ASSERT_EQ(v.steps[2].label, std::string("Nozzle clean"));
  ASSERT_EQ(v.steps[9].label, std::string("Verifying"));
}

TEST(Progress, save_step_is_a_restart) {
  auto v = cal_progress_view(CalFlow::AUTO_CALIBRATE,
    prog("auto_calibrate", 9, 10, "commit", "Saving (Klipper restarts)", ""));
  ASSERT_TRUE(v.restarting);
}

TEST(Progress, other_flow_or_null_is_inactive) {
  ASSERT_FALSE(cal_progress_view(CalFlow::ESTEPS,
    prog("auto_calibrate", 1, 10, "preflight", "Load cell check", "")).active);
  ASSERT_FALSE(cal_progress_view(CalFlow::AUTO_CALIBRATE, {{"progress", nullptr}}).active);
  ASSERT_FALSE(cal_progress_view(CalFlow::AUTO_CALIBRATE, json::object()).active);
  ASSERT_FALSE(cal_progress_view(CalFlow::AUTO_CALIBRATE,
    prog("auto_calibrate", 11, 10, "x", "x", "")).active);
}

TEST(Progress, unknown_step_table_falls_back) {
  // a newer extensions version with 12 steps: no stale names from the table
  auto v = cal_progress_view(CalFlow::AUTO_CALIBRATE,
    prog("auto_calibrate", 3, 12, "new", "Something new", ""));
  ASSERT_TRUE(v.active);
  ASSERT_EQ(v.steps.size(), (size_t)12);
  ASSERT_EQ(v.steps[0].label, std::string("Step 1"));
  ASSERT_EQ(v.steps[2].label, std::string("Something new"));
}

TEST(Progress, esteps_temperatures) {
  auto v = cal_progress_view(CalFlow::ESTEPS,
    prog("esteps", 1, 5, "heating", "Heating up", "nozzle 148/210 C"));
  ASSERT_TRUE(v.active);
  ASSERT_EQ(v.detail, std::string("nozzle 148/210 C"));
  ASSERT_EQ(v.steps[1].label, std::string("Mark the filament"));
}

TEST(Result, auto_calibrate_success) {
  utc();
  json s = {{"last_result", {{"flow", "auto_calibrate"}, {"ok", true}, {"finished_at", OCT24},
    {"summary", {"Z offset 1.752 mm (was 1.780)", "Bed mesh 9x9, range 0.16 mm"}},
    {"error_step", ""}, {"error", ""}, {"advice", ""}}}};
  auto r = cal_result_view(CalFlow::AUTO_CALIBRATE, s);
  ASSERT_TRUE(r.present);
  ASSERT_TRUE(r.ok);
  ASSERT_EQ(r.headline, std::string("Done 24 Oct"));
  ASSERT_EQ(r.lines.size(), (size_t)3);
  ASSERT_EQ(r.lines[1], std::string("Bed mesh 9x9, range 0.16 mm"));
  ASSERT_EQ(r.lines[2], std::string("Your Z adjustment was reset to 0."));
  // the extensions already said it: not twice
  s["last_result"]["summary"].push_back("Z adjustment reset to 0 (was -0.040 mm)");
  r = cal_result_view(CalFlow::AUTO_CALIBRATE, s);
  ASSERT_EQ(r.lines.size(), (size_t)3);
}

TEST(Result, failure_has_step_reason_advice) {
  json s = {{"last_result", {{"flow", "auto_calibrate"}, {"ok", false}, {"finished_at", OCT24},
    {"summary", json::array()}, {"error_step", "Measuring the Z offset"},
    {"error", "the touches disagree by 0.08 mm"},
    {"advice", "Clean the nozzle and the bed at the front-left corner, then try again."}}}};
  auto r = cal_result_view(CalFlow::AUTO_CALIBRATE, s);
  ASSERT_TRUE(r.present);
  ASSERT_FALSE(r.ok);
  ASSERT_EQ(r.headline, std::string("Failed: Measuring the Z offset"));
  ASSERT_EQ(r.lines[0], std::string("Why: the touches disagree by 0.08 mm"));
  ASSERT_EQ(r.lines[1].rfind("What to do: Clean the nozzle", 0), (size_t)0);
  ASSERT_EQ(r.lines.back(), std::string("Nothing was saved."));
}

TEST(Result, other_flow_and_esteps_no_z_line) {
  json s = {{"last_result", {{"flow", "esteps"}, {"ok", true}, {"finished_at", OCT24},
    {"summary", {"rotation_distance 7.71 -> 7.65"}}}}};
  ASSERT_FALSE(cal_result_view(CalFlow::AUTO_CALIBRATE, s).present);
  auto r = cal_result_view(CalFlow::ESTEPS, s);
  ASSERT_TRUE(r.present);
  ASSERT_EQ(r.lines.size(), (size_t)1);
  ASSERT_FALSE(cal_result_view(CalFlow::ESTEPS, {{"last_result", nullptr}}).present);
}

static CalView base(CalFlow f, const json &s) { return cal_view(f, s, false); }

TEST(Feedback, running_step_list_replaces_the_stage_text) {
  json s = prog("auto_calibrate", 7, 10, "localized_z_offset", "Measuring the Z offset", "touch 2 of 3");
  s["auto_calibrate_state"] = "running";
  s["auto_calibrate_stage"] = "localized_z_offset";
  CalView v = base(CalFlow::AUTO_CALIBRATE, s);
  auto f = cal_feedback(CalFlow::AUTO_CALIBRATE, s, true, v);
  ASSERT_TRUE(f.progress.active);
  ASSERT_FALSE(f.result.present);
  ASSERT_EQ(v.text, std::string(""));
  ASSERT_TRUE(v.can_cancel);
}

TEST(Feedback, restart_and_reconnect_notice) {
  json s = prog("auto_calibrate", 9, 10, "commit", "Saving (Klipper restarts)", "");
  s["auto_calibrate_state"] = "running";
  CalView v = base(CalFlow::AUTO_CALIBRATE, s);
  cal_feedback(CalFlow::AUTO_CALIBRATE, s, true, v);
  ASSERT_EQ(v.text, std::string(CAL_RESTART_NOTICE));
  ASSERT_FALSE(v.can_cancel);
  v = base(CalFlow::AUTO_CALIBRATE, s);
  cal_feedback(CalFlow::AUTO_CALIBRATE, s, false, v);
  ASSERT_EQ(v.text, std::string(CAL_RECONNECT_NOTICE));
  // no progress (older extensions), restart_pending
  json o = {{"auto_calibrate_state", "restart_pending"}};
  v = base(CalFlow::AUTO_CALIBRATE, o);
  cal_feedback(CalFlow::AUTO_CALIBRATE, o, true, v);
  ASSERT_EQ(v.text, std::string(CAL_RESTART_NOTICE));
}

TEST(Feedback, awaiting_step_keeps_its_prompt) {
  json s = prog("esteps", 3, 5, "measure", "Measure and enter the length", "");
  s["esteps_state"] = "awaiting_measurement";
  CalView v = base(CalFlow::ESTEPS, s);
  std::string before = v.text;
  auto f = cal_feedback(CalFlow::ESTEPS, s, true, v);
  ASSERT_TRUE(f.progress.active);
  ASSERT_EQ(v.text, before);
  ASSERT_TRUE(v.needs_measurement);
}

TEST(Feedback, end_screen_after_complete) {
  utc();
  json s = {{"auto_calibrate_state", "complete"}, {"progress", nullptr},
            {"last_result", {{"flow", "auto_calibrate"}, {"ok", true}, {"finished_at", OCT24},
                             {"summary", {"Z offset 1.669 mm (was 1.795)", "Bed mesh 9x9, range 0.16 mm"}}}}};
  CalView v = base(CalFlow::AUTO_CALIBRATE, s);
  auto f = cal_feedback(CalFlow::AUTO_CALIBRATE, s, true, v);
  ASSERT_TRUE(f.result.present);
  ASSERT_EQ(f.result.headline, std::string("Done 24 Oct"));
  ASSERT_EQ(v.text, std::string("Z offset 1.669 mm (was 1.795)\nBed mesh 9x9, range 0.16 mm\n"
                                "Your Z adjustment was reset to 0."));
  ASSERT_TRUE(v.can_start);
}

TEST(Feedback, input_shaper_end_screen_details_each_axis) {
  // G109 (Run 20): the detailed per-axis result replaces the short summary
  utc();
  json s = {{"input_shaper_state", "complete"}, {"progress", nullptr},
            {"input_shaper_axes",
             {{"x", {{"type", "mzv"}, {"freq", 52.4}, {"vibrations", 2.1}, {"smoothing", 0.061},
                     {"max_accel", 7000.0}, {"graph", "/c/x.png"}, {"graph_error", ""}}},
              {"y", {{"type", "ei"}, {"freq", 38.7}, {"graph", ""},
                     {"graph_error", "took longer than 240 s"}}}}},
            {"last_result", {{"flow", "input_shaper"}, {"ok", true}, {"finished_at", OCT24},
                             {"summary", {"X: mzv 52.4 Hz", "Y: ei 38.7 Hz"}}}}};
  CalView v = base(CalFlow::INPUT_SHAPER, s);
  auto f = cal_feedback(CalFlow::INPUT_SHAPER, s, true, v);
  ASSERT_TRUE(f.result.present);
  ASSERT_EQ(v.text, std::string("X: mzv 52.4 Hz\n  vibrations 2.1 %\n  smoothing 0.061\n  max accel 7000\n"
                                "Y: ei 38.7 Hz\n"
                                "Y graph not drawn: took longer than 240 s"));
  // older extensions (no input_shaper_axes): the summary as before
  s.erase("input_shaper_axes");
  v = base(CalFlow::INPUT_SHAPER, s);
  cal_feedback(CalFlow::INPUT_SHAPER, s, true, v);
  ASSERT_EQ(v.text, std::string("X: mzv 52.4 Hz\nY: ei 38.7 Hz"));
}

TEST(Feedback, failure_screen) {
  json s = {{"auto_calibrate_state", "error"}, {"auto_calibrate_error", "x"},
            {"last_result", {{"flow", "auto_calibrate"}, {"ok", false}, {"error_step", "Bed mesh"},
                             {"error", "probe did not trigger"}, {"advice", "Check the probe."}}}};
  CalView v = cal_view(CalFlow::AUTO_CALIBRATE, s, false);
  auto f = cal_feedback(CalFlow::AUTO_CALIBRATE, s, true, v);
  ASSERT_EQ(f.result.headline, std::string("Failed: Bed mesh"));
  ASSERT_TRUE(v.can_start);
  ASSERT_EQ(v.text, std::string("Why: probe did not trigger\nWhat to do: Check the probe.\n"
                                "Nothing was saved."));
  // idle while printing: the note stays under the result
  s["auto_calibrate_state"] = "idle";
  v = cal_view(CalFlow::AUTO_CALIBRATE, s, true);
  cal_feedback(CalFlow::AUTO_CALIBRATE, s, true, v);
  ASSERT_EQ(v.text.substr(v.text.size() - 29), std::string("Not available while printing."));
}

TEST(Feedback, cancelled_and_other_pages_unchanged) {
  json s = {{"auto_calibrate_state", "cancelled"},
            {"last_result", {{"flow", "auto_calibrate"}, {"ok", false}}}};
  CalView v = base(CalFlow::AUTO_CALIBRATE, s);
  std::string before = v.text;
  ASSERT_FALSE(cal_feedback(CalFlow::AUTO_CALIBRATE, s, true, v).result.present);
  ASSERT_EQ(v.text, before);
  json lc = {{"state", "idle"}};
  v = base(CalFlow::LOAD_CELL, lc);
  before = v.text;
  cal_feedback(CalFlow::LOAD_CELL, lc, true, v);
  ASSERT_EQ(v.text, before);
}

static json profile(json points) {
  return {{"points", points}, {"mesh_params", {{"x_count", 3}, {"y_count", 2}}}};
}

TEST(SavedMesh, prefers_default) {
  json p = {{"aaa", profile({{0.1, 0.2, 0.3}, {0.0, 0.1, 0.2}})},
            {"default", profile({{0.0, 0.1, 0.0}, {0.0, -0.1, 0.0}})}};
  ASSERT_EQ(bedmesh_saved_profile(p), std::string("default"));
  auto pts = bedmesh_profile_points(p, "default");
  ASSERT_EQ(pts.size(), (size_t)2);
  ASSERT_EQ(pts[0].size(), (size_t)3);
  ASSERT_NEAR(pts[1][1], -0.1, 1e-9);
}

TEST(SavedMesh, first_usable_otherwise) {
  json p = {{"aaa", profile({{0.1, "x"}})}, {"bbb", profile({{0.1, 0.2}, {0.3, 0.4}})},
            {"default", profile(json::array())}};
  ASSERT_EQ(bedmesh_saved_profile(p), std::string("bbb"));
}

TEST(SavedMesh, none) {
  ASSERT_EQ(bedmesh_saved_profile(json::object()), std::string(""));
  ASSERT_EQ(bedmesh_saved_profile(json()), std::string(""));
  ASSERT_EQ(bedmesh_saved_profile({{"default", {{"mesh_params", json::object()}}}}), std::string(""));
  // ragged rows are not a mesh
  ASSERT_EQ(bedmesh_saved_profile({{"default", profile({{0.1, 0.2}, {0.3}})}}), std::string(""));
}

TEST(SavedMesh, texts) {
  ASSERT_EQ(bedmesh_saved_notice("default"),
            std::string("Saved mesh 'default' - each print measures its own mesh at the start"));
  ASSERT_EQ(std::string(BEDMESH_NONE_TEXT),
            std::string("No bed mesh yet - run Calibration > Auto Calibration."));
}

TEST(Pid, running_text_with_progress) {
  json heater = {{"temperature", 52.4}, {"target", 65.0}};
  json none = json::object();
  // older extensions: the plain text
  ASSERT_EQ(pid_running_text(PidHeater::BED, heater, none), pid_running_text(PidHeater::BED, heater));
  json other = {{"progress", {{"flow", "esteps"}, {"step", 1}, {"total", 5}}}};
  ASSERT_EQ(pid_running_text(PidHeater::BED, heater, other), pid_running_text(PidHeater::BED, heater));
  json heat = {{"progress", {{"flow", "pid"}, {"step", 1}, {"total", 2}, {"key", "heating"},
                             {"label", "Heating up"}, {"detail", "Bed 52/65 C"}}}};
  std::string t = pid_running_text(PidHeater::BED, heater, heat);
  ASSERT_TRUE(t.find("Heating up: Bed 52/65 C") != std::string::npos);
  json meas = {{"progress", {{"flow", "pid"}, {"step", 2}, {"total", 2}, {"key", "cycles"},
                             {"label", "Measuring"},
                             {"detail", "Bed 65 C, cycle 2 of 6, about 10 min left"}}}};
  t = pid_running_text(PidHeater::BED, heater, meas);
  ASSERT_TRUE(t.find("Measuring: Bed 65 C, cycle 2 of 6, about 10 min left") != std::string::npos);
  ASSERT_TRUE(t.find("Tuning the bed at 65 C") == 0);
}

int main() {
  return minitest::run_all("nebulaos_calibration_status") > 0 ? 1 : 0;
}
