// Offline tests for the touch calibration plausibility checks - no LVGL.
// Build/run:
//   make test-touch-calibration-check
//
// The crosshairs are the wizard's automatic points on the KE's 480x272 panel -
// (40,40), (440,40), (240,232) - turned 180 degrees into the touch driver's
// space, as the wizard does for the default display_rotate 2.
//
// This file may be distributed under the terms of the GNU GPLv3 license.
#include "minitest.h"
extern "C" {
#include "../lv_touch_calibration/lv_tc_check.h"
}

static const lv_tc_check_point_t SCR[3] = {{439, 231}, {39, 231}, {239, 39}};

static bool tri(const lv_tc_check_point_t (&tch)[3]) {
  return lv_tc_check_triangle(SCR, tch, 60, 170);
}

TEST(Taps, close_together_pass) {
  ASSERT_TRUE(lv_tc_check_taps_close({100, 100}, {120, 115}, 40));
}

TEST(Taps, exactly_at_the_limit_pass) {
  ASSERT_TRUE(lv_tc_check_taps_close({100, 100}, {140, 140}, 40));
}

TEST(Taps, one_tap_elsewhere_fails) {
  // Two taps on the crosshair, one on a popup's OK button in the middle.
  ASSERT_FALSE(lv_tc_check_taps_close({239, 136}, {439, 231}, 40));
}

TEST(Taps, spread_on_one_axis_fails) {
  ASSERT_FALSE(lv_tc_check_taps_close({100, 100}, {105, 160}, 40));
}

TEST(Triangle, accurate_taps_pass) {
  lv_tc_check_point_t t[3] = {{436, 228}, {42, 234}, {241, 36}};
  ASSERT_TRUE(tri(t));
}

TEST(Triangle, panel_with_smaller_range_passes) {
  // A touch layer that reports only about 80 % of the range on each axis,
  // shifted by 20 pixels: the area is about 64 % of the crosshairs'.
  lv_tc_check_point_t t[3] = {{419, 221}, {99, 221}, {259, 67}};
  ASSERT_TRUE(tri(t));
}

TEST(Triangle, one_point_in_the_middle_fails) {
  // The last crosshair's taps went to the middle of the screen instead.
  lv_tc_check_point_t t[3] = {{439, 231}, {39, 231}, {239, 136}};
  ASSERT_FALSE(tri(t));
}

TEST(Triangle, stretched_too_far_fails) {
  lv_tc_check_point_t t[3] = {{479, 271}, {0, 271}, {239, 0}};
  ASSERT_TRUE(tri(t));  // the whole screen is only about 1.7 x, still plausible
  lv_tc_check_point_t u[3] = {{479, 271}, {-200, 271}, {239, -100}};
  ASSERT_FALSE(tri(u));
}

TEST(Triangle, all_taps_in_one_place_fail) {
  lv_tc_check_point_t t[3] = {{240, 136}, {242, 138}, {239, 135}};
  ASSERT_FALSE(tri(t));
}

TEST(Triangle, points_in_a_line_fail) {
  lv_tc_check_point_t t[3] = {{50, 50}, {150, 100}, {250, 150}};
  ASSERT_FALSE(tri(t));
}

TEST(Triangle, degenerate_crosshairs_fail) {
  lv_tc_check_point_t s[3] = {{10, 10}, {10, 10}, {10, 10}};
  lv_tc_check_point_t t[3] = {{436, 228}, {42, 234}, {241, 36}};
  ASSERT_FALSE(lv_tc_check_triangle(s, t, 60, 170));
}

int main() {
  return minitest::run_all("touch_calibration_check") > 0 ? 1 : 0;
}
