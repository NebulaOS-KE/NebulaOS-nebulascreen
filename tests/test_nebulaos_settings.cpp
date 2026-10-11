// Offline tests for the Connections, Wi-Fi and Updates screen logic (S1, Q3) -
// no LVGL, no Moonraker. Build/run:
//   make test-nebulaos-settings
//
// This file may be distributed under the terms of the GNU GPLv3 license.
#include "minitest.h"
#include "../src/nebulaos_settings.h"

using json = nlohmann::json;

static json net_status(const char *mode) {
  return {{"result", {{"config", {{"hostname", "nebulaos"}, {"ipv4", {{"mode", mode}}}}},
                      {"live", {{"address", "192.168.0.98"}, {"ssid", "Office_2.4Ghz"},
                                {"hostname", "nebulaos"}}}}}};
}

TEST(Connections, full) {
  json cfg = {{"result", {{"config", {{"server", {{"port", 7125}}},
                                      {"spoolman", {{"server", "http://192.168.0.20:7912"}}}}}}}};
  auto v = connections_view(net_status("dhcp"), cfg);
  ASSERT_EQ(v.wifi, std::string("Office_2.4Ghz"));
  ASSERT_EQ(v.address, std::string("192.168.0.98 (DHCP)"));
  ASSERT_EQ(v.name, std::string("nebulaos.local"));
  ASSERT_EQ(v.mainsail, std::string("http://192.168.0.98"));
  ASSERT_EQ(v.moonraker, std::string("192.168.0.98:7125"));
  ASSERT_EQ(v.spoolman, std::string("http://192.168.0.20:7912"));
}

TEST(Connections, static_no_spoolman_no_config) {
  auto v = connections_view(net_status("static"), json());
  ASSERT_EQ(v.address, std::string("192.168.0.98 (static)"));
  ASSERT_EQ(v.moonraker, std::string("192.168.0.98:7125"));  // default port
  ASSERT_EQ(v.spoolman, std::string(""));
}

TEST(Connections, offline) {
  auto v = connections_view(json{{"error", {{"message", "x"}}}}, json());
  ASSERT_EQ(v.wifi, std::string("not connected"));
  ASSERT_EQ(v.address, std::string("-"));
  ASSERT_EQ(v.mainsail, std::string(""));
}

// Shape of machine.nebulaos.wifi.scan (nebulaos_osconfig/wifi.py scan()).
static json scan_reply() {
  return json::parse(R"({"result":{"complete":true,
    "status":{"state":"COMPLETED","ssid":"Home","id":0,"address":"192.168.0.98"},
    "networks":[
      {"ssid":"Home","signal":-48,"frequency":2412,"security":"psk","saved":true,"current":true},
      {"ssid":"Cafe","signal":-61,"frequency":2437,"security":"open","saved":false,"current":false},
      {"ssid":"Corp","signal":-66,"frequency":2462,"security":"eap","saved":false,"current":false},
      {"ssid":"","signal":-40,"security":"open"},
      {"ssid":"Neighbour","signal":-80,"frequency":2412,"security":"psk","saved":false,"current":false}]}})");
}

TEST(Wifi, networks) {
  auto n = wifi_networks(scan_reply());
  ASSERT_EQ(n.size(), (size_t)4);
  ASSERT_EQ(n[0].ssid, std::string("Home"));
  ASSERT_TRUE(n[0].saved && n[0].current);
  ASSERT_FALSE(wifi_needs_password(n[0]));   // saved
  ASSERT_FALSE(wifi_needs_password(n[1]));   // open
  ASSERT_TRUE(wifi_supported(n[1]));
  ASSERT_FALSE(wifi_supported(n[2]));        // enterprise
  ASSERT_TRUE(wifi_needs_password(n[3]));
  ASSERT_EQ(wifi_signal_text(n[0].signal), std::string("Excellent"));
  ASSERT_EQ(wifi_signal_text(n[3].signal), std::string("Weak"));
}

