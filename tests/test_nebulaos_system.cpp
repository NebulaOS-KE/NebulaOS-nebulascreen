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
  ASSERT_EQ(boot_mode_index(boot_status("blank", "builtin")), 1);
  ASSERT_EQ(boot_mode_index(boot_status("console", "builtin")), 0);  // retired (D10): logo
  ASSERT_EQ(boot_mode_index(json()), 0);
  ASSERT_EQ(boot_mode_at(0), std::string("logo"));
  ASSERT_EQ(boot_mode_at(1), std::string("blank"));
  ASSERT_EQ(boot_mode_at(2), std::string(""));
  ASSERT_EQ(std::string(BOOT_MODE_OPTIONS), std::string("Logo\nBlank"));
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
  ASSERT_EQ(boot_image_options(imgs), std::string("Built-in\nUP.PNG\nlogo.png\nzz.bmp"));
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

int main() {
  return minitest::run_all("nebulaos_system") > 0 ? 1 : 0;
}
