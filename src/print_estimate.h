#ifndef __PRINT_ESTIMATE_H__
#define __PRINT_ESTIMATE_H__

// Print progress and time left, computed exactly like Mainsail (Run 21, Tim:
// "the screen and Mainsail must show the same"). Ported from Mainsail's
// printer store getters (getPrintPercent*, getEstimatedTimeFile/Filament,
// getEstimatedTimeAvg) and its gui.general defaults:
//   calcPrintProgress = "file-relative", calcEstimateTime = [file, filament].
// Pure logic: no LVGL, no websocket (tests/test_print_estimate.cpp).

#include <cstdint>
#include <string>

#include "hv/json.hpp"

using json = nlohmann::json;

struct PrintEstimateInput {
  double print_duration = 0.0;     // print_stats.print_duration
  double filament_used = 0.0;      // print_stats.filament_used
  std::string print_filename;      // print_stats.filename
  double file_position = 0.0;      // virtual_sdcard.file_position
  double sd_progress = 0.0;        // virtual_sdcard.progress
  double slicer_progress = 0.0;    // display_status.progress (M73)
  std::string file_filename;       // metadata filename
  double gcode_start_byte = 0.0;   // metadata (0 = unknown)
  double gcode_end_byte = 0.0;
  double filament_total = 0.0;     // metadata (0 = unknown)
  bool have_filament_total = false;
};

struct PrintEstimateSettings {
  std::string progress = "file-relative";  // file-relative|file-absolute|slicer|filament
  bool use_file = true;
  bool use_filament = true;
};

// Mainsail's gui.general from Moonraker's "mainsail" database namespace
// (key "general"); anything missing keeps Mainsail's default.
PrintEstimateSettings print_estimate_settings(const json &general);

double print_percent(const PrintEstimateInput &in, const PrintEstimateSettings &s);

// Seconds left as Mainsail's "Estimate" shows it; 0 = unknown ("--").
double print_time_left(const PrintEstimateInput &in, const PrintEstimateSettings &s);

// Mainsail's formatDuration: [-]H:MM:SS (hours not padded).
std::string format_duration(double seconds);

#endif // __PRINT_ESTIMATE_H__
