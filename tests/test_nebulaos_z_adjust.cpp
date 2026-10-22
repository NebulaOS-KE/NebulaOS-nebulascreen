// Offline tests for the screen's saved Z offset adjustment (G92, src/nebulaos_z_adjust.cpp).
// Build/run:
//   make test-nebulaos-z-adjust
//
// This file may be distributed under the terms of the GNU GPLv3 license.
#include "minitest.h"
#include "../src/nebulaos_z_adjust.h"

using json = nlohmann::json;

TEST(Available, object_list_forms) {
  json objs = {"gcode_move", "nebulaos_z_adjust", "gcode_macro M600"};
  ASSERT_TRUE(z_adjust_available(objs));
  ASSERT_TRUE(z_adjust_available(json{{"objects", objs}}));
  ASSERT_TRUE(z_adjust_available(json{{"result", {{"objects", objs}}}}));
}

TEST(Available, older_extensions_or_unknown) {
  ASSERT_FALSE(z_adjust_available(json{"gcode_move", "nebulaos_bed_tram"}));
  ASSERT_FALSE(z_adjust_available(json()));
  ASSERT_FALSE(z_adjust_available(json::object()));
  ASSERT_FALSE(z_adjust_available(json{{"objects", "nebulaos_z_adjust"}}));
  // a prefix or a section with that name as a suffix is not the object
  ASSERT_FALSE(z_adjust_available(json{"nebulaos_z_adjust_old", "gcode_macro nebulaos_z_adjust"}));
}

TEST(Command, saved_steps) {
  ASSERT_EQ(z_adjust_step_command(true, true, "0.01"), std::string("NEBULAOS_Z_ADJUST DELTA=+0.01"));
  ASSERT_EQ(z_adjust_step_command(true, false, "0.025"), std::string("NEBULAOS_Z_ADJUST DELTA=-0.025"));
  ASSERT_EQ(z_adjust_reset_command(true), std::string("NEBULAOS_Z_ADJUST RESET=1"));
}

TEST(Command, fallback_is_the_old_klipper_command) {
  ASSERT_EQ(z_adjust_step_command(false, true, "0.05"),
            std::string("SET_GCODE_OFFSET Z_ADJUST=+0.05 MOVE=1"));
  ASSERT_EQ(z_adjust_step_command(false, false, "0.001"),
            std::string("SET_GCODE_OFFSET Z_ADJUST=-0.001 MOVE=1"));
  ASSERT_EQ(z_adjust_reset_command(false), std::string("SET_GCODE_OFFSET Z=0 MOVE=1"));
}

TEST(Command, bad_step_sends_nothing) {
  ASSERT_EQ(z_adjust_step_command(true, true, ""), std::string(""));
  ASSERT_EQ(z_adjust_step_command(true, true, "abc"), std::string(""));
  ASSERT_EQ(z_adjust_step_command(false, false, "0"), std::string(""));
  ASSERT_EQ(z_adjust_step_command(true, false, "-0.01"), std::string(""));
  ASSERT_EQ(z_adjust_step_command(true, true, "0.01; G28"), std::string(""));
}

TEST(Text, saved_value) {
  json st = {{"value", 0.05}, {"applied", 0.05}, {"error", nullptr}};
  ASSERT_EQ(z_adjust_text(true, st), std::string("Z offset adjustment: 0.050 mm, saved"));
  st["value"] = -0.125;
  st["error"] = "";
  ASSERT_EQ(z_adjust_text(true, st), std::string("Z offset adjustment: -0.125 mm, saved"));
  st["value"] = -0.0001;
  ASSERT_EQ(z_adjust_text(true, st), std::string("Z offset adjustment: 0.000 mm, saved"));
}

TEST(Text, save_error_is_never_saved) {
  json st = {{"value", 0.02}, {"applied", 0.02}, {"error", "file not writable"}};
  ASSERT_EQ(z_adjust_text(true, st),
            std::string("Z offset adjustment: 0.020 mm, NOT saved: file not writable"));
}

TEST(Text, no_saved_claim_without_the_object) {
  json st = {{"value", 0.05}};
  ASSERT_EQ(z_adjust_text(false, st), std::string(""));
  ASSERT_EQ(z_adjust_text(true, json()), std::string(""));
  ASSERT_EQ(z_adjust_text(true, json{{"value", "0.05"}}), std::string(""));
}

int main() {
  return minitest::run_all("nebulaos_z_adjust") > 0 ? 1 : 0;
}
