// Offline tests for Settings > Updates > System image (on-printer image
// updates, 2026-10-14) - no LVGL, no Moonraker. Build/run:
//   make test-nebulaos-image
// Replies are shaped like nebulaos_osconfig/image.py's (firmware
// tests/nebulaos-osconfig-tests.py ImageTests exercise the same fields).
//
// This file may be distributed under the terms of the GNU GPLv3 license.
#include "minitest.h"
#include "../src/nebulaos_image.h"

using json = nlohmann::json;

static bool has(const std::string &text, const char *part) {
  return text.find(part) != std::string::npos;
}

static json slots(const char *running, const char *a, const char *b) {
  auto slot = [](const char *system) {
    std::string s(system);
    if (s.empty()) return json{{"kernel", nullptr}, {"system", "unknown"}};
    return json{{"kernel", s == "creality" ? "Linux-4.4.94" : "Linux-6.6.157-rt79"},
                {"system", s}};
  };
  return {{"running", running}, {"A", slot(a)}, {"B", slot(b)}};
}

static json check_reply(const char *system, const char *running, const char *a, const char *b) {
  std::string target = std::string(running) == "A" ? "B" : "A";
  return {{"result", {{"result", "OK"}, {"file", "usb/sda1/NebulaOS.img"},
                      {"sha256", std::string(64, 'a')}, {"size", 112000000}, {"version", "1.2"},
                      {"system", system}, {"target", target},
                      {"slots", slots(running, a, b)}}}};
}

static json error(const char *message) {
  return {{"error", {{"code", 400}, {"message", message}}}};
}

TEST(Slots, lines_and_other) {
  auto s = image_slots(slots("B", "creality", "nebulaos"));
  ASSERT_TRUE(s.known);
  ASSERT_EQ(s.other, std::string("A"));
  ASSERT_EQ(image_slot_line(s, "A"), std::string("Slot A: other system"));
  ASSERT_EQ(image_slot_line(s, "B"), std::string("Slot B: NebulaOS, running"));
  auto empty = image_slots(slots("A", "nebulaos", ""));
  ASSERT_EQ(image_slot_line(empty, "B"), std::string("Slot B: empty"));
  // G46/G48: the build or version each slot holds
  json j = slots("B", "creality", "nebulaos");
  j["A"]["version"] = "1.1.0.12";
  j["B"]["build"] = "14a9280";
  j["B"]["date"] = "2026-10-15";
  auto v = image_slots(j);
  ASSERT_EQ(image_slot_line(v, "A"), std::string("Slot A: other system, version 1.1.0.12"));
  ASSERT_EQ(image_slot_line(v, "B"), std::string("Slot B: NebulaOS build 14a9280 (2026-10-15), running"));
  ASSERT_FALSE(image_slots(json{{"running", nullptr}}).known);
}

TEST(Files, list) {
  auto f = image_files(json{{"result", {
    {"files", {{{"id", "upload/a.img"}, {"source", "upload"}, {"name", "a.img"}, {"size", 98400000}},
               {{"id", "usb/sda1/KE.img"}, {"source", "usb"}, {"name", "KE.img"}, {"size", 112000000}},
               {{"id", "usb/sda1/N.img"}, {"source", "usb"}, {"name", "N.img"}, {"size", 104000000},
                {"version", "0.9"}, {"build", "14a9280"}, {"date", "2026-10-15"}},
               {{"id", "usb/sda1/O.img"}, {"source", "usb"}, {"name", "O.img"}, {"size", 104000000},
                {"version", "1.1.0.12"}},
               {{"name", "no-id.img"}}}},
    {"usb", {"sda1"}}, {"slots", slots("B", "creality", "nebulaos")}}}});
  ASSERT_TRUE(f.ok);
  ASSERT_TRUE(f.usb_present);
  ASSERT_EQ(f.files.size(), (size_t)4);
  ASSERT_EQ(f.files[2].detail, std::string("USB stick, 104 MB - NebulaOS build 14a9280 (2026-10-15)"));
  ASSERT_EQ(f.files[3].detail, std::string("USB stick, 104 MB - version 1.1.0.12"));
  ASSERT_EQ(f.files[0].detail, std::string("Uploaded, 98 MB"));
  ASSERT_EQ(f.files[1].detail, std::string("USB stick, 112 MB"));
  ASSERT_EQ(f.slots.running, std::string("B"));
  auto bad = image_files(error("INTERNAL: boom"));
  ASSERT_FALSE(bad.ok);
  ASSERT_TRUE(has(bad.error, "boom"));
}

