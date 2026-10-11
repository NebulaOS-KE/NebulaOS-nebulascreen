// Offline tests for the Connections, Wi-Fi, Updates, printer-name and MCU
// firmware screen logic (S1, Q3, run 4) -
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

// -- Printer name (run 4) ---------------------------------------------------------

TEST(Hostname, check) {
  auto ok = hostname_check("  KE-Workshop ", "nebulaos");
  ASSERT_TRUE(ok.ok);
  ASSERT_EQ(ok.name, std::string("ke-workshop"));
  ASSERT_EQ(ok.message, std::string("Reachable as ke-workshop.local after saving."));
  ASSERT_TRUE(hostname_check(std::string(63, 'a'), "nebulaos").ok);
  struct Bad { const char *in; const char *msg; };
  Bad bad[] = {
    {"", "Enter a name."}, {"   ", "Enter a name."},
    {"a_b", "Use only letters a-z, digits 0-9 and '-'."},
    {"a.b", "Use only letters a-z, digits 0-9 and '-'."},
    {"a b", "Use only letters a-z, digits 0-9 and '-'."},
    {"-ke", "The name cannot start or end with '-'."},
    {"ke-", "The name cannot start or end with '-'."},
    {"1234", "The name needs at least one letter."},
    {"LocalHost", "'localhost' is reserved."},
    {"NebulaOS", "This is the current name."},
  };
  for (const auto &b : bad) {
    auto c = hostname_check(b.in, "nebulaos");
    ASSERT_FALSE(c.ok);
    ASSERT_EQ(c.message, std::string(b.msg));
  }
  ASSERT_EQ(hostname_check(std::string(64, 'a'), "x").message, std::string("At most 63 characters."));
  ASSERT_EQ(hostname_params("ke")["hostname"].get<std::string>(), std::string("ke"));
  ASSERT_EQ(moonraker_restart_params()["service"].get<std::string>(), std::string("moonraker"));
}

TEST(Hostname, reply) {
  // Shape of machine.nebulaos.network.hostname (network.py set_hostname).
  json applied = json::parse(R"({"result":{"result":"APPLIED","mdns_restart_required":true,
    "config":{"hostname":"ke"}}})");
  ASSERT_TRUE(hostname_restart_needed(applied));
  ASSERT_EQ(hostname_error_text(applied), std::string(""));
  ASSERT_FALSE(hostname_restart_needed(json::parse(R"({"result":{"result":"UNCHANGED"}})")));
  json invalid = {{"error", {{"code", 400}, {"message", "INVALID_HOSTNAME: hostname must not be 'localhost'"}}}};
  ASSERT_EQ(hostname_error_text(invalid),
            std::string("The printer refused the name: hostname must not be 'localhost'"));
  json failed = {{"error", {{"code", 500}, {"message", "HOSTNAME_APPLY_FAILED: could not apply the hostname"}}}};
  ASSERT_EQ(hostname_error_text(failed), std::string("Could not apply the name. The old name is kept."));
  ASSERT_EQ(connections_view(net_status("dhcp"), json()).hostname, std::string("nebulaos"));
}

// -- MCU firmware (run 4) --------------------------------------------------------

// Shape of machine.nebulaos.mcu.files (nebulaos_osconfig/mcu.py list_files()).
static json mcu_files_reply() {
  std::string sha(64, 'a');
  return json::parse(R"({"result":{"usb":["sda1"],"upload_dir":"/x",
    "builtin":{"id":"builtin/candidate-001.bin","source":"builtin","name":"NebulaOS firmware",
               "sha256":")" + std::string(64, 'c') + R"(","native":true,"size":40048,"valid":true,"kind":"packed"},
    "files":[
      {"id":"upload/junk.bin","source":"upload","name":"junk.bin","size":4096,"valid":false,
       "kind":null,"sha256":")" + std::string(64, 'b') + R"(","native":false,
       "problem":"not firmware for this printer's mainboard"},
      {"id":"upload/klipper.bin","source":"upload","name":"klipper.bin","size":40048,"valid":true,
       "kind":"raw","sha256":")" + sha + R"(","native":false,"problem":null},
      {"id":"usb/sda1/KE.BIN","source":"usb","name":"KE.BIN","size":40048,"valid":true,
       "kind":"packed","sha256":")" + std::string(64, 'c') + R"(","native":true,"problem":null}]}})");
}

