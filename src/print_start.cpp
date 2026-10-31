#include "print_start.h"

#include <algorithm>
#include <cstdio>

// G112 (2026-10-31): see print_start.h.
//
// This file may be distributed under the terms of the GNU GPLv3 license.

namespace {
const std::vector<std::string> &order() {
  static const std::vector<std::string> o = {"preparing", "heating_bed", "soak", "mesh",
                                             "heating_nozzle", "purge"};
  return o;
}

std::string temps(const char *what, double t, double target) {
  char buf[64];
  snprintf(buf, sizeof(buf), "%s %.0f / %.0f\xc2\xb0" "C", what, t, target);
  return buf;
}

std::string text_of(const std::string &key, bool current, const StartInput &in) {
  if (key == "preparing") return "Preparing (homing)";
  if (key == "heating_bed") return current ? temps("Heating bed", in.bed, in.bed_target) : "Heating bed";
  if (key == "soak") {
    if (!current || in.soak_seconds <= 0) return "Bed soak";
    int left = std::max(0, in.soak_seconds - in.soak_elapsed);
    char buf[48];
    snprintf(buf, sizeof(buf), "Bed soak, %d:%02d left", left / 60, left % 60);
    return buf;
  }
  if (key == "mesh") return "Bed mesh";
  if (key == "heating_nozzle")
    return current ? temps("Heating nozzle", in.nozzle, in.nozzle_target) : "Heating nozzle";
  return "Purge line";
}
}  // namespace

bool start_running(const std::string &step) {
  return std::find(order().begin(), order().end(), step) != order().end();
}

std::vector<StartStepRow> start_steps(const StartInput &in) {
  std::vector<StartStepRow> rows;
  if (!start_running(in.step)) return rows;
  bool soak = in.soaked || in.step == "soak";
  auto cur = std::find(order().begin(), order().end(), in.step);
  for (auto it = order().begin(); it != order().end(); ++it) {
    if (*it == "soak" && !soak) continue;
    StartStepState s = it < cur ? StartStepState::Done
                     : it == cur ? StartStepState::Current : StartStepState::Pending;
    rows.push_back({text_of(*it, it == cur, in), s});
  }
  return rows;
}
