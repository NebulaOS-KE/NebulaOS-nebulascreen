// Offline tests for NebulaOS refusals (G91 v3.4, src/nebulaos_refusal.cpp): the reason
// mapping, the JSON-RPC error parsing and the console-line filter.
// Build/run:
//   make test-nebulaos-refusal
//
// This file may be distributed under the terms of the GNU GPLv3 license.
#include "minitest.h"
#include "../src/nebulaos_refusal.h"

using json = nlohmann::json;

static json error_reply(const std::string &message, int code = 409) {
  return json{{"jsonrpc", "2.0"}, {"id", 7}, {"error", {{"code", code}, {"message", message}}}};
}

TEST(Mapping, every_reason_has_its_text) {
  ASSERT_EQ(refusal_reason_text("PRINTING"), std::string("Not possible while a print is running."));
  ASSERT_EQ(refusal_reason_text("PAUSED"), std::string("Not possible while a print is paused."));
  ASSERT_EQ(refusal_reason_text("ACTIVE"),
            std::string("Not while the printer is heating or moving."));
  ASSERT_EQ(refusal_reason_text("CALIBRATING"),
            std::string("Not possible while a calibration is running."));
  ASSERT_EQ(refusal_reason_text("UPDATING"), std::string("Another update is running."));
  ASSERT_EQ(refusal_reason_text("LOW_MEMORY"),
            std::string("Not possible right now: the printer is low on memory."));
  ASSERT_EQ(refusal_reason_text("UNKNOWN"),
            std::string("Printer state unknown - not possible right now."));
  ASSERT_EQ(refusal_reason_text("SOMETHING_NEW"), refusal_reason_text("UNKNOWN"));
  ASSERT_EQ(std::string(REFUSAL_TITLE), std::string("Not now"));
}

TEST(Parse, jsonrpc_refusal) {
  auto r = refusal_of(error_reply("PRINTING: Not possible while a print is running."));
  ASSERT_TRUE(r.refused);
  ASSERT_EQ(r.reason, std::string("PRINTING"));
  ASSERT_EQ(r.text, std::string("Not possible while a print is running."));
  r = refusal_of(error_reply(
    "UPDATING: The new system is still being checked - try again after the check."));
  ASSERT_EQ(r.reason, std::string("UPDATING"));
  ASSERT_EQ(r.text, std::string("The new system is still being checked - try again after the check."));
  // the screen's words for the other reasons (a preheated printer is ACTIVE)
  r = refusal_of(error_reply("ACTIVE: Not possible while the printer is moving or heating."));
  ASSERT_EQ(r.reason, std::string("ACTIVE"));
  ASSERT_EQ(r.text, std::string("Not while the printer is heating or moving."));
}

TEST(Parse, code_only_uses_the_table) {
  for (const char *code : {"PRINTING", "PAUSED", "ACTIVE", "CALIBRATING", "UPDATING",
                           "LOW_MEMORY", "UNKNOWN"}) {
    auto r = refusal_of(error_reply(std::string(code) + ": "));
    ASSERT_TRUE(r.refused);
    ASSERT_EQ(r.reason, std::string(code));
    ASSERT_EQ(r.text, refusal_reason_text(code));
    ASSERT_TRUE(refusal_from_message(code).refused);
  }
}

TEST(Parse, older_firmware_codes) {
  auto r = refusal_of(error_reply("BUSY: an MCU flash is already running"));
  ASSERT_TRUE(r.refused);
  ASSERT_EQ(r.reason, std::string("UPDATING"));
  ASSERT_EQ(r.text, std::string("An MCU flash is already running"));
  r = refusal_of(error_reply("REFUSED: printing", 400));
  ASSERT_EQ(r.reason, std::string("PRINTING"));
  ASSERT_EQ(r.text, refusal_reason_text("PRINTING"));
}

TEST(Parse, other_errors_are_not_refusals) {
  ASSERT_FALSE(refusal_of(error_reply("INVALID_IMAGE: not a KE update package", 400)).refused);
  ASSERT_FALSE(refusal_of(error_reply("FLASH_FAILED: no handshake", 500)).refused);
  ASSERT_FALSE(refusal_of(error_reply("Printer is printing")).refused);
  ASSERT_FALSE(refusal_of(error_reply("printing: lower case is not a code")).refused);
  ASSERT_FALSE(refusal_of(json{{"result", {{"result", "OK"}}}}).refused);
  ASSERT_FALSE(refusal_of(json()).refused);
  ASSERT_FALSE(refusal_of(json{{"error", 5}}).refused);
  ASSERT_TRUE(refusal_of(json{{"error", "PAUSED: Not possible while a print is paused."}}).refused);
}

