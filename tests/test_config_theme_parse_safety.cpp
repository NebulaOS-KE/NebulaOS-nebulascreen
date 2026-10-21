// Regression test for the 2026-08-06 GuppyScreen startup crash: a real
// deployment found Config::init()/ThemeConfig::init() would call
// json::parse(std::fstream(path)) unguarded whenever stat() found the file
// present, with no fallback if the actual read/parse then failed (observed
// live: an uncaught nlohmann::detail::parse_error - "unexpected end of
// input" - terminated the whole process before a single log line was
// written, leaving the display stuck on the kernel's boot splash). The
// leading theory is a read-only-mounted file rejecting std::fstream's
// default read+write open mode, but this test doesn't depend on that theory
// being exactly right - it proves the general contract that now holds
// regardless of *why* a stat()-present file fails to parse: fall back to
// the same in-memory defaults already used for "file doesn't exist", never
// crash.
//
// Build/run: make test-config-theme-parse-safety
//
// This file may be distributed under the terms of the GNU GPLv3 license.
#include "minitest.h"
#include "../src/config.h"
#include "../src/theme.h"
#include "../src/nebulascreen_paths.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <sys/stat.h>

namespace {

std::string make_tmpdir() {
  const char *base = getenv("TMPDIR");
  std::string tmpl = std::string(base && *base ? base : "/tmp") + "/nebulascreen_config_test_XXXXXX";
  char *dir = mkdtemp(&tmpl[0]);
  ASSERT_TRUE(dir != nullptr);
  return std::string(dir);
}

void write_file(const std::string &path, const std::string &content) {
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  f << content;
}

}  // namespace

TEST(ConfigParseSafety, malformed_config_falls_back_to_defaults_not_crash) {
  auto dir = make_tmpdir();
  std::string cfg = dir + "/nebulascreen.json";
  // Present (stat() succeeds) but not valid JSON - exactly what an
  // open-mode-rejected fstream read (or any other partial/failed read)
  // looks like to json::parse: non-empty stat, empty-or-garbage content.
  write_file(cfg, "");

  Config c;
  // Must not throw - this is the actual regression: it used to.
  c.init(cfg, dir + "/thumbnails");

  // Falls back to the exact same defaults used for "file doesn't exist"
  // (wpa_supplicant was dropped from them with the Wi-Fi code, Q4).
  ASSERT_TRUE(c.get_json("/display_sleep_sec").is_number());
  ASSERT_EQ(c.get_json("/display_sleep_sec").get<int>(), 600);
  ASSERT_EQ(c.get_json("/log_path").get<std::string>(), std::string(nebulascreen::kLogPath));
}

TEST(ConfigParseSafety, well_formed_config_still_parses_normally) {
  // Confirms the fix didn't disable the success path - only the failure
  // path changed.
  auto dir = make_tmpdir();
  std::string cfg = dir + "/nebulascreen.json";
  write_file(cfg, "{\"display_sleep_sec\": 42}");

  Config c;
  c.init(cfg, dir + "/thumbnails");

  ASSERT_EQ(c.get_json("/display_sleep_sec").get<int>(), 42);
}

TEST(ConfigParseSafety, missing_config_still_uses_defaults_unaffected_by_fix) {
  auto dir = make_tmpdir();
  std::string cfg = dir + "/does_not_exist.json";

  Config c;
  c.init(cfg, dir + "/thumbnails");

  ASSERT_EQ(c.get_json("/display_sleep_sec").get<int>(), 600);
}

TEST(ThemeParseSafety, malformed_theme_falls_back_to_default_blue_not_crash) {
  auto dir = make_tmpdir();
  std::string theme_path = dir + "/blue.json";
  write_file(theme_path, "");

  ThemeConfig t;
  // Must not throw.
  t.init(theme_path);

  ASSERT_EQ(t.get_json("/primary_color").get<std::string>(), std::string("0x2196F3"));
  ASSERT_EQ(t.get_json("/secondary_color").get<std::string>(), std::string("0xF44336"));
}

TEST(ThemeParseSafety, well_formed_theme_still_parses_normally) {
  auto dir = make_tmpdir();
  std::string theme_path = dir + "/green.json";
  write_file(theme_path, "{\"primary_color\": \"0x00FF00\", \"secondary_color\": \"0xFF0000\"}");

  ThemeConfig t;
  t.init(theme_path);

  ASSERT_EQ(t.get_json("/primary_color").get<std::string>(), std::string("0x00FF00"));
}

