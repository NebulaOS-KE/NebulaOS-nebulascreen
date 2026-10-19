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
  ASSERT_EQ(v.title, std::string("Load Cell"));
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

int main() {
  return minitest::run_all("nebulaos_calibration_flow") > 0 ? 1 : 0;
}
