// Offline tests for the "Power & System" and Logs screen logic (D9) - no LVGL,
// no Moonraker. Build/run:
//   make test-nebulaos-system
//
// This file may be distributed under the terms of the GNU GPLv3 license.
#include "minitest.h"
#include "../src/nebulaos_system.h"

using json = nlohmann::json;

// Verbatim result from GET /machine/nebulaos/info on ke-dev (3580378), 2026-10-10.
static json info_reply() {
  return json::parse(R"({"jsonrpc":"2.0","id":1,"result":{"build":{"available":true,"build_date":"2026-10-10T02:43:29Z","firmware_head":"3580378d447da95e8313af6284c0bcfd77018ac5","guppyscreen_pin":"121d7dd","klipper":"58bd67d","moonraker":"d5ee171","nebulaos_klipper_extensions":"1b2ac56"},"hostname":"nebulaos","kernel":{"release":"6.6.157-rt79","version":"#1"},"sftp":{"available":true},"storage":{"boot_check":{"check_seconds":"4.11","e2fsck_rc":"1","mount":"rw","result":"REPAIRED","state_before":"not clean"},"free_bytes":1766465536,"mount":"/usr/data","read_only":false,"total_bytes":6314295296}}})");
}

static json boot_status(const char *mode, const char *image) {
  return {{"result", {{"config", {{"mode", mode}, {"image", image}, {"schema", 1}}}}}};
}

TEST(Info, live_reply) {
  auto s = system_info(info_reply());
  ASSERT_TRUE(s.ok);
  ASSERT_FALSE(s.storage_warning);  // REPAIRED is a normal outcome, not a warning
  ASSERT_TRUE(s.text.find("NebulaOS 3580378  (2026-10-10)") != std::string::npos);
  json v = info_reply();
  v["result"]["build"]["nebulaos_version"] = "0.9.0";
  ASSERT_TRUE(system_info(v).text.find("NebulaOS v0.9.0  (3580378, 2026-10-10)") != std::string::npos);
  ASSERT_TRUE(s.text.find("Kernel 6.6.157-rt79") != std::string::npos);
  ASSERT_TRUE(s.text.find("Storage 1.8 GB free of 6.3 GB") != std::string::npos);
  ASSERT_TRUE(s.text.find("nebulaos.local") != std::string::npos);
}

TEST(Info, read_only_and_failed_check_warn) {
  json j = info_reply();
  j["result"]["storage"]["read_only"] = true;
  ASSERT_TRUE(system_info(j).storage_warning);
  j = info_reply();
  j["result"]["storage"]["boot_check"]["result"] = "UNREPAIRED";
  auto s = system_info(j);
  ASSERT_TRUE(s.storage_warning);
  ASSERT_TRUE(s.text.find("UNREPAIRED") != std::string::npos);
}

TEST(Info, error_reply) {
  auto s = system_info({{"error", {{"code", 404}, {"message", "Not Found"}}}});
  ASSERT_FALSE(s.ok);
}

TEST(Bytes, human) {
  ASSERT_EQ(human_bytes(512e6), std::string("512 MB"));
  ASSERT_EQ(human_bytes(6314295296.0), std::string("6.3 GB"));
}

TEST(BootMode, index_and_value) {
}

TEST(Info, slot) {
  json j = info_reply();
  j["result"]["slot"] = "B";
  ASSERT_TRUE(system_info(j).text.find("(2026-10-10), slot B") != std::string::npos);
}

TEST(BootImage, list_filters_and_sorts) {
  json files = {{"result", json::array({{{"path", "zz.bmp"}}, {{"path", "logo.png"}},
                                        {{"path", "notes.txt"}}, {{"path", "sub/x.png"}},
                                        {{"path", "UP.PNG"}}})}};
  auto imgs = boot_images(files);
  ASSERT_EQ(imgs.size(), (size_t)3);
  ASSERT_EQ(imgs[0], std::string("UP.PNG"));
  ASSERT_EQ(boot_image_options(imgs), std::string("NebulaOS\nUP.PNG\nlogo.png\nzz.bmp"));
  ASSERT_EQ(boot_image_index(boot_status("logo", "logo.png"), imgs), 2);
  ASSERT_EQ(boot_image_index(boot_status("logo", "builtin"), imgs), 0);
}

TEST(Logs, list) {
  json files = {{"result", json::array({{{"path", "klippy.log"}}, {{"path", "moonraker.log.1"}},
                                        {{"path", "guppyscreen.log"}}, {{"path", "x.log.old"}},
                                        {{"path", "crash.txt"}}, {{"path", "sub/a.log"}}})}};
  auto logs = log_files(files);
  ASSERT_EQ(logs.size(), (size_t)3);
  ASSERT_EQ(logs[0], std::string("guppyscreen.log"));
  ASSERT_EQ(logs[2], std::string("moonraker.log.1"));
}

TEST(Logs, tail_starts_at_a_line) {
  ASSERT_EQ(tail_text("short", 100), std::string("short"));
  ASSERT_EQ(tail_text("line one\nline two\nline three\n", 15), std::string("line three\n"));
}

TEST(BootLogo, power_on_view) {
  auto st = [](const char *logo, bool writable, bool original) {
    return json{{"result", {{"power_on", {{"slot", "B"}, {"logo", logo}, {"writable", writable},
                                          {"original_available", original}}}}}};
  };
  auto o = power_on_view(st("original", true, true));
  ASSERT_EQ(o.text, std::string("Power-on logo (slot B): original"));
  ASSERT_TRUE(o.can_set_nebulaos);
  ASSERT_FALSE(o.can_restore);
  auto n = power_on_view(st("nebulaos", true, true));
  ASSERT_FALSE(n.can_set_nebulaos);
  ASSERT_TRUE(n.can_restore);
  auto c = power_on_view(st("custom", true, false));
  ASSERT_EQ(c.text, std::string("Power-on logo (slot B): your image"));
  ASSERT_TRUE(c.can_set_nebulaos);
  ASSERT_FALSE(c.can_restore);
  json u = st("unknown", false, true);
  u["result"]["power_on"]["reason"] = "the RTOS in this slot is not the known one; it is never changed";
  auto x = power_on_view(u);
  ASSERT_FALSE(x.can_set_nebulaos || x.can_restore);
  ASSERT_TRUE(x.text.find("Cannot be changed here: the RTOS") != std::string::npos);
  ASSERT_EQ(power_on_view(json()).text, std::string("Power-on logo: unknown"));
  for (const auto *t : {&o.text, &n.text, &c.text, &x.text}) {
    ASSERT_TRUE(t->find("Creality") == std::string::npos);
  }
}

TEST(BootLogo, power_on_result) {
  ASSERT_EQ(power_on_result(json{{"result", {{"result", "APPLIED"},
            {"power_on", {{"result", "WRITTEN"}}}}}}), std::string(""));
  ASSERT_EQ(power_on_result(json{{"result", {{"result", "UNCHANGED"}}}}), std::string(""));
  auto f = power_on_result(json{{"result", {{"result", "APPLIED"}, {"power_on",
            {{"result", "FAILED"}, {"error", "RTOS_UNKNOWN"}, {"message", "not the known one"}}}}}});
  ASSERT_TRUE(f.find("power-on logo did not: not the known one") != std::string::npos);
  ASSERT_TRUE(power_on_result(json{{"error", {{"message", "PRINTING: no"}}}}).find("PRINTING") != std::string::npos);
}

TEST(Updates, os_note) {
  ASSERT_EQ(updates_os_note("NebulaOS 701a888  (2026-10-15), slot B\nKernel 6.6"),
            std::string("This list updates Klipper, Moonraker and Mainsail.\n"
                        "NebulaOS itself (build 701a888, 2026-10-15) is updated in System image."));
  ASSERT_EQ(updates_os_note("NebulaOS v0.9.0  (93d55f9, 2026-10-18), slot B\nKernel 6.6"),
            std::string("This list updates Klipper, Moonraker and Mainsail.\n"
                        "NebulaOS itself (v0.9.0, build 93d55f9, 2026-10-18) is updated in System image."));
  ASSERT_EQ(updates_os_note("System information unavailable."),
            std::string("This list updates Klipper, Moonraker and Mainsail.\n"
                        "NebulaOS itself is updated in System image."));
}

TEST(BootLogo, jpeg_listed_and_plain_errors) {
  json files = {{"result", json::array({{{"path", "photo.JPG"}}, {{"path", "a.jpeg"}},
                                        {{"path", "x.gif"}}})}};
  auto imgs = boot_images(files);
  ASSERT_EQ(imgs.size(), (size_t)2);
  auto t = boot_logo_error_text("IMAGE_TOO_LARGE: the file is larger than 4 MB");
  ASSERT_TRUE(t.find("This image cannot be used: the file is larger than 4 MB.") == 0);
  ASSERT_TRUE(t.find("PNG, BMP or JPEG") != std::string::npos);
  ASSERT_EQ(boot_logo_error_text("PRINTING: not possible while a print is active"),
            std::string("Not possible while a print is active."));
}

TEST(BootLogo, page_list) {
  json r = json::parse(R"({"result":{"images":[{"name":"A b.png","size":1},{"name":"logo.jpg","size":2}],
    "selected":"logo.jpg","custom_active":true,
    "usb":[{"id":"usb/sda1/x.png","name":"x.png","size":3}],"usb_present":true}})");
  auto l = boot_logo_list(r);
  ASSERT_EQ(l.options, std::string("NebulaOS\nA b.png\nlogo.jpg"));
  ASSERT_EQ(l.active, 2);
  ASSERT_EQ(l.usb.size(), (size_t)1);
  ASSERT_TRUE(l.usb_present);
  ASSERT_EQ(boot_logo_index(l, "A b.png"), 1);     // auto-select after Add from USB
  ASSERT_EQ(boot_logo_index(l, "gone.png"), 0);
  auto none = boot_logo_list(json::parse(R"({"result":{"images":[],"selected":""}})"));
  ASSERT_EQ(none.options, std::string("NebulaOS"));
  ASSERT_EQ(none.active, 0);
  ASSERT_EQ(boot_logo_preview_src(json::parse(R"({"result":{"path":"/run/nebulaos/boot-preview/p.png"}})")),
            std::string("A:/run/nebulaos/boot-preview/p.png"));
  ASSERT_EQ(boot_logo_preview_src(json()), std::string(""));
}

