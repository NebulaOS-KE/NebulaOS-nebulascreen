// Offline tests for the pure power-loss recovery panel logic - no LVGL, no WebSocket, no
// Moonraker, no printer. Build/run:
//   make test-plr-status
//
// This file may be distributed under the terms of the GNU GPLv3 license.
#include "minitest.h"
#include "../src/plr_status.h"

#include <limits>

using json = nlohmann::json;

static json available(const char *mode) {
  return {{"state", "RECOVERY_AVAILABLE"}, {"position_safe", false}, {"resume_mode", mode},
          {"recovery", {{"generation", 7}, {"file", "parts/bracket.gcode"},
                        {"file_size", 1000}, {"file_position", 420}, {"progress", 0.42},
                        {"checked", "light"}, {"reason", ""}}},
          {"last_action", ""}, {"last_error", ""}, {"active_session", false},
          {"resume_in_progress", false}, {"candidate_pending", false}};
}

// ---------------------------------------------------------------------------------------
// parse_plr_status
// ---------------------------------------------------------------------------------------

TEST(Parse, live_snapshot_from_ke_dev_is_none) {
  // Verbatim from the gs-services capture on ke-dev, 2026-10-09.
  json j = json::parse(R"({"state":"NONE","position_safe":false,"resume_mode":"state_restore_only","recovery":{},"last_action":"","last_error":"","active_session":false,"resume_in_progress":false,"candidate_pending":false})");
  auto s = parse_plr_status(j);
  ASSERT_TRUE(s.present);
  ASSERT_EQ(s.state, std::string("NONE"));
  ASSERT_EQ(s.resume_mode, std::string("state_restore_only"));
  ASSERT_TRUE(s.file.empty());
}

TEST(Parse, null_means_not_present) {
  ASSERT_FALSE(parse_plr_status(json()).present);
  ASSERT_FALSE(parse_plr_status(json::array()).present);
}

TEST(Parse, recovery_fields) {
  auto s = parse_plr_status(available("physical"));
  ASSERT_EQ(s.file, std::string("parts/bracket.gcode"));
  ASSERT_TRUE(s.progress > 0.419 && s.progress < 0.421);
  ASSERT_EQ(s.resume_mode, std::string("physical"));
}

TEST(Parse, wrong_types_are_ignored_not_thrown) {
  json j = {{"state", 3}, {"resume_mode", nullptr}, {"recovery", "x"},
            {"resume_in_progress", "yes"}, {"last_error", false}};
  auto s = parse_plr_status(j);
  ASSERT_TRUE(s.present);
  ASSERT_TRUE(s.state.empty());
  ASSERT_FALSE(s.resume_in_progress);
}

TEST(Parse, progress_is_clamped_and_nan_safe) {
  json j = available("physical");
  j["recovery"]["progress"] = 3.5;
  ASSERT_TRUE(parse_plr_status(j).progress == 1.0);
  j["recovery"]["progress"] = -1;
  ASSERT_TRUE(parse_plr_status(j).progress == 0.0);
}

// ---------------------------------------------------------------------------------------
// plr_view: what the panel offers
// ---------------------------------------------------------------------------------------

TEST(View, absent_object_offers_nothing) {
  auto v = plr_view(parse_plr_status(json()), false);
  ASSERT_FALSE(v.show_resume);
  ASSERT_FALSE(v.show_discard);
}

TEST(View, none_offers_nothing) {
  auto v = plr_view(parse_plr_status({{"state", "NONE"}}), false);
  ASSERT_FALSE(v.show_resume);
  ASSERT_FALSE(v.show_discard);
}

TEST(View, physical_available_offers_resume_and_discard) {
  auto v = plr_view(parse_plr_status(available("physical")), false);
  ASSERT_TRUE(v.show_resume);
  ASSERT_TRUE(v.show_discard);
  ASSERT_TRUE(v.text.find("bracket.gcode at 42%") != std::string::npos);
}

TEST(View, state_restore_only_never_offers_resume) {
  auto v = plr_view(parse_plr_status(available("state_restore_only")), false);
  ASSERT_FALSE(v.show_resume);
  ASSERT_TRUE(v.show_discard);
}

TEST(View, unknown_resume_mode_never_offers_resume) {
  auto v = plr_view(parse_plr_status(available("something_new")), false);
  ASSERT_FALSE(v.show_resume);
}

TEST(View, printing_hides_actions_even_with_a_record) {
  auto v = plr_view(parse_plr_status(available("physical")), true);
  ASSERT_FALSE(v.show_resume);
  ASSERT_FALSE(v.show_discard);
}

