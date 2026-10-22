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

int main() {
  return minitest::run_all("nebulaos_refusal") > 0 ? 1 : 0;
}