TEST(BootLogo, shipped_logos_have_no_delete) {
  // G116: osconfig marks the logos that come with NebulaOS
  json r = json::parse(R"({"result":{"images":[{"name":"NebulaOS Deep Space.jpg","size":1,"shipped":true},
    {"name":"mine.png","size":2}],"selected":""}})");
  auto l = boot_logo_list(r);
  ASSERT_EQ(l.options, std::string("NebulaOS\nNebulaOS Deep Space.jpg\nmine.png"));
  ASSERT_EQ(l.shipped.size(), (size_t)2);
  ASSERT_TRUE(l.shipped[0]);
  ASSERT_FALSE(l.shipped[1]);
}

TEST(Logs, level_and_save) {
  ASSERT_EQ(std::string(LOG_LEVEL_OPTIONS), std::string("Info\nWarning\nDebug"));
  ASSERT_EQ(log_level_at(1), std::string("warning"));
  ASSERT_EQ(log_level_at(3), std::string(""));
  ASSERT_EQ(log_level_spdlog("warning"), std::string("warn"));
  ASSERT_EQ(log_level_spdlog("debug"), std::string("debug"));
  ASSERT_EQ(log_level_index(json::parse(R"({"result":{"level":"debug"}})")), 2);
  ASSERT_EQ(log_level_index(json()), 0);
  bool ok = false;
  auto t = logs_save_text(json::parse(R"({"result":{"result":"SAVED","file":"nebulaos-logs-x.zip","bytes":2500000}})"), &ok);
  ASSERT_TRUE(ok);
  ASSERT_TRUE(t.find("nebulaos-logs-x.zip") != std::string::npos);
  ASSERT_TRUE(logs_save_text(json::parse(R"({"error":{"message":"NO_USB: no USB stick found"}})"), &ok).find("No USB stick") == 0);
  ASSERT_FALSE(ok);
}