TEST(Check, nebulaos_over_creality) {
  auto c = image_check(check_reply("nebulaos", "B", "creality", "nebulaos"));
  ASSERT_TRUE(c.ok);
  ASSERT_EQ(c.title, std::string("Install NebulaOS image"));
  ASSERT_TRUE(has(c.text, "Installs NebulaOS, version 1.2 into slot A."));
  ASSERT_TRUE(has(c.text, "replaces other system in slot A"));
  ASSERT_TRUE(has(c.text, "keeps running from slot B"));
  ASSERT_TRUE(has(c.text, "goes back to slot B once"));
  auto p = image_install_params(c);
  ASSERT_EQ(p["file"].get<std::string>(), std::string("usb/sda1/NebulaOS.img"));
  ASSERT_EQ(p["sha256"].get<std::string>().size(), (size_t)64);
}

TEST(Check, creality_has_no_fallback) {
  auto c = image_check(check_reply("creality", "A", "nebulaos", "nebulaos"));
  ASSERT_TRUE(c.ok);
  ASSERT_EQ(c.title, std::string("Install other system"));
  ASSERT_TRUE(has(c.text, "into slot B"));
  ASSERT_TRUE(has(c.text, "not NebulaOS: there is no automatic way back"));
  ASSERT_TRUE(has(c.text, "re-flash the printer board"));
  ASSERT_FALSE(has(c.text, "once, by itself"));
}

TEST(Check, unknown_system_warns) {
  auto c = image_check(check_reply("unknown", "B", "nebulaos", "nebulaos"));
  ASSERT_TRUE(c.ok);
  ASSERT_TRUE(has(c.text, "not NebulaOS, and its system is unknown"));
}

TEST(Check, names_the_builds) {
  auto r = check_reply("nebulaos", "B", "nebulaos", "nebulaos");
  r["result"]["build"] = "14a9280";
  r["result"]["date"] = "2026-10-15";
  r["result"]["slots"]["A"]["build"] = "cba8c3b";
  r["result"]["slots"]["A"]["date"] = "2026-10-14";
  auto c = image_check(r);
  ASSERT_TRUE(has(c.text, "Installs NebulaOS build 14a9280 (2026-10-15) into slot A."));
  ASSERT_TRUE(has(c.text, "replaces NebulaOS build cba8c3b (2026-10-14) in slot A"));
}

TEST(Wording, never_names_the_vendor) {
  // G57: the screen talks about NebulaOS and boot slots A/B, nothing else
  std::vector<std::string> texts = {
    image_slot_line(image_slots(slots("B", "creality", "nebulaos")), "A"),
    image_check(check_reply("creality", "A", "nebulaos", "nebulaos")).text,
    image_check(check_reply("creality", "A", "nebulaos", "nebulaos")).title,
    image_check(check_reply("nebulaos", "B", "creality", "nebulaos")).text,
    image_check(check_reply("unknown", "B", "creality", "nebulaos")).text,
    swap_confirm_text(json{{"result", {{"slots", slots("B", "creality", "nebulaos")}}}}),
    swap_confirm_text(json()),
  };
  for (const auto &t : texts) {
    ASSERT_FALSE(has(t, "Creality"));
    ASSERT_FALSE(has(t, "creality"));
    ASSERT_FALSE(has(t, "stock"));
  }
}

TEST(Check, refusals) {
  auto c = image_check(error("INVALID_IMAGE: the rootfs chunk count does not match its size"));
  ASSERT_FALSE(c.ok);
  ASSERT_EQ(c.title, std::string("Cannot install"));
  ASSERT_TRUE(has(c.text, "chunk count does not match"));
  ASSERT_TRUE(has(c.text, "Nothing was changed"));
  ASSERT_TRUE(has(image_check(error("PRINTING: not possible while a print is active")).text,
                  "print is active"));
  ASSERT_TRUE(has(image_check(error("FILE_GONE: the file is no longer there")).text, "USB stick"));
  // the running slot as target, or no running slot: refused on the screen too
  auto r = check_reply("nebulaos", "B", "nebulaos", "nebulaos");
  r["result"]["target"] = "B";
  ASSERT_FALSE(image_check(r).ok);
  r["result"]["slots"]["running"] = nullptr;
  ASSERT_FALSE(image_check(r).ok);
}