TEST(Mcu, files) {
  auto f = mcu_files(mcu_files_reply());
  ASSERT_EQ(f.files.size(), (size_t)3);
  ASSERT_TRUE(f.usb_present);
  ASSERT_TRUE(f.has_builtin);
  ASSERT_EQ(f.builtin.id, std::string("builtin/candidate-001.bin"));
  ASSERT_FALSE(f.files[0].valid);
  ASSERT_EQ(f.files[0].detail, std::string("Uploaded  -  4.0 KB  -  not firmware for this printer's mainboard"));
  ASSERT_EQ(f.files[1].detail, std::string("Uploaded  -  39.1 KB  -  ready (raw build, packed on install)"));
  ASSERT_EQ(f.files[2].detail, std::string("USB  -  39.1 KB  -  NebulaOS firmware"));
  auto p = mcu_flash_params(f.files[1]);
  ASSERT_EQ(p["file"].get<std::string>(), std::string("upload/klipper.bin"));
  ASSERT_EQ(p["sha256"].get<std::string>(), std::string(64, 'a'));
  ASSERT_TRUE(mcu_confirm_text(f.files[1]).find("another Klipper version") != std::string::npos);
  ASSERT_TRUE(mcu_confirm_text(f.builtin).find("the NebulaOS firmware") != std::string::npos);
  ASSERT_TRUE(mcu_files(json{{"error", {{"message", "x"}}}}).files.empty());
  ASSERT_EQ(human_size(512), std::string("512 B"));
}

TEST(Mcu, status) {
  json st = json::parse(R"({"result":{"managed":false,"guard":{"result":"WARN",
    "application_class":"UNKNOWN_APPLICATION","application_identity":"v0.13.0-99"},
    "last_flash":{"file":"usb/sda1/KE.BIN","ok":true}}})");
  auto v = mcu_status_view(st);
  ASSERT_EQ(v.running, std::string("custom firmware (v0.13.0-99)"));
  ASSERT_EQ(v.ownership, std::string("you (custom firmware)"));
  ASSERT_EQ(v.last, std::string("KE.BIN, installed"));
  auto n = mcu_status_view(json::parse(R"({"result":{"managed":true,"guard":null,"last_flash":null}})"));
  ASSERT_EQ(n.running, std::string("not checked yet this boot"));
  ASSERT_EQ(n.last, std::string(""));
  ASSERT_EQ(mcu_status_view(json()).running, std::string("unknown (no answer)"));
}

TEST(Mcu, flash_result) {
  auto ok = mcu_flash_result(json::parse(R"({"result":{"result":"FLASHED","native":false}})"));
  ASSERT_TRUE(ok.ok);
  ASSERT_EQ(ok.title, std::string("Firmware installed"));
  auto busy = mcu_flash_result({{"error", {{"message", "PRINTING: not possible while a print is active"}}}});
  ASSERT_FALSE(busy.ok);
  ASSERT_EQ(busy.title, std::string("Not installed"));
  auto bad = mcu_flash_result({{"error", {{"message", "INVALID_FIRMWARE: too small for MCU firmware (100 bytes)"}}}});
  ASSERT_EQ(bad.text, std::string("This file cannot be installed: too small for MCU firmware (100 bytes). Nothing was changed."));
  auto failed = mcu_flash_result({{"error", {{"message", "FLASH_FAILED: Could not enter bootloader: timeout"}}}});
  ASSERT_EQ(failed.title, std::string("Install failed"));
  ASSERT_TRUE(failed.text.find("Could not enter bootloader: timeout") == 0);
  auto gave_up = mcu_flash_result({{"error", {{"message", "TIMEOUT: no answer from the printer"}}}});
  ASSERT_EQ(gave_up.title, std::string("No answer"));
  ASSERT_EQ(gave_up.text, std::string("no answer from the printer."));
}