TEST(Logs, wifi_lines) {
  std::string log =
    "Oct 15 08:20:01 buildroot daemon.notice wpa_supplicant[300]: wlan0: CTRL-EVENT-CONNECTED - Connection to aa:bb completed\n"
    "Oct 15 08:20:02 buildroot daemon.info udhcpc[310]: lease of 192.168.0.98 obtained, lease time 86400\n"
    "Oct 15 08:20:03 buildroot cron.info crond[1273]: time disparity\n"
    "Oct 15 08:21:00 buildroot user.notice nebulaos-osconfig: wifi connect ssid=office ok result=connected\n"
    "Oct 15 08:21:01 buildroot user.notice nebulaos-osconfig: display set level=5 ok\n"
    "Oct 15 08:21:02 buildroot daemon.debug wpa_supplicant[300]: wlan0: PSK=secret\n"
    "Oct 15 08:21:03 buildroot user.notice nebulaos-osconfig: network set mode=dhcp ok\n"
    "Oct 15 08:21:04 buildroot daemon.notice wpa_supplicant[300]: wlan0: WRONG_KEY";
  std::string w = wifi_log_lines(log);
  ASSERT_TRUE(w.find("CTRL-EVENT-CONNECTED") != std::string::npos);
  ASSERT_TRUE(w.find("lease of 192.168.0.98") != std::string::npos);
  ASSERT_TRUE(w.find("wifi connect ssid=office") != std::string::npos);
  ASSERT_TRUE(w.find("network set mode=dhcp") != std::string::npos);
  ASSERT_TRUE(w.find("WRONG_KEY\n") != std::string::npos);
  ASSERT_TRUE(w.find("crond") == std::string::npos);
  ASSERT_TRUE(w.find("display set") == std::string::npos);
  ASSERT_TRUE(w.find("secret") == std::string::npos);
  ASSERT_EQ(wifi_log_lines("Oct 15 crond: nothing\n"), std::string(""));
}