TEST(Hint, client_side_check) {
  ASSERT_FALSE(refusal_hint(false, false).refused);
  auto r = refusal_hint(true, false);
  ASSERT_TRUE(r.refused);
  ASSERT_EQ(r.text, std::string("Not possible while a print is running."));
  r = refusal_hint(true, true);
  ASSERT_EQ(r.reason, std::string("PAUSED"));
  ASSERT_EQ(r.text, std::string("Not possible while a print is paused."));
}

TEST(Filter, refusal_lines) {
  ASSERT_TRUE(is_refusal_line("!! NebulaOS refused: Not possible while a print is running."));
  ASSERT_TRUE(is_refusal_line("!! NebulaOS refused:"));
  ASSERT_FALSE(is_refusal_line("!! Must home axis first: 110.000 138.000 0.000"));
  ASSERT_FALSE(is_refusal_line("!! MCU 'mcu' shutdown: Timer too close"));
  ASSERT_FALSE(is_refusal_line("// NebulaOS refused: echo"));
  ASSERT_FALSE(is_refusal_line("!! NebulaOS: something else"));
  ASSERT_FALSE(is_refusal_line(""));
}

// G98 (Run 18): the screen's Restart Klipper / Restart MCU.
static json busy_reply(bool busy, const char *reason, const char *klippy, const char *msg = "") {
  json r = {{"busy", busy}, {"message", msg}, {"profile", "restart"}};
  r["reason"] = reason ? json(reason) : json(nullptr);
  r["klippy"] = klippy ? json(klippy) : json(nullptr);
  return {{"result", r}};
}

TEST(Restart, asks_the_restart_profile) {
  ASSERT_EQ(std::string(BUSY_METHOD), std::string("machine.nebulaos.system.busy"));
  ASSERT_EQ(busy_params_restart().dump(), std::string("{\"profile\":\"restart\"}"));
}

TEST(Restart, busy_refuses_with_the_popup_text) {
  for (auto kind : {RestartKind::KLIPPER, RestartKind::MCU}) {
    auto d = restart_decision(kind, busy_reply(true, "PRINTING", "ready",
                                               "Not possible while a print is running."));
    ASSERT_TRUE(d.refused);
    ASSERT_TRUE(d.refusal.refused);
    ASSERT_EQ(d.refusal.reason, std::string("PRINTING"));
    ASSERT_EQ(d.refusal.text, std::string("Not possible while a print is running."));
    ASSERT_TRUE(d.method.empty());
  }
  auto d = restart_decision(RestartKind::KLIPPER, busy_reply(true, "ACTIVE", "ready", "x"));
  ASSERT_EQ(d.refusal.text, refusal_reason_text("ACTIVE"));          // the table's words
  d = restart_decision(RestartKind::KLIPPER,
                       busy_reply(true, "UPDATING", "ready", "An MCU flash is running."));
  ASSERT_EQ(d.refusal.text, std::string("An MCU flash is running."));  // what runs
  d = restart_decision(RestartKind::KLIPPER, busy_reply(true, "NEWCODE", "ready", "Something new."));
  ASSERT_EQ(d.refusal.reason, std::string("UNKNOWN"));
  ASSERT_EQ(d.refusal.text, std::string("Something new."));
  d = restart_decision(RestartKind::KLIPPER, busy_reply(true, nullptr, "unknown"));
  ASSERT_TRUE(d.refused);
  ASSERT_EQ(d.refusal.text, refusal_reason_text("UNKNOWN"));
}

TEST(Restart, not_busy_and_ready_uses_klipper) {
  auto d = restart_decision(RestartKind::KLIPPER, busy_reply(false, nullptr, "ready"));
  ASSERT_FALSE(d.refused);
  ASSERT_FALSE(d.check_unavailable);
  ASSERT_EQ(d.method, std::string("printer.restart"));
  ASSERT_EQ(d.fallback_method, std::string("machine.services.restart"));
  ASSERT_EQ(d.fallback_params["service"].get<std::string>(), std::string("klipper"));
  d = restart_decision(RestartKind::MCU, busy_reply(false, nullptr, "ready"));
  ASSERT_EQ(d.method, std::string("printer.firmware_restart"));
}

