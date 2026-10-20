// Offline tests for the NebulaScreen rename's legacy config normalization.
// Build/run: make test-nebulascreen-paths
//
// This file may be distributed under the terms of the GNU GPLv3 license.
#include "minitest.h"
#include "../src/nebulascreen_paths.h"

using json = nlohmann::json;
using namespace nebulascreen;

// What a migrated guppyconfig.json from ke-dev looks like (the relevant keys).
static json legacy() {
  return json::parse(R"({
    "log_path": "/opt/printer_data/logs/guppyscreen.log",
    "guppy_init_script": "/etc/init.d/S58guppyscreen",
    "thumbnail_path": "/opt/guppyscreen/thumbnails",
    "config_path": "/opt/guppyscreen/guppyconfig.json",
    "display_rotate": 2,
    "wpa_supplicant": "/var/run/wpa_supplicant",
    "printers": {"k1": {"moonraker_host": "127.0.0.1"}}
  })");
}

TEST(Legacy, maps_every_old_path) {
  json c = legacy();
  ASSERT_TRUE(normalize_legacy_config(c));
  ASSERT_EQ(c["log_path"].get<std::string>(), std::string(kLogPath));
  ASSERT_EQ(c["thumbnail_path"].get<std::string>(), std::string(kThumbnailPath));
  ASSERT_EQ(c["config_path"].get<std::string>(), std::string(kConfigPath));
  ASSERT_EQ(c[kInitScriptKey].get<std::string>(), std::string(kInitScript));
  ASSERT_FALSE(c.contains("guppy_init_script"));
}

TEST(Legacy, leaves_user_settings_alone) {
  json c = legacy();
  normalize_legacy_config(c);
  ASSERT_EQ(c["display_rotate"].get<int>(), 2);
  ASSERT_EQ(c["wpa_supplicant"].get<std::string>(), std::string("/var/run/wpa_supplicant"));
  ASSERT_EQ(c["printers"]["k1"]["moonraker_host"].get<std::string>(), std::string("127.0.0.1"));
}

TEST(Legacy, only_exact_legacy_values_change) {
  json c = json::parse(R"({"log_path": "/mnt/usb/my.log",
                           "guppy_init_script": "/etc/init.d/S99mine"})");
  normalize_legacy_config(c);
  ASSERT_EQ(c["log_path"].get<std::string>(), std::string("/mnt/usb/my.log"));
  // a custom script keeps its value under the new key
  ASSERT_EQ(c[kInitScriptKey].get<std::string>(), std::string("/etc/init.d/S99mine"));
}

TEST(Legacy, new_key_wins_over_old_key) {
  json c = json::parse(R"({"nebulascreen_init_script": "/etc/init.d/S58nebulascreen",
                           "guppy_init_script": "/etc/init.d/S58guppyscreen"})");
  normalize_legacy_config(c);
  ASSERT_EQ(c[kInitScriptKey].get<std::string>(), std::string(kInitScript));
  ASSERT_FALSE(c.contains("guppy_init_script"));
}

TEST(Legacy, already_new_config_is_unchanged) {
  json c = legacy();
  normalize_legacy_config(c);
  json again = c;
  ASSERT_FALSE(normalize_legacy_config(again));
  ASSERT_TRUE(again == c);
  json not_object = json::array();
  ASSERT_FALSE(normalize_legacy_config(not_object));
}

TEST(Names, contract) {
  ASSERT_EQ(std::string(kService), std::string("nebulascreen"));
  ASSERT_EQ(std::string(kLegacyDbNamespace), std::string("guppyscreen"));
  ASSERT_EQ(std::string(kInitScript), std::string("/etc/init.d/S58nebulascreen"));
  ASSERT_TRUE(std::string(kService).size() <= 15);   // process comm limit
}

TEST(UiApi, contract_levels) {
  ASSERT_EQ(ui_api_problem(json::parse(R"({"nebulaos_ui_api": 1})")), std::string(""));
  ASSERT_TRUE(ui_api_problem(json::parse(R"({"nebulaos_ui_api": 2})")).find("needs image contract 1")
              != std::string::npos);
  ASSERT_FALSE(ui_api_problem(json::parse(R"({"nebulaos_ui_api": "1"})")).empty());
  // an image that states no level (or an unreadable file) is accepted
  ASSERT_EQ(ui_api_problem(json::parse(R"({"version": "0.9.0"})")), std::string(""));
  ASSERT_EQ(ui_api_problem(json()), std::string(""));
}

TEST(Report, params) {
  json p = report_params();
  ASSERT_EQ(p["name"].get<std::string>(), std::string("nebulascreen"));
  ASSERT_EQ(p["version"].get<std::string>(), std::string("dev-snapshot"));
  ASSERT_EQ(p["nebulaos_ui_api"].get<int>(), kRequiredUiApi);
  ASSERT_EQ(std::string(kReportMethod), std::string("machine.nebulaos.screen.report"));
}

int main() {
  return minitest::run_all("nebulascreen_paths") > 0 ? 1 : 0;
}
