#ifndef __PRINT_START_H__
#define __PRINT_START_H__

#include <string>
#include <vector>

// G112 (2026-10-31): the step list while a print starts, LVGL-free.
// START_PRINT (firmware print.cfg) records its step in
// printer['gcode_macro START_PRINT'].step - preparing, heating_bed, soak,
// mesh, heating_nozzle, purge; "" when it is not running - and the soak
// length in .soak_seconds (_WARP_STABILISE).
//
// This file may be distributed under the terms of the GNU GPLv3 license.

struct StartInput {
  std::string step;
  int soak_seconds = 0;   // 0 = not known (yet)
  int soak_elapsed = 0;   // seconds since the screen saw the soak start
  bool soaked = false;    // this start has a soak step (seen at least once)
  double bed = 0, bed_target = 0;
  double nozzle = 0, nozzle_target = 0;
};

enum class StartStepState { Done, Current, Pending };

struct StartStepRow {
  std::string text;
  StartStepState state;
};

// True while START_PRINT runs (a known step).
bool start_running(const std::string &step);
// The rows, in order; empty when START_PRINT is not running.
std::vector<StartStepRow> start_steps(const StartInput &in);

#endif // __PRINT_START_H__