TEST(Install, errors) {
  ASSERT_EQ(image_install_error(json{{"result", {{"result", "STARTED"}}}}), std::string(""));
  ASSERT_TRUE(has(image_install_error(error("BUSY: an image install is already running")),
                  "already running"));
  ASSERT_FALSE(image_install_error(json{{"result", json::object()}}).empty());
}

TEST(Job, phases) {
  auto job = [](json j) { return image_job(json{{"result", {{"job", j}}}}); };
  ASSERT_FALSE(image_job(json{{"result", {{"job", nullptr}}}}).active);
  auto w = job({{"state", "running"}, {"phase", "writing"}, {"percent", 42}});
  ASSERT_TRUE(w.active);
  ASSERT_EQ(w.percent, 42);
  ASSERT_TRUE(has(w.text, "42%"));
  ASSERT_TRUE(has(job({{"state", "running"}, {"phase", "unpacking"}, {"percent", 5}}).text, "Unpacking"));
  auto r = job({{"state", "restarting"}, {"target", "A"}});
  ASSERT_TRUE(r.restarting);
  ASSERT_TRUE(has(r.text, "slot A"));
  auto f = job({{"state", "failed"}, {"message", "rootfs.squashfs chunk 2 is damaged"},
                {"target_cleared", true}});
  ASSERT_TRUE(f.failed);
  ASSERT_FALSE(f.active);
  ASSERT_TRUE(has(f.text, "chunk 2 is damaged"));
  ASSERT_TRUE(has(f.text, "now empty"));
  auto g = job({{"state", "failed"}, {"message", "the file changed since it was checked"},
                {"target_cleared", false}});
  ASSERT_FALSE(has(g.text, "now empty"));
  ASSERT_TRUE(has(g.text, "keeps running as before"));
  ASSERT_FALSE(g.stopped);
  // G91: stopped at a point of no return because a print started
  auto s = job({{"state", "failed"}, {"phase", "stopped_print"}, {"error", "PRINTING"},
                {"message", "Stopped: Not possible while a print is running. Nothing was changed "
                            "on the running system. Install again after that."},
                {"stopped", true}, {"target_cleared", true}});
  ASSERT_TRUE(s.failed);
  ASSERT_TRUE(s.stopped);
  ASSERT_EQ(s.text, std::string("Stopped because a print was started. Nothing changed. "
                                "Install again after the print."));
  // G91: installed, but the slot selection could not be put back - no reboot now
  auto n = job({{"state", "finished"}, {"phase", "next_power_on"}, {"percent", 100}, {"target", "A"},
                {"error", "PRINTING"}, {"message", "Install finished; ..."}});
  ASSERT_TRUE(n.next_power_on);
  ASSERT_FALSE(n.active);
  ASSERT_FALSE(n.failed);
  ASSERT_EQ(n.text, std::string("Install finished; the printer starts the new system at the "
                                "next power-on."));
}

TEST(Last, results_once) {
  auto last = [](const char *result) {
    return image_last(json{{"result", {{"last", {{"result", result}, {"version", "1.2"},
      {"system", "nebulaos"}, {"from", "B"}, {"to", "A"},
      {"note", "Klipper or the screen did not come up within 300 s"}}}}}});
  };
  ASSERT_FALSE(image_last(json{{"result", {{"last", nullptr}}}}).show);
  auto ok = last("CONFIRMED");
  ASSERT_TRUE(ok.show);
  ASSERT_TRUE(has(ok.text, "Now running NebulaOS 1.2 from slot A"));
  auto back = last("ROLLED_BACK");
  ASSERT_EQ(back.title, std::string("Went back to the previous version"));
  ASSERT_TRUE(has(back.text, "within 300 s"));
  ASSERT_TRUE(has(back.text, "back to slot B"));
  ASSERT_TRUE(has(last("NOT_STARTED").text, "started from slot B"));
  ASSERT_TRUE(has(last("ROLLBACK_FAILED").title, "Could not go back"));
  ASSERT_FALSE(last("SOMETHING_ELSE").show);
  ASSERT_TRUE(image_trial_pending(json{{"result", {{"trial", {{"from", "B"}, {"to", "A"}}}}}}));
  ASSERT_FALSE(image_trial_pending(json{{"result", {{"trial", nullptr}}}}));
}