TEST(Restart, hung_klippy_is_restarted_as_a_service) {
  for (const char *k : {"not_ready", "unknown", "something_new"}) {
    auto d = restart_decision(RestartKind::KLIPPER, busy_reply(false, nullptr, k));
    ASSERT_FALSE(d.refused);
    ASSERT_EQ(d.method, std::string("machine.services.restart"));
    ASSERT_EQ(d.params["service"].get<std::string>(), std::string("klipper"));
    ASSERT_TRUE(d.fallback_method.empty());
  }
  auto d = restart_decision(RestartKind::KLIPPER, busy_reply(false, nullptr, nullptr));
  ASSERT_EQ(d.method, std::string("machine.services.restart"));
  // a shut-down MCU needs FIRMWARE_RESTART; the service restart if Klippy does not answer
  d = restart_decision(RestartKind::MCU, busy_reply(false, nullptr, "not_ready"));
  ASSERT_EQ(d.method, std::string("printer.firmware_restart"));
  ASSERT_EQ(d.fallback_method, std::string("machine.services.restart"));
  // ...but a Klippy that does not answer cannot run FIRMWARE_RESTART (the call
  // could hang): the service restart, also for Restart MCU (verifier finding 2)
  for (const char *k : {"unknown", "something_new"}) {
    d = restart_decision(RestartKind::MCU, busy_reply(false, nullptr, k));
    ASSERT_EQ(d.method, std::string("machine.services.restart"));
    ASSERT_EQ(d.params["service"].get<std::string>(), std::string("klipper"));
  }
}

TEST(Restart, missing_or_failing_check_never_blocks) {
  json missing = {{"error", {{"code", 404}, {"message", "Method not found"}}}};
  json timeout = {{"error", {{"code", 504}, {"message", "timed out"}}}};
  json refusal_shaped = {{"error", {{"code", 409}, {"message", "PRINTING: printing"}}}};
  for (const json &r : {missing, timeout, refusal_shaped, json(), json::object(),
                        json({{"result", "odd"}})}) {
    for (auto kind : {RestartKind::KLIPPER, RestartKind::MCU}) {
      auto d = restart_decision(kind, r);
      ASSERT_FALSE(d.refused);
      ASSERT_TRUE(d.check_unavailable);
      ASSERT_EQ(d.method, std::string(kind == RestartKind::KLIPPER ? "printer.restart"
                                                                   : "printer.firmware_restart"));
    }
  }
  // busy must be a real true
  json odd = {{"result", {{"busy", "yes"}, {"klippy", "ready"}}}};
  ASSERT_FALSE(restart_decision(RestartKind::KLIPPER, odd).refused);
}

// ---- G107: one action, one popup ----

TEST(Line, git_wrapper_line_drops_action_and_reason) {
  auto r = refusal_from_line(
    "!! NebulaOS refused: update (git reset) - Not possible while a print is paused. (PAUSED)");
  ASSERT_TRUE(r.refused);
  ASSERT_EQ(r.reason, std::string("PAUSED"));
  ASSERT_EQ(r.text, std::string("Not possible while a print is paused."));
  // the firmware's ACTIVE words -> the screen's ACTIVE words
  r = refusal_from_line("!! NebulaOS refused: update (git checkout) - "
                        "Not possible while the printer is moving or heating. (ACTIVE)");
  ASSERT_EQ(r.reason, std::string("ACTIVE"));
  ASSERT_EQ(r.text, refusal_reason_text("ACTIVE"));
}

TEST(Line, reason_from_the_firmware_text_without_suffix) {
  // supervisorctl shim (no "(REASON)") and the [nebulaos] component (no action)
  auto r = refusal_from_line("!! NebulaOS refused: restart klipper - "
                             "Not possible while the printer is moving or heating.");
  ASSERT_EQ(r.reason, std::string("ACTIVE"));
  ASSERT_EQ(r.text, refusal_reason_text("ACTIVE"));
  r = refusal_from_line("!! NebulaOS refused: Not possible while a print is running.");
  ASSERT_EQ(r.reason, std::string("PRINTING"));
  // a text with " - " in it and no action prefix stays whole
  r = refusal_from_line("!! NebulaOS refused: Printer state unknown - not possible right now.");
  ASSERT_EQ(r.reason, std::string("UNKNOWN"));
  ASSERT_EQ(r.text, refusal_reason_text("UNKNOWN"));
  r = refusal_from_line("!! NebulaOS refused: "
                        "The new system is still being checked - try again after the check.");
  ASSERT_EQ(r.reason, std::string("UPDATING"));
  ASSERT_EQ(r.text,
            std::string("The new system is still being checked - try again after the check."));
  // a text the screen does not know: shown as the printer says it
  r = refusal_from_line("!! NebulaOS refused: update (git pull) - something new happened");
  ASSERT_EQ(r.reason, std::string("UNKNOWN"));
  ASSERT_EQ(r.text, std::string("Something new happened"));
  ASSERT_FALSE(refusal_from_line("!! Must home axis first").refused);
  ASSERT_FALSE(refusal_from_line("// NebulaOS refused: x").refused);
}

