// Offline tests for src/print_start.cpp (G112, 2026-10-31). Build/run:
//   make test-print-start
//
// This file may be distributed under the terms of the GNU GPLv3 license.
#include "minitest.h"
#include "../src/print_start.h"

TEST(PrintStart, not_running) {
  StartInput in;
  ASSERT_TRUE(start_steps(in).empty());
  in.step = "something_else";
  ASSERT_TRUE(start_steps(in).empty());
  ASSERT_FALSE(start_running(""));
  ASSERT_TRUE(start_running("mesh"));
}

TEST(PrintStart, heating_bed_shows_temperatures) {
  StartInput in;
  in.step = "heating_bed";
  in.bed = 62.6;
  in.bed_target = 85;
  auto rows = start_steps(in);
  ASSERT_EQ(rows.size(), (size_t)5);   // no soak in this start
  ASSERT_TRUE(rows[0].state == StartStepState::Done);
  ASSERT_TRUE(rows[1].state == StartStepState::Current);
  ASSERT_EQ(rows[1].text, std::string("Heating bed 63 / 85\xc2\xb0" "C"));
  ASSERT_TRUE(rows[2].state == StartStepState::Pending);
  ASSERT_EQ(rows[2].text, std::string("Bed mesh"));
  ASSERT_EQ(rows[4].text, std::string("Purge line"));
}

TEST(PrintStart, soak_countdown) {
  StartInput in;
  in.step = "soak";
  in.soak_seconds = 680;
  in.soak_elapsed = 70;
  auto rows = start_steps(in);
  ASSERT_EQ(rows.size(), (size_t)6);
  ASSERT_EQ(rows[2].text, std::string("Bed soak, 10:10 left"));
  ASSERT_TRUE(rows[2].state == StartStepState::Current);
  in.soak_elapsed = 900;
  ASSERT_EQ(start_steps(in)[2].text, std::string("Bed soak, 0:00 left"));
  in.soak_seconds = 0;
  ASSERT_EQ(start_steps(in)[2].text, std::string("Bed soak"));
}

TEST(PrintStart, soak_stays_listed_after_it) {
  StartInput in;
  in.step = "heating_nozzle";
  in.soaked = true;
  in.nozzle = 180;
  in.nozzle_target = 220;
  auto rows = start_steps(in);
  ASSERT_EQ(rows.size(), (size_t)6);
  ASSERT_TRUE(rows[2].state == StartStepState::Done);
  ASSERT_EQ(rows[2].text, std::string("Bed soak"));
  ASSERT_EQ(rows[4].text, std::string("Heating nozzle 180 / 220\xc2\xb0" "C"));
  ASSERT_TRUE(rows[5].state == StartStepState::Pending);
}

int main() {
  return minitest::run_all("print_start") > 0 ? 1 : 0;
}
