// Offline tests for src/print_estimate.cpp (Run 21): progress and time left
// exactly as Mainsail computes them. Expected values worked out by hand from
// Mainsail's own getters. Build/run:
//   make test-print-estimate
//
// This file may be distributed under the terms of the GNU GPLv3 license.
#include "minitest.h"
#include "../src/print_estimate.h"

static PrintEstimateInput mid_print() {
  PrintEstimateInput in;
  in.print_duration = 600;
  in.print_filename = in.file_filename = "a.gcode";
  in.gcode_start_byte = 1000;
  in.gcode_end_byte = 11000;
  in.file_position = 3500;       // 25 % of the G-code itself
  in.sd_progress = 0.30;         // whole file incl. header/thumbnails
  in.slicer_progress = 0.40;
  in.filament_used = 1000;
  in.filament_total = 5000;      // 20 %
  in.have_filament_total = true;
  return in;
}

TEST(Percent, file_relative_is_the_default) {
  PrintEstimateSettings s;
  ASSERT_NEAR(print_percent(mid_print(), s), 0.25, 1e-12);
}

TEST(Percent, file_relative_edges_and_fallback) {
  PrintEstimateSettings s;
  auto in = mid_print();
  in.file_position = 900;
  ASSERT_NEAR(print_percent(in, s), 0.0, 1e-12);
  in.file_position = 11000;
  ASSERT_NEAR(print_percent(in, s), 1.0, 1e-12);
  in = mid_print();
  in.file_filename = "other.gcode";              // metadata of another file
  ASSERT_NEAR(print_percent(in, s), 0.30, 1e-12);
  in = mid_print();
  in.gcode_start_byte = 0;                       // no byte range known
  ASSERT_NEAR(print_percent(in, s), 0.30, 1e-12);
}

TEST(Percent, other_modes) {
  PrintEstimateSettings s;
  s.progress = "file-absolute";
  ASSERT_NEAR(print_percent(mid_print(), s), 0.30, 1e-12);
  s.progress = "slicer";
  ASSERT_NEAR(print_percent(mid_print(), s), 0.40, 1e-12);
  s.progress = "filament";
  ASSERT_NEAR(print_percent(mid_print(), s), 0.20, 1e-12);
  s.progress = "something-new";                  // Mainsail's default branch
  ASSERT_NEAR(print_percent(mid_print(), s), 0.25, 1e-12);
}

TEST(TimeLeft, average_of_file_and_filament) {
  PrintEstimateSettings s;
  // file 600/0.25-600 = 1800, filament 600/0.2-600 = 2400 -> 2100
  ASSERT_NEAR(print_time_left(mid_print(), s), 2100, 1e-9);
}

TEST(TimeLeft, slicer_estimate_is_not_part_of_it) {
  PrintEstimateSettings s;
  auto in = mid_print();
  in.filament_total = 0;                         // no filament estimate
  in.have_filament_total = false;
  ASSERT_NEAR(print_time_left(in, s), 1800, 1e-9);
}

TEST(TimeLeft, each_estimate_rounded_before_the_mean) {
  PrintEstimateSettings s;
  s.use_filament = false;
  auto in = mid_print();
  in.print_duration = 700;
  in.file_position = 4000;                       // 30 %: 700/0.3-700 = 1633.33
  ASSERT_NEAR(print_time_left(in, s), 1633, 1e-9);
}

TEST(TimeLeft, nothing_known_is_zero) {
  PrintEstimateSettings s;
  PrintEstimateInput in;
  ASSERT_NEAR(print_time_left(in, s), 0, 1e-12);
  auto mp = mid_print();
  mp.print_duration = 0;
  ASSERT_NEAR(print_time_left(mp, s), 0, 1e-12);
}

TEST(Settings, mainsail_gui_general) {
  auto s = print_estimate_settings(json::parse(
    R"({"calcPrintProgress":"slicer","calcEstimateTime":["filament"]})"));
  ASSERT_EQ(s.progress, std::string("slicer"));
  ASSERT_FALSE(s.use_file);
  ASSERT_TRUE(s.use_filament);
  auto d = print_estimate_settings(json());      // nothing saved: defaults
  ASSERT_EQ(d.progress, std::string("file-relative"));
  ASSERT_TRUE(d.use_file && d.use_filament);
  auto none = print_estimate_settings(json::parse(R"({"calcEstimateTime":[]})"));
  ASSERT_NEAR(print_time_left(mid_print(), none), 0, 1e-12);
}

TEST(Format, like_mainsail) {
  ASSERT_EQ(format_duration(2100), std::string("0:35:00"));
  ASSERT_EQ(format_duration(3725.9), std::string("1:02:05"));
  ASSERT_EQ(format_duration(59), std::string("0:00:59"));
  ASSERT_EQ(format_duration(100000), std::string("27:46:40"));
  ASSERT_EQ(format_duration(-5), std::string("-0:00:05"));
}

int main() {
  return minitest::run_all("print_estimate") > 0 ? 1 : 0;
}
