// Offline tests for the NebulaOS display contract helpers - no LVGL, no WebSocket, no
// Moonraker. Build/run:
//   make test-nebulaos-display
//
// This file may be distributed under the terms of the GNU GPLv3 license.
#include "minitest.h"
#include "../src/nebulaos_display.h"

using json = nlohmann::json;

// Verbatim result from GET /machine/nebulaos/display/status on ke-dev, 2026-10-09.
static json live_reply() {
  return json::parse(R"({"jsonrpc":"2.0","id":7,"result":{"backlight":{"asleep":false,"available":true,"interface":"sysfs","level":100,"selected_level":100,"touch_wake_running":false},"config":{"brightness":100,"schema":1,"touch_wake":true},"config_error":null,"interface":"sysfs","levels":[25,50,75,100]}})");
}

TEST(Parse, live_reply) {
  auto s = parse_display_status(live_reply());
  ASSERT_TRUE(s.available);
  ASSERT_EQ(s.levels.size(), (size_t)4);
  ASSERT_EQ(s.brightness, 100);
}

TEST(Parse, error_reply_is_unavailable) {
  json j = {{"error", {{"code", 404}, {"message", "Not Found"}}}};
  ASSERT_FALSE(parse_display_status(j).available);
}

TEST(Parse, no_backlight_is_unavailable) {
  json j = live_reply();
  j["result"]["backlight"]["available"] = false;
  ASSERT_FALSE(parse_display_status(j).available);
}

TEST(Parse, no_levels_is_unavailable) {
  json j = live_reply();
  j["result"]["levels"] = json::array();
  ASSERT_FALSE(parse_display_status(j).available);
}

TEST(Parse, bad_types_are_ignored) {
  json j = live_reply();
  j["result"]["levels"] = {25, "50", -1, 75.5, 100};
  j["result"]["config"]["brightness"] = "high";
  auto s = parse_display_status(j);
  ASSERT_EQ(s.levels.size(), (size_t)2);
  ASSERT_EQ(s.brightness, -1);
}

TEST(Dropdown, options_follow_levels) {
  ASSERT_EQ(brightness_dropdown_options(parse_display_status(live_reply())),
            std::string("25%\n50%\n75%\n100%"));
}

TEST(Dropdown, index_of_persisted_level) {
  json j = live_reply();
  j["result"]["config"]["brightness"] = 50;
  ASSERT_EQ(brightness_dropdown_index(parse_display_status(j)), 1);
}

TEST(Dropdown, unknown_level_selects_highest) {
  json j = live_reply();
  j["result"]["config"]["brightness"] = 60;
  ASSERT_EQ(brightness_dropdown_index(parse_display_status(j)), 3);
}

TEST(Errors, message) {
  ASSERT_EQ(jsonrpc_error_message(live_reply()), std::string(""));
  json e = {{"error", {{"code", 500}, {"message", "SLEEP_UNAVAILABLE: no touch-wake"}}}};
  ASSERT_EQ(jsonrpc_error_message(e), std::string("SLEEP_UNAVAILABLE: no touch-wake"));
}

TEST(Methods, names) {
  ASSERT_EQ(std::string(DISPLAY_STATUS_METHOD), std::string("machine.nebulaos.display.status"));
  ASSERT_EQ(std::string(DISPLAY_BRIGHTNESS_METHOD), std::string("machine.nebulaos.display.brightness"));
  ASSERT_EQ(std::string(DISPLAY_SLEEP_METHOD), std::string("machine.nebulaos.display.sleep"));
  ASSERT_EQ(std::string(DISPLAY_WAKE_METHOD), std::string("machine.nebulaos.display.wake"));
}

int main() {
  return minitest::run_all("nebulaos_display") > 0 ? 1 : 0;
}
