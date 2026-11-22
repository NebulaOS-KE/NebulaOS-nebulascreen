// Offline tests for the wake-touch gate (#27, 2026-11-22): the tap that wakes
// the dark display must never press what is on screen (the power-loss
// "Resume?" popup was pressed that way and the printer resumed on its own).
// Build/run: make test-touch-wake-gate
//
// This file may be distributed under the terms of the GNU GPLv3 license.
#include "minitest.h"
#include "../src/touch_wake_gate.h"

TEST(wake_gate, awake_touch_passes_through) {
  TouchWakeGate g;
  bool wake;
  ASSERT_TRUE(g.filter(true, false, &wake));
  ASSERT_FALSE(wake);
  ASSERT_TRUE(g.filter(true, false, &wake));
  ASSERT_FALSE(g.filter(false, false, &wake));
}

TEST(wake_gate, wake_tap_is_swallowed_and_wakes) {
  TouchWakeGate g;
  bool wake;
  ASSERT_FALSE(g.filter(true, true, &wake));
  ASSERT_TRUE(wake);
  ASSERT_FALSE(g.filter(false, true, &wake));
  ASSERT_FALSE(wake);
}

TEST(wake_gate, held_wake_touch_stays_swallowed_after_display_woke) {
  TouchWakeGate g;
  bool wake;
  ASSERT_FALSE(g.filter(true, true, &wake));
  ASSERT_TRUE(wake);
  for (int i = 0; i < 50; i++) {      // display awake now, same finger down
    ASSERT_FALSE(g.filter(true, false, &wake));
    ASSERT_FALSE(wake);
  }
  ASSERT_FALSE(g.filter(false, false, &wake));
  ASSERT_TRUE(g.filter(true, false, &wake));   // next tap is a normal tap
  ASSERT_FALSE(wake);
}

TEST(wake_gate, press_that_began_awake_is_not_cut_off) {
  TouchWakeGate g;
  bool wake;
  ASSERT_TRUE(g.filter(true, false, &wake));
  ASSERT_TRUE(g.filter(true, true, &wake));
  ASSERT_FALSE(wake);
}

TEST(wake_gate, two_wake_taps_in_a_row_each_only_wake) {
  TouchWakeGate g;
  bool wake;
  ASSERT_FALSE(g.filter(true, true, &wake));
  ASSERT_TRUE(wake);
  ASSERT_FALSE(g.filter(false, true, &wake));
  ASSERT_FALSE(g.filter(true, true, &wake));
  ASSERT_TRUE(wake);
}

int main() {
  return minitest::run_all("touch_wake_gate") > 0 ? 1 : 0;
}