TEST(Info, after_clean_reset) {
  ASSERT_FALSE(after_clean_reset(info_reply()));
  json j = info_reply();
  j["result"]["after_clean_reset"] = "2026-10-12T01:00:00Z";
  ASSERT_TRUE(after_clean_reset(j));
  j["result"]["after_clean_reset"] = nullptr;
  ASSERT_FALSE(after_clean_reset(j));
}

TEST(Logs, save_says_safe_to_remove) {
  bool ok = false;
  auto t = logs_save_text(json::parse(R"({"result":{"result":"SAVED","file":"z.zip","bytes":645917,"safe_to_remove":true}})"), &ok);
  ASSERT_TRUE(ok);
  ASSERT_TRUE(t.find("646 KB") != std::string::npos);
  ASSERT_TRUE(t.find("remove the stick") != std::string::npos);
}

TEST(Notices, from_info) {
  auto n = system_notices(json::parse(R"({"result":{"notices":[{"id":"config-edits","text":"Your changes to machine.cfg were replaced."},{"id":"","text":"x"},{"id":"y"}]}})"));
  ASSERT_EQ((int)n.size(), 1);
  ASSERT_EQ(n[0].id, std::string("config-edits"));
  ASSERT_TRUE(system_notices(info_reply()).empty());
  ASSERT_EQ(std::string(ACK_NOTICE_METHOD), std::string("machine.nebulaos.system.ack_notice"));
}

