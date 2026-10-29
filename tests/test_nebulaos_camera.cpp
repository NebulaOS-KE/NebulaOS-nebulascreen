// Offline tests for src/nebulaos_camera.cpp (G121, Run 22). Build/run:
//   make test-nebulaos-camera
//
// This file may be distributed under the terms of the GNU GPLv3 license.
#include "minitest.h"
#include "../src/nebulaos_camera.h"

TEST(Camera, options_match_values) {
  ASSERT_EQ(camera_ir_values().size(), (size_t)3);
  ASSERT_EQ(camera_index_of(camera_ir_values(), "bw"), 2);
  ASSERT_EQ(camera_index_of(camera_flicker_values(), "50"), 1);
  ASSERT_EQ(camera_index_of(camera_flicker_values(), "nonsense"), 0);
  ASSERT_EQ(std::string(CAMERA_IR_OPTIONS), std::string("Auto\nAlways colour\nAlways black-and-white"));
}

TEST(Camera, sliders_have_the_backend_ranges) {
  const auto &s = camera_sliders();
  ASSERT_EQ(s.size(), (size_t)7);
  ASSERT_EQ(std::string(s[0].key), std::string("brightness"));
  ASSERT_EQ(s[0].min, 50);
  ASSERT_EQ(s[0].max, 160);
  ASSERT_EQ(s[4].min, 100);
  ASSERT_EQ(s[6].max, 1000);
}

TEST(Camera, view) {
  json st = json::parse(R"({"result":{"config":{"schema":1,"ir_mode":"bw","brightness":140,
    "wb_auto":false},"defaults":{"brightness":128},"present":true,"firmware":"V020"}})");
  auto v = camera_view(st);
  ASSERT_TRUE(v.ok);
  ASSERT_TRUE(v.present);
  ASSERT_EQ(v.firmware, std::string("V020"));
  ASSERT_EQ(camera_int(v.config, "brightness", 0), 140);
  ASSERT_EQ(camera_int(v.config, "zoom", 100), 100);
  ASSERT_FALSE(camera_bool(v.config, "wb_auto", true));
  ASSERT_EQ(camera_str(v.config, "ir_mode", "auto"), std::string("bw"));
  ASSERT_FALSE(camera_view(json::parse(R"({"error":{"message":"x"}})")).ok);
}

TEST(Camera, params_and_results) {
  json p = camera_settings_params({{"ir_mode", "color"}});
  ASSERT_EQ(p["settings"]["ir_mode"].get<std::string>(), std::string("color"));
  ASSERT_EQ(camera_result_text(json::parse(R"({"result":{"result":"APPLIED"}})")), std::string(""));
  ASSERT_EQ(camera_result_text(json::parse(R"({"result":{"result":"SAVED_APPLIES_LATER"}})")),
            std::string("Saved - applied when the camera is connected."));
  ASSERT_EQ(camera_result_text(json::parse(R"({"error":{"message":"EXPOSURE_ONE_WAY: x"}})")),
            std::string("Automatic exposure comes back only with Reset camera."));
  ASSERT_EQ(camera_result_text(json::parse(R"({"error":{"message":"PRINTING: A print is running."}})")),
            std::string("A print is running."));
  ASSERT_EQ(camera_version_of(json::parse(R"({"result":{"firmware":"UVC-V020"}})")), std::string("UVC-V020"));
}

int main() {
  return minitest::run_all("nebulaos_camera") > 0 ? 1 : 0;
}
