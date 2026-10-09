#include "nebulaos_macros.h"

const char *const NEBULAOS_LOAD_FILAMENT = "LOAD_FILAMENT";
const char *const NEBULAOS_UNLOAD_FILAMENT = "UNLOAD_FILAMENT";

std::string filament_macro(const std::string &configured, bool load) {
  if (configured.empty() || configured == "_GUPPY_LOAD_MATERIAL"
      || configured == "_GUPPY_QUIT_MATERIAL") {
    return load ? NEBULAOS_LOAD_FILAMENT : NEBULAOS_UNLOAD_FILAMENT;
  }
  return configured;
}

std::string unload_filament_gcode(const std::string &configured, int temp) {
  std::string macro = filament_macro(configured, false);
  if (macro == NEBULAOS_UNLOAD_FILAMENT) {
    return macro + " EXTRUDER_TEMP=" + std::to_string(temp);
  }
  return macro;
}