TEST(Wifi, connected_text) {
  bool ok = false;
  json dhcp = json::parse(R"({"result":{"result":"CONNECTED","ssid":"Office_2.4Ghz",
    "address":"192.168.0.98","address_mode":"dhcp","address_ok":true,"address_note":null}})");
  ASSERT_EQ(wifi_connected_text(dhcp, "Office_2.4Ghz", &ok),
            std::string("Connected to\nOffice_2.4Ghz\nIP: 192.168.0.98"));
  ASSERT_TRUE(ok);
  json bad = json::parse(R"({"result":{"result":"CONNECTED","address":"10.0.0.5",
    "address_mode":"static","address_ok":false,
    "address_note":"the static address 10.0.0.5 does not work on this network"}})");
  std::string t = wifi_connected_text(bad, "Cafe", &ok);
  ASSERT_FALSE(ok);
  ASSERT_TRUE(t.find("does not work on this network") != std::string::npos);
  // Older NebulaOS without address_ok: treated as fine.
  json old = json::parse(R"({"result":{"result":"CONNECTED","address":null}})");
  ASSERT_TRUE(wifi_connected_text(old, "X", &ok).find("waiting for an address") != std::string::npos);
  ASSERT_TRUE(ok);
}

TEST(Updates, progress) {
  json n = json::parse(R"({"jsonrpc":"2.0","method":"notify_update_response","params":[
    {"application":"moonraker","proc_id":1,"message":"Updating Repo...\nGit Repo moonraker: Fetching\n","complete":false}]})");
  auto p = update_progress(n);
  ASSERT_TRUE(p.valid);
  ASSERT_FALSE(p.complete);
  ASSERT_EQ(p.text, std::string("Moonraker: Git Repo moonraker: Fetching"));
  json done = json::parse(R"({"params":[{"application":"klipper","message":"","complete":true}]})");
  p = update_progress(done);
  ASSERT_TRUE(p.valid && p.complete);
  ASSERT_EQ(p.text, std::string("Klipper: done"));
  ASSERT_FALSE(update_progress(json::object()).valid);
}

TEST(Mcu, version_shown_and_warned) {
  json files = json::parse(R"({"result":{"files":[
    {"id":"usb/sda1/old.bin","name":"old.bin","source":"usb","sha256":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
     "size":39000,"valid":true,"kind":"packed","native":false,"klipper_version":"v0.12.0-300-gbeef",
     "version_note":"built from Klipper v0.12.0, NebulaOS uses v0.13.0 - Klipper will probably not connect"},
    {"id":"upload/new.bin","name":"new.bin","source":"upload","sha256":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
     "size":39000,"valid":true,"kind":"raw","native":false,"klipper_version":"v0.13.0-1-gdead","version_note":null}]}})");
  auto m = mcu_files(files);
  ASSERT_EQ(m.files.size(), (size_t)2);
  ASSERT_TRUE(m.files[0].valid && m.files[0].warn);
  ASSERT_TRUE(m.files[0].detail.find("Klipper v0.12.0-300-gbeef") != std::string::npos);
  ASSERT_TRUE(mcu_confirm_text(m.files[0]).find("Warning: built from Klipper v0.12.0") != std::string::npos);
  ASSERT_FALSE(m.files[1].warn);
  ASSERT_TRUE(mcu_confirm_text(m.files[1]).find("Built from Klipper v0.13.0-1-gdead.") != std::string::npos);
}

int main() {
  return minitest::run_all("nebulaos_settings") > 0 ? 1 : 0;
}
