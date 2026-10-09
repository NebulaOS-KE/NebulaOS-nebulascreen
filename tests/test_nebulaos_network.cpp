// Offline tests for the static IP panel's NebulaOS network contract helpers - no LVGL, no
// WebSocket, no Moonraker. Build/run:
//   make test-nebulaos-network
//
// This file may be distributed under the terms of the GNU GPLv3 license.
#include "minitest.h"
#include "../src/nebulaos_network.h"

using json = nlohmann::json;

// Verbatim result from GET /machine/nebulaos/network/status on ke-dev, 2026-10-09.
static json live_reply() {
  return json::parse(R"({"jsonrpc":"2.0","id":3,"result":{"boot_state":"DHCP","config":{"hostname":"nebulaos","ipv4":{"mode":"dhcp"},"schema":1},"config_error":null,"last_known_good":null,"live":{"address":"192.168.0.98","dns":["192.168.0.100"],"gateway":"192.168.0.1","hostname":"nebulaos","interface":"wlan0","prefix":24,"ssid":"Office_2.4Ghz","wpa_state":"COMPLETED"},"mdns_name":"nebulaos.local","mdns_note":"x"}})");
}

TEST(Netmask, prefix_roundtrip) {
  ASSERT_EQ(netmask_to_prefix("255.255.255.0"), 24);
  ASSERT_EQ(netmask_to_prefix("255.255.252.0"), 22);
  ASSERT_EQ(netmask_to_prefix("255.0.255.0"), -1);
  ASSERT_EQ(netmask_to_prefix("nonsense"), -1);
  ASSERT_EQ(prefix_to_netmask(24), std::string("255.255.255.0"));
  ASSERT_EQ(prefix_to_netmask(30), std::string("255.255.255.252"));
  ASSERT_EQ(prefix_to_netmask(-1), std::string(""));
}

TEST(Validate, accepts_a_normal_entry) {
  ASSERT_EQ(validate_static_entry("192.168.0.50", "255.255.255.0", "192.168.0.1"), std::string(""));
}

TEST(Validate, rejects_like_the_server) {
  ASSERT_TRUE(!validate_static_entry("192.168.0.50", "255.255.255.0", "10.0.0.1").empty());
  ASSERT_TRUE(!validate_static_entry("192.168.0.1", "255.255.255.0", "192.168.0.1").empty());
  ASSERT_TRUE(!validate_static_entry("192.168.0.0", "255.255.255.0", "192.168.0.1").empty());
  ASSERT_TRUE(!validate_static_entry("192.168.0.255", "255.255.255.0", "192.168.0.1").empty());
  ASSERT_TRUE(!validate_static_entry("192.168.0.50", "255.255.255.254", "192.168.0.51").empty());
  ASSERT_TRUE(!validate_static_entry("10.1.2.3", "254.0.0.0", "10.0.0.1").empty());
}

TEST(Status, dhcp_shows_live_values) {
  auto v = parse_network_status(live_reply());
  ASSERT_TRUE(v.ok);
  ASSERT_FALSE(v.configured_static);
  ASSERT_EQ(v.address, std::string("192.168.0.98"));
  ASSERT_EQ(v.netmask, std::string("255.255.255.0"));
  ASSERT_EQ(v.gateway, std::string("192.168.0.1"));
  ASSERT_EQ(v.dns, std::string("192.168.0.100"));
}

TEST(Status, static_shows_configured_values) {
  json j = live_reply();
  j["result"]["config"]["ipv4"] = {{"mode", "static"}, {"address", "192.168.0.50"},
                                   {"prefix", 22}, {"gateway", "192.168.0.1"},
                                   {"dns", {"1.1.1.1"}}};
  auto v = parse_network_status(j);
  ASSERT_TRUE(v.configured_static);
  ASSERT_EQ(v.address, std::string("192.168.0.50"));
  ASSERT_EQ(v.netmask, std::string("255.255.252.0"));
  ASSERT_EQ(v.dns, std::string("1.1.1.1"));
  ASSERT_EQ(v.live_address, std::string("192.168.0.98"));
}

TEST(Status, error_reply) {
  auto v = parse_network_status({{"error", {{"code", 404}, {"message", "Not Found"}}}});
  ASSERT_FALSE(v.ok);
  ASSERT_EQ(v.error, std::string("Not Found"));
}

TEST(Status, missing_live_fields_do_not_throw) {
  json j = live_reply();
  j["result"]["live"] = {{"address", nullptr}, {"prefix", nullptr}, {"dns", json::array()}};
  auto v = parse_network_status(j);
  ASSERT_TRUE(v.ok);
  ASSERT_EQ(v.address, std::string(""));
  ASSERT_EQ(v.netmask, std::string(""));
}

TEST(Params, static_and_dhcp) {
  json p = static_ipv4_params("192.168.0.50", "255.255.255.0", "192.168.0.1", "192.168.0.1");
  ASSERT_EQ(p["mode"].get<std::string>(), std::string("static"));
  ASSERT_EQ(p["prefix"].get<int>(), 24);
  ASSERT_EQ(p["dns"].size(), (size_t)1);
  ASSERT_EQ(dhcp_ipv4_params().dump(), std::string(R"({"mode":"dhcp"})"));
}

TEST(Outcome, applied) {
  json r = {{"result", {{"result", "APPLIED"}}}};
  ASSERT_TRUE(ipv4_apply_outcome(r, true).success);
}

TEST(Outcome, reverted_codes_explain_the_revert) {
  json g = {{"error", {{"code", 500}, {"message", "GATEWAY_UNREACHABLE_REVERTED: the new network settings did not work"}}}};
  auto o = ipv4_apply_outcome(g, true);
  ASSERT_FALSE(o.success);
  ASSERT_TRUE(o.message.find("gateway did not answer") != std::string::npos);
  json d = {{"error", {{"code", 500}, {"message", "DHCP_FAILED_REVERTED: x"}}}};
  ASSERT_TRUE(ipv4_apply_outcome(d, false).message.find("No DHCP address") != std::string::npos);
  json i = {{"error", {{"code", 500}, {"message", "IFUP_FAILED_REVERTED: x"}}}};
  ASSERT_TRUE(ipv4_apply_outcome(i, true).message.find("restored") != std::string::npos);
}

TEST(Outcome, validation_error_is_shown) {
  json e = {{"error", {{"code", 400}, {"message", "INVALID_GATEWAY: gateway 10.0.0.1 is not in 192.168.0.0/24"}}}};
  auto o = ipv4_apply_outcome(e, true);
  ASSERT_FALSE(o.success);
  ASSERT_TRUE(o.message.find("INVALID_GATEWAY") != std::string::npos);
}

int main() {
  return minitest::run_all("nebulaos_network") > 0 ? 1 : 0;
}
