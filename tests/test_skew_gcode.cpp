// Offline tests for src/skew_gcode.h (#31, 2026-11-22). Build/run:
//   make test-skew-gcode
//
// This file may be distributed under the terms of the GNU GPLv3 license.
#include "minitest.h"
#include "../src/skew_gcode.h"

TEST(SkewGcode, apply_saves_profile_then_config) {
  ASSERT_EQ(skew_apply_gcode("140.4", "141.9", "99.8", ""),
            std::string("SET_SKEW XY=140.4,141.9,99.8\n"
                        "SKEW_PROFILE SAVE=nebulaos\n"
                        "SAVE_CONFIG"));
}

TEST(SkewGcode, apply_keeps_history_record_before_save_config) {
  std::string g = skew_apply_gcode("1", "2", "3", "REC X=1\n");
  ASSERT_EQ(g, std::string("SET_SKEW XY=1,2,3\nSKEW_PROFILE SAVE=nebulaos\nREC X=1\nSAVE_CONFIG"));
}

TEST(SkewGcode, clear_removes_profile) {
  ASSERT_EQ(skew_clear_gcode(),
            std::string("SET_SKEW CLEAR=1\nSKEW_PROFILE REMOVE=nebulaos\nSAVE_CONFIG"));
}

TEST(SkewGcode, status_follows_current_profile_name) {
  ASSERT_TRUE(skew_is_on({{"current_profile_name", "nebulaos"}}));
  ASSERT_FALSE(skew_is_on({{"current_profile_name", ""}}));
  ASSERT_FALSE(skew_is_on({{"current_profile_name", "other"}}));
  ASSERT_FALSE(skew_is_on(nlohmann::json()));
  ASSERT_FALSE(skew_is_on({{"current_profile_name", nullptr}}));
}

int main() {
  return minitest::run_all("skew_gcode") > 0 ? 1 : 0;
}