TEST(Swap, text_from_the_other_slot) {
  auto st = [](const char *running, const char *a, const char *b) {
    return json{{"result", {{"slots", slots(running, a, b)}}}};
  };
  ASSERT_TRUE(has(swap_confirm_text(st("B", "nebulaos", "nebulaos")), "Come back the same way"));
  ASSERT_TRUE(has(swap_confirm_text(st("B", "creality", "nebulaos")), "It is not NebulaOS"));
  ASSERT_TRUE(has(swap_confirm_text(st("A", "nebulaos", "creality")), "slot B (other system)"));
  ASSERT_TRUE(has(swap_confirm_text(json()), "other boot slot (A/B)"));
}

TEST(Ota, downloaded_file_in_the_list) {
  auto f = image_files(json{{"result", {{"usb", json::array()}, {"slots", slots("B", "", "nebulaos")},
    {"files", {{{"id", "ota/NebulaOS-Ender3V3KE-0.9.2.img"}, {"source", "ota"},
                {"name", "NebulaOS-Ender3V3KE-0.9.2.img"}, {"size", 130000000}}}}}}});
  ASSERT_TRUE(f.ok);
  ASSERT_EQ(f.files[0].detail, std::string("Downloaded, 130 MB"));
}

TEST(Ota, check_offers_a_download) {
  auto c = ota_check(json{{"result", {{"result", "AVAILABLE"}, {"available", true},
    {"version", "0.9.2"}, {"running_version", "0.9.1"},
    {"img", {{"name", "NebulaOS-Ender3V3KE-0.9.2.img"}, {"size", 130000000}}}}}});
  ASSERT_TRUE(c.available);
  ASSERT_EQ(c.title, std::string("NebulaOS 0.9.2"));
  ASSERT_TRUE(has(c.text, "this printer runs 0.9.1"));
  ASSERT_TRUE(has(c.text, "GitHub (130 MB)"));
}

TEST(Ota, check_without_an_offer) {
  auto up = ota_check(json{{"result", {{"result", "UP_TO_DATE"}, {"available", false},
                                       {"version", "0.9.1"}, {"running_version", "0.9.1"}}}});
  ASSERT_FALSE(up.available);
  ASSERT_TRUE(has(up.text, "up to date (0.9.1)"));
  auto off = ota_check(json{{"result", {{"result", "OFFLINE"}, {"available", false},
    {"message", "Could not reach the release channel (URLError)."}}}});
  ASSERT_TRUE(has(off.text, "Could not reach"));
  ASSERT_TRUE(has(ota_check(error("PRINTING: a print is active")).text, "print is active"));
}

TEST(Ota, download_reply_and_job) {
  ASSERT_EQ(ota_download_error(json{{"result", {{"result", "STARTED"}}}}), std::string(""));
  auto again = json{{"result", {{"result", "ALREADY_DOWNLOADED"}}}};
  ASSERT_TRUE(ota_already_downloaded(again));
  ASSERT_TRUE(has(ota_download_error(again), "already downloaded"));
  ASSERT_TRUE(has(ota_download_error(error("NO_SPACE: not enough free space")), "free space"));
  auto run = ota_download(json{{"result", {{"download", {{"state", "running"}, {"percent", 42},
                                                          {"version", "0.9.2"}}}}}});
  ASSERT_TRUE(run.active);
  ASSERT_TRUE(has(run.text, "0.9.2: 42%"));
  auto bad = ota_download(json{{"result", {{"download", {{"state", "failed"},
    {"message", "the download does not match the release record (sha256)"}}}}}});
  ASSERT_TRUE(bad.failed);
  ASSERT_TRUE(has(bad.text, "Nothing was installed"));
  ASSERT_TRUE(ota_download(json{{"result", {{"download", {{"state", "finished"}}}}}}).finished);
  ASSERT_FALSE(ota_download(json{{"result", {{"download", nullptr}}}}).active);
}

int main() {
  return minitest::run_all("nebulaos_image") > 0 ? 1 : 0;
}
