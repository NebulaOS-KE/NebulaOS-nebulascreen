// Offline tests for the GuppyScreen -> NebulaOS macro mapping - no LVGL, no Klipper.
// Build/run:
//   make test-nebulaos-macros
//
// This file may be distributed under the terms of the GNU GPLv3 license.
#include "minitest.h"
#include "../src/nebulaos_macros.h"

TEST(Filament, legacy_guppy_names_map_to_nebulaos) {
  ASSERT_EQ(filament_macro("_GUPPY_LOAD_MATERIAL", true), std::string("LOAD_FILAMENT"));
  ASSERT_EQ(filament_macro("_GUPPY_QUIT_MATERIAL", false), std::string("UNLOAD_FILAMENT"));
  ASSERT_EQ(filament_macro("", false), std::string("UNLOAD_FILAMENT"));
}

TEST(Filament, user_macro_is_kept) {
  ASSERT_EQ(filament_macro("MY_UNLOAD", false), std::string("MY_UNLOAD"));
}

TEST(Filament, unload_passes_the_temperature_to_the_nebulaos_macro) {
  ASSERT_EQ(unload_filament_gcode("_GUPPY_QUIT_MATERIAL", 240),
            std::string("UNLOAD_FILAMENT EXTRUDER_TEMP=240"));
  ASSERT_EQ(unload_filament_gcode("UNLOAD_FILAMENT", 220),
            std::string("UNLOAD_FILAMENT EXTRUDER_TEMP=220"));
  ASSERT_EQ(unload_filament_gcode("MY_UNLOAD", 220), std::string("MY_UNLOAD"));
}

int main() {
  return minitest::run_all("nebulaos_macros") > 0 ? 1 : 0;
}
