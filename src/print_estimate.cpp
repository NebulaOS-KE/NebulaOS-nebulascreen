#include "print_estimate.h"

#include <cmath>
#include <cstdio>

// Mainsail sends its estimates through toFixed(0) and parseInt(): each one is
// rounded to whole seconds (half away from zero) before they are averaged.
static double mainsail_round(double v) {
  return static_cast<double>(static_cast<int64_t>(std::llround(v)));
}

PrintEstimateSettings print_estimate_settings(const json &general) {
  PrintEstimateSettings s;
  if (!general.is_object()) {
    return s;
  }
  auto p = general.find("calcPrintProgress");
  if (p != general.end() && p->is_string()) {
    s.progress = p->get<std::string>();
  }
  auto e = general.find("calcEstimateTime");
  if (e != general.end() && e->is_array()) {
    s.use_file = s.use_filament = false;
    for (auto &k : *e) {
      if (k == "file") s.use_file = true;
      if (k == "filament") s.use_filament = true;
    }
  }
  return s;
}

static double percent_file_relative(const PrintEstimateInput &in) {
  if (!in.file_filename.empty() && in.gcode_start_byte > 0 && in.gcode_end_byte > 0
      && in.file_filename == in.print_filename) {
    if (in.file_position <= in.gcode_start_byte) return 0.0;
    if (in.file_position >= in.gcode_end_byte) return 1.0;
    double done = in.file_position - in.gcode_start_byte;
    double total = in.gcode_end_byte - in.gcode_start_byte;
    if (done > 0 && total > 0) return 1.0 / total * done;
  }
  return in.sd_progress;
}

double print_percent(const PrintEstimateInput &in, const PrintEstimateSettings &s) {
  if (s.progress == "file-absolute") return in.sd_progress;
  if (s.progress == "slicer") return in.slicer_progress;
  if (s.progress == "filament") {
    if (in.have_filament_total) {
      if (in.filament_total == 0) return 0.0;
      double r = in.filament_used / in.filament_total;
      return r > 1 ? 1.0 : r;
    }
    return in.sd_progress;
  }
  return percent_file_relative(in);
}

double print_time_left(const PrintEstimateInput &in, const PrintEstimateSettings &s) {
  double sum = 0.0;
  int count = 0;
  double pct = print_percent(in, s);
  if (s.use_file && in.print_duration > 0 && pct > 0) {
    double file = mainsail_round(in.print_duration / pct - in.print_duration);
    if (file > 0) { sum += file; ++count; }
  }
  if (s.use_filament && in.print_duration > 0 && in.filament_used > 0
      && in.filament_total > 0 && in.filament_total > in.filament_used) {
    double fil = mainsail_round(in.print_duration / (in.filament_used / in.filament_total)
                                - in.print_duration);
    if (fil > 0) { sum += fil; ++count; }
  }
  return (sum != 0 && count) ? sum / count : 0.0;
}

std::string format_duration(double seconds) {
  std::string sign = seconds < 0 ? "-" : "";
  double n = std::fabs(seconds);
  int64_t h = static_cast<int64_t>(std::floor(n / 3600));
  n = std::fmod(n, 3600);
  int m = static_cast<int>(std::floor(n / 60));
  int sec = static_cast<int>(std::floor(std::fmod(n, 60)));
  char buf[48];
  snprintf(buf, sizeof(buf), "%s%lld:%02d:%02d", sign.c_str(), static_cast<long long>(h), m, sec);
  return buf;
}