TEST(ThemeParseSafety, malformed_theme_write_back_is_best_effort_and_does_not_throw) {
  // The write-back on line 36-37 of theme.cpp (std::ofstream, no exception
  // mask set) must never crash even if the read side just fell back to
  // defaults - this proves the whole init() call completes cleanly
  // end-to-end for the failure case, not just the parse step in isolation.
  auto dir = make_tmpdir();
  std::string theme_path = dir + "/pink.json";
  write_file(theme_path, "not json");

  ThemeConfig t;
  t.init(theme_path);  // must return normally, no exception

  // The defaults were written back (this file IS writable in the test -
  // proves the normal writable-path behavior still works, distinct from
  // the unwritable/read-only-mount scenario this whole fix targets).
  std::ifstream check(theme_path);
  std::string content((std::istreambuf_iterator<char>(check)), std::istreambuf_iterator<char>());
  ASSERT_TRUE(content.find("0x2196F3") != std::string::npos);
}

// --- power-cut safe save (journal, then in place) -----------------------

std::string read_file(const std::string &path) {
  std::ifstream f(path);
  return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

bool exists(const std::string &path) {
  struct stat st;
  return stat(path.c_str(), &st) == 0;
}

TEST(ConfigSave, save_round_trips_and_leaves_no_journal) {
  auto dir = make_tmpdir();
  std::string cfg = dir + "/nebulascreen.json";
  Config c;
  c.init(cfg, dir + "/thumbnails");
  c.set<int>("/display_sleep_sec", 123);
  c.save();
  ASSERT_TRUE(!exists(Config::journal_for(cfg)));
  Config d;
  d.init(cfg, dir + "/thumbnails");
  ASSERT_EQ(d.get_json("/display_sleep_sec").get<int>(), 123);
}

TEST(ConfigSave, save_keeps_the_inode_a_bind_mount_points_at) {
  auto dir = make_tmpdir();
  std::string cfg = dir + "/nebulascreen.json";
  write_file(cfg, "{\"display_sleep_sec\": 1, \"padding\": \"a long value to be shortened\"}");
  struct stat before, after;
  ASSERT_EQ(stat(cfg.c_str(), &before), 0);
  Config c;
  c.init(cfg, dir + "/thumbnails");
  c.get_json("").erase("padding");
  c.save();
  ASSERT_EQ(stat(cfg.c_str(), &after), 0);
  ASSERT_TRUE(before.st_ino == after.st_ino);
  ASSERT_TRUE(json::parse(read_file(cfg)).is_object());  // shorter content, cut cleanly
}

TEST(ConfigSave, interrupted_in_place_write_is_restored_from_the_journal) {
  auto dir = make_tmpdir();
  std::string cfg = dir + "/nebulascreen.json";
  write_file(Config::journal_for(cfg), "{\"display_sleep_sec\": 77}");
  write_file(cfg, "{\"display_sleep_sec\": 7");  // cut off mid-write
  Config c;
  c.init(cfg, dir + "/thumbnails");
  ASSERT_EQ(c.get_json("/display_sleep_sec").get<int>(), 77);
  ASSERT_TRUE(!exists(Config::journal_for(cfg)));
}

TEST(ConfigSave, interrupted_journal_write_leaves_the_config_alone) {
  auto dir = make_tmpdir();
  std::string cfg = dir + "/nebulascreen.json";
  write_file(cfg, "{\"display_sleep_sec\": 55}");
  write_file(Config::journal_for(cfg), "{\"display_sl");  // cut off
  Config c;
  c.init(cfg, dir + "/thumbnails");
  ASSERT_EQ(c.get_json("/display_sleep_sec").get<int>(), 55);
  ASSERT_TRUE(!exists(Config::journal_for(cfg)));
}

TEST(ConfigSave, the_device_journal_lives_beside_the_real_file) {
  ASSERT_EQ(Config::journal_for(nebulascreen::kConfigPath),
            std::string("/usr/data/nebulaos/nebulascreen/nebulascreen.json.saving"));
}

int main() {
  return minitest::run_all("config_theme_parse_safety") > 0 ? 1 : 0;
}