TEST(Wifi, current_and_saved) {
  auto c = wifi_current(scan_reply());
  ASSERT_TRUE(c.connected);
  ASSERT_EQ(c.ssid, std::string("Home"));
  ASSERT_EQ(c.address, std::string("192.168.0.98"));
  json st = json::parse(R"({"result":{"status":{"state":"SCANNING","ssid":null},
    "saved":[{"id":0,"ssid":"Home","current":false,"disabled":false},{"id":1,"ssid":"Old"}]}})");
  ASSERT_FALSE(wifi_current(st).connected);
  auto saved = wifi_saved(st);
  ASSERT_EQ(saved.size(), (size_t)2);
  ASSERT_EQ(saved[1], std::string("Old"));
}

TEST(Wifi, errors) {
  // Moonraker passes the helper's "CODE: message" through as the error message.
  json wrong = {{"error", {{"code", 500}, {"message",
    "WRONG_PASSWORD: the password was not accepted; the previous Wi-Fi network was restored"}}}};
  ASSERT_EQ(wifi_error_text(wrong), std::string("Wrong password. The previous network is connected again."));
  json other = {{"error", {{"code", 500}, {"message", "SOMETHING_NEW: details"}}}};
  ASSERT_EQ(wifi_error_text(other), std::string("SOMETHING_NEW: details"));
  ASSERT_EQ(wifi_error_text(json{{"result", {{"result", "CONNECTED"}}}}), std::string(""));
  auto p = wifi_connect_params("Home", "pa\"ss");
  ASSERT_EQ(p["password"].get<std::string>(), std::string("pa\"ss"));
}

// Shape of machine.update.status (Moonraker update_manager, git_deploy/net_deploy).
static json update_reply() {
  return json::parse(R"({"result":{"busy":false,"github_rate_limit":60,
    "version_info":{
      "system":{"package_count":3},
      "klipper":{"version":"v0.13.0-58","remote_version":"v0.13.0-58","is_valid":true,
                 "corrupt":false,"commits_behind_count":0,"commits_behind":[]},
      "moonraker":{"version":"v0.9.3-12","remote_version":"v0.9.3-15","is_valid":true,
                   "corrupt":false,"commits_behind_count":3,"commits_behind":[{},{},{}]},
      "mainsail":{"version":"v2.13.0","remote_version":"v2.14.0","name":"mainsail"},
      "nebulaos-klipper-extensions":{"version":"8f86925","remote_version":"8f86925",
                   "is_valid":false,"corrupt":false,"commits_behind_count":0}}}})");
}

TEST(Updates, rows) {
  auto rows = update_rows(update_reply());
  ASSERT_EQ(rows.size(), (size_t)4);            // "system" left out
  ASSERT_EQ(rows[0].label, std::string("Klipper"));
  ASSERT_FALSE(rows[0].available);
  ASSERT_EQ(rows[0].state, std::string("up to date"));
  ASSERT_EQ(rows[1].label, std::string("Mainsail"));
  ASSERT_TRUE(rows[1].available);
  ASSERT_EQ(rows[1].state, std::string("v2.14.0 available"));
  ASSERT_EQ(rows[2].label, std::string("Moonraker"));
  ASSERT_EQ(rows[2].state, std::string("3 new"));
  ASSERT_EQ(rows[3].label, std::string("NebulaOS extensions"));
  ASSERT_FALSE(rows[3].valid);
  ASSERT_FALSE(rows[3].available);
  ASSERT_FALSE(updates_busy(update_reply()));
  ASSERT_TRUE(update_rows(json()).empty());
}

TEST(Updates, unknown_remote_is_not_an_update) {
  json r = {{"result", {{"version_info", {{"mainsail", {{"version", "v2.13.0"}, {"remote_version", "?"}}}}}}}};
  auto rows = update_rows(r);
  ASSERT_FALSE(rows[0].available);
}

int main() {
  return minitest::run_all("nebulaos_settings") > 0 ? 1 : 0;
}