TEST(Report, debug_note) {
  ASSERT_TRUE(debug_note(json::parse(R"({"result":{"level":"debug","debug_until":1792186400}})")).find("2026-10-16") != std::string::npos);
  ASSERT_TRUE(debug_note(json::parse(R"({"result":{"level":"info"}})")).find("24 hours") != std::string::npos);
  ASSERT_EQ(std::string(REPORT_ISSUES_URL), std::string("https://github.com/coreflake1/NebulaOS/issues"));
}

TEST(Storage, text_and_cleanup) {
  auto t = storage_text(json::parse(R"({"result":{"parts":[{"label":"Print files","bytes":12000000},{"label":"Creality files","bytes":2030000000}],"total_bytes":6314295296,"free_bytes":3985342464}})"));
  ASSERT_EQ(t, std::string("Free 4.0 GB of 6.3 GB\nPrint files: 12 MB\nCreality files: 2.0 GB"));
  auto p = cleanup_plan(json::parse(R"({"result":{"bytes":1620000000,"videos_bytes":412000000,"other_slot":"creality","items":[]}})"));
  ASSERT_TRUE(p.ok && p.other_is_creality);
  ASSERT_TRUE(cleanup_text(p, false).find("Frees 1.6 GB") == 0);
  ASSERT_TRUE(cleanup_text(p, true).find("Frees 2.0 GB") == 0);
  ASSERT_TRUE(cleanup_text(p, false).find("factory reset") != std::string::npos);
  auto q = cleanup_plan(json::parse(R"({"result":{"bytes":0,"videos_bytes":0,"other_slot":"nebulaos"}})"));
  ASSERT_TRUE(cleanup_text(q, true).find("no Creality files") != std::string::npos);
  ASSERT_FALSE(cleanup_plan(json::parse(R"({"error":{"message":"x"}})")).ok);
  ASSERT_TRUE(cleanup_result_text(json::parse(R"({"result":{"result":"CLEANED","freed_bytes":1900000000,"kept":[]}})")).find("Freed 1.9 GB") != std::string::npos);
  ASSERT_TRUE(cleanup_result_text(json::parse(R"({"error":{"message":"refused while a print is active"}})")).find("print") != std::string::npos);
}

TEST(Mcu, running_version) {
  ASSERT_EQ(mcu_running_version(json::parse(R"({"result":{"eventtime":1.0,"status":{"mcu":{"mcu_version":"v0.13.0-742-g01a9c2f92"}}}})")), std::string("v0.13.0-742-g01a9c2f92"));
  ASSERT_EQ(mcu_running_version(json::parse(R"({"error":{"message":"Klippy not ready"}})")), std::string(""));
}

TEST(Database, has_namespace) {
  auto l = json::parse(R"({"result":{"namespaces":["mainsail","moonraker","fluidd"],"backups":[]}})");
  ASSERT_TRUE(database_has_namespace(l, "fluidd"));
  ASSERT_FALSE(database_has_namespace(l, "guppyscreen"));
  ASSERT_FALSE(database_has_namespace(json::parse(R"({"error":{"message":"x"}})"), "fluidd"));
}

TEST(BootLogo, import_has_no_power_on_message) {
  ASSERT_EQ(power_on_result(json::parse(R"({"result":{"result":"IMPORTED","name":"a.png"}})")), std::string(""));
  ASSERT_EQ(power_on_result(json::parse(R"({"result":{"result":"DELETED"}})")), std::string(""));
  ASSERT_EQ(power_on_result(json::parse(R"({"result":{"result":"APPLIED","power_on":{"result":"WRITTEN"}}})")), std::string(""));
  ASSERT_TRUE(power_on_result(json::parse(R"({"error":{"message":"RTOS_UNKNOWN: x; nothing was changed"}})")).find("not changed") != std::string::npos);
}

int main() {
  return minitest::run_all("nebulaos_system") > 0 ? 1 : 0;
}