TEST(View, recording_hides_actions) {
  auto v = plr_view(parse_plr_status({{"state", "RECORDING"}, {"resume_mode", "physical"}}), false);
  ASSERT_FALSE(v.show_resume);
  ASSERT_FALSE(v.show_discard);
}

TEST(View, resume_in_progress_hides_actions) {
  json j = available("physical");
  j["resume_in_progress"] = true;
  auto v = plr_view(parse_plr_status(j), false);
  ASSERT_FALSE(v.show_resume);
  ASSERT_FALSE(v.show_discard);
}

TEST(View, incompatible_offers_discard_only_with_reason) {
  json j = available("physical");
  j["state"] = "RECOVERY_INCOMPATIBLE";
  j["recovery"]["reason"] = "printer.cfg changed";
  auto v = plr_view(parse_plr_status(j), false);
  ASSERT_FALSE(v.show_resume);
  ASSERT_TRUE(v.show_discard);
  ASSERT_TRUE(v.text.find("printer.cfg changed") != std::string::npos);
}

TEST(View, prepared_offers_discard_only) {
  json j = available("physical");
  j["state"] = "RESUME_PREPARED";
  auto v = plr_view(parse_plr_status(j), false);
  ASSERT_FALSE(v.show_resume);
  ASSERT_TRUE(v.show_discard);
}

TEST(View, error_shows_message_and_no_actions) {
  auto v = plr_view(parse_plr_status({{"state", "ERROR"}, {"last_error", "EEPROM read failed"}}), false);
  ASSERT_FALSE(v.show_resume);
  ASSERT_TRUE(v.text.find("EEPROM read failed") != std::string::npos);
}

TEST(View, unknown_state_offers_nothing) {
  auto v = plr_view(parse_plr_status({{"state", "SOMETHING"}, {"resume_mode", "physical"}}), false);
  ASSERT_FALSE(v.show_resume);
  ASSERT_FALSE(v.show_discard);
}

// ---------------------------------------------------------------------------------------
// Commands: the Creality path is gone; the NebulaOS commands are exact
// ---------------------------------------------------------------------------------------

TEST(Commands, resume_is_physical_and_explicitly_unsafe) {
  ASSERT_EQ(std::string(PLR_RESUME_COMMAND),
            std::string("NEBULAOS_RESUME_POWER_LOSS ALLOW_UNSAFE=1 PHYSICAL=1"));
  ASSERT_TRUE(std::string(PLR_RESUME_COMMAND).find("ISCONTINUEPRINT") == std::string::npos);
  ASSERT_TRUE(std::string(PLR_RESUME_COMMAND).find("SDCARD_PRINT_FILE") == std::string::npos);
}

TEST(Commands, discard) {
  ASSERT_EQ(std::string(PLR_DISCARD_COMMAND), std::string("NEBULAOS_CLEAR_POWER_LOSS_RECOVERY"));
}

TEST(Commands, confirm_text_names_the_file_and_the_z_assumption) {
  auto t = plr_resume_confirm_text(parse_plr_status(available("physical")));
  ASSERT_TRUE(t.find("bracket.gcode") != std::string::npos);
  ASSERT_TRUE(t.find("Z is NOT re-homed") != std::string::npos);
}

TEST(Offer, asked_once_per_physical_record) {
  PlrStatus s;
  s.present = true;
  s.state = "RECOVERY_AVAILABLE";
  s.resume_mode = "physical";
  s.file = "benchy.gcode";
  s.progress = 0.37;
  std::string k = plr_offer_key(s, false);
  ASSERT_FALSE(k.empty());
  ASSERT_EQ(plr_offer_key(s, false), k);              // same record: same key
  PlrStatus later = s;
  later.progress = 0.52;
  ASSERT_TRUE(plr_offer_key(later, false) != k);      // a new power loss: asked again
  ASSERT_EQ(plr_offer_key(s, true), std::string(""));  // printing
  PlrStatus restore = s;
  restore.resume_mode = "state_restore_only";
  ASSERT_EQ(plr_offer_key(restore, false), std::string(""));
  PlrStatus going = s;
  going.resume_in_progress = true;
  ASSERT_EQ(plr_offer_key(going, false), std::string(""));
  PlrStatus none = s;
  none.state = "NONE";
  ASSERT_EQ(plr_offer_key(none, false), std::string(""));
}

int main() {
  return minitest::run_all("plr_status") > 0 ? 1 : 0;
}