TEST(Line, git_commands_of_one_update_are_identical) {
  const char *lines[] = {
    "!! NebulaOS refused: update (git reset) - Not possible while the printer is moving or heating. (ACTIVE)",
    "!! NebulaOS refused: update (git checkout) - Not possible while the printer is moving or heating. (ACTIVE)",
    "!! NebulaOS refused: update (git pull) - Not possible while the printer is moving or heating. (ACTIVE)",
  };
  std::string key = refusal_key(refusal_from_line(lines[0]));
  for (const char *l : lines) {
    ASSERT_EQ(refusal_key(refusal_from_line(l)), key);
  }
}

using A = RefusalGate::Action;

static Refusal line_of(const std::string &tail) {
  return refusal_from_line(std::string(REFUSAL_LINE_PREFIX) + tail);
}

TEST(Gate, moonraker_retries_give_one_popup) {
  RefusalGate g;
  Refusal r = line_of("update (git reset) - Not possible while a print is paused. (PAUSED)");
  ASSERT_TRUE(g.line(r, 1000) == A::SHOW);
  ASSERT_TRUE(g.line(r, 1200) == A::DROP);
  ASSERT_TRUE(g.line(r, 6000) == A::DROP);
  // the window slides: each retry within 10 s of the previous one
  ASSERT_TRUE(g.line(r, 15000) == A::DROP);
  ASSERT_TRUE(g.line(r, 24999) == A::DROP);
  // closed by the user: the same retry chain still adds nothing
  g.closed();
  ASSERT_TRUE(g.line(r, 30000) == A::DROP);
  // quiet for 10 s: a new attempt is a new popup
  ASSERT_TRUE(g.line(r, 40000) == A::SHOW);
}

TEST(Gate, a_new_reason_replaces_the_open_popup) {
  RefusalGate g;
  Refusal paused = line_of("Not possible while a print is paused.");
  Refusal low = line_of("Not possible right now: the printer is low on memory.");
  ASSERT_TRUE(g.line(paused, 0) == A::SHOW);
  ASSERT_TRUE(g.popup_open());
  ASSERT_TRUE(g.line(low, 500) == A::REPLACE);
  ASSERT_TRUE(g.line(low, 900) == A::DROP);
  ASSERT_TRUE(g.line(paused, 1000) == A::REPLACE);
  g.closed();
  ASSERT_FALSE(g.popup_open());
  ASSERT_TRUE(g.line(low, 2000) == A::SHOW);
}

TEST(Gate, own_refusal_swallows_its_console_line) {
  RefusalGate g;
  // the screen's own call: its popup comes from the JSON-RPC error ...
  Refusal own = refusal_of(error_reply("ACTIVE: Not possible while the printer is moving or heating."));
  g.own(own, 5000);
  // ... and the same refusal's console line adds nothing
  Refusal line = line_of("Not possible while the printer is moving or heating.");
  ASSERT_TRUE(g.line(line, 5100) == A::DROP);
  ASSERT_FALSE(g.popup_open());
  // a later Mainsail action with another reason still shows
  ASSERT_TRUE(g.line(line_of("update (git pull) - Not possible while a print is running. (PRINTING)"),
                     6000) == A::SHOW);
  // an own refusal is only remembered, never suppressed (the caller shows it)
  g.own(own, 6100);
  ASSERT_TRUE(g.line(line, 6200) == A::DROP);
  g.own(Refusal(), 7000);  // not a refusal: ignored
  ASSERT_TRUE(g.line(line, 7100) == A::DROP);
}

TEST(Gate, clock_wrap_is_safe) {
  RefusalGate g;
  Refusal r = line_of("Not possible while a print is running.");
  ASSERT_TRUE(g.line(r, 0xFFFFF000u) == A::SHOW);
  ASSERT_TRUE(g.line(r, 0x00000800u) == A::DROP);   // 6 s later, after the wrap
  // still open after the window: the popup is refreshed in place, never a second one
  ASSERT_TRUE(g.line(r, 0x00003000u + REFUSAL_DEDUPE_MS) == A::REPLACE);
  g.closed();
  ASSERT_TRUE(g.line(r, 0x00006000u + 2 * REFUSAL_DEDUPE_MS) == A::SHOW);
  ASSERT_TRUE(g.line(Refusal(), 1) == A::DROP);     // not a refusal
  ASSERT_TRUE(&refusal_gate() == &refusal_gate());
}

int main() {
  return minitest::run_all("nebulaos_refusal") > 0 ? 1 : 0;
}
