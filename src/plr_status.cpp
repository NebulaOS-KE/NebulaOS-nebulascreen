#include "plr_status.h"

#include <cmath>
#include <cstdio>

const char *const PLR_RESUME_COMMAND = "NEBULAOS_RESUME_POWER_LOSS ALLOW_UNSAFE=1 PHYSICAL=1";
const char *const PLR_DISCARD_COMMAND = "NEBULAOS_CLEAR_POWER_LOSS_RECOVERY";

static std::string get_string(const json &j, const char *key) {
  if (j.is_object() && j.contains(key) && j[key].is_string()) {
    return j[key].template get<std::string>();
  }
  return "";
}

PlrStatus parse_plr_status(const json &snapshot) {
  PlrStatus s;
  if (!snapshot.is_object()) {
    return s;
  }
  s.present = true;
  s.state = get_string(snapshot, "state");
  s.resume_mode = get_string(snapshot, "resume_mode");
  s.last_error = get_string(snapshot, "last_error");
  if (snapshot.contains("resume_in_progress") && snapshot["resume_in_progress"].is_boolean()) {
    s.resume_in_progress = snapshot["resume_in_progress"].template get<bool>();
  }
  if (snapshot.contains("recovery") && snapshot["recovery"].is_object()) {
    const json &r = snapshot["recovery"];
    s.file = get_string(r, "file");
    s.reason = get_string(r, "reason");
    if (r.contains("progress") && r["progress"].is_number()) {
      double p = r["progress"].template get<double>();
      if (std::isfinite(p)) {
        s.progress = p < 0.0 ? 0.0 : (p > 1.0 ? 1.0 : p);
      }
    }
  }
  return s;
}

std::string plr_display_name(const std::string &path) {
  auto slash = path.find_last_of('/');
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

static std::string file_line(const PlrStatus &s) {
  char pct[16];
  std::snprintf(pct, sizeof(pct), "%d%%", (int)std::lround(s.progress * 100.0));
  std::string name = s.file.empty() ? std::string("(unknown file)") : plr_display_name(s.file);
  return "Interrupted print: " + name + " at " + pct;
}

PlrView plr_view(const PlrStatus &s, bool printing) {
  PlrView v;
  if (!s.present) {
    v.text = "Power-loss recovery is not available on this printer.";
    return v;
  }
  if (s.resume_in_progress) {
    v.text = "Resuming...";
    return v;
  }
  if (printing || s.state == "RECORDING") {
    v.text = "A print is running and being recorded. If power is lost, reopen this screen.";
    return v;
  }
  if (s.state == "NONE") {
    v.text = "No interrupted print to recover.";
    return v;
  }
  if (s.state == "RECOVERY_AVAILABLE") {
    v.show_discard = true;
    if (s.resume_mode == "physical") {
      v.text = file_line(s) + "\nResume from where power was lost?";
      v.show_resume = true;
    } else {
      v.text = file_line(s) +
        "\nAutomatic resume is not enabled on this printer. Start the print again or discard this record.";
    }
    return v;
  }
  if (s.state == "RECOVERY_INCOMPATIBLE") {
    v.show_discard = true;
    v.text = file_line(s) + "\nCannot be resumed: " +
      (s.reason.empty() ? std::string("the file or printer configuration changed") : s.reason);
    return v;
  }
  if (s.state == "RESUME_PREPARED") {
    v.show_discard = true;
    v.text = file_line(s) +
      "\nPrint state was restored from the console; continue or discard it there.";
    return v;
  }
  if (s.state == "ERROR") {
    v.text = "Power-loss recovery error: " +
      (s.last_error.empty() ? std::string("see klippy.log") : s.last_error);
    return v;
  }
  v.text = "Power-loss recovery status unknown.";
  return v;
}

std::string plr_offer_key(const PlrStatus &s, bool printing) {
  if (!plr_view(s, printing).show_resume) return "";
  char buf[32];
  std::snprintf(buf, sizeof(buf), "|%.6f", s.progress);
  return s.file + buf;
}

std::string plr_resume_confirm_text(const PlrStatus &s) {
  return "Resume " + plr_display_name(s.file) + "?\n\n"
    "The printer heats up, homes X and Y, lifts the nozzle, primes and continues printing. "
    "Z is NOT re-homed: it assumes the bed and nozzle did not move while the power was off. "
    "Only continue if the part is still firmly on the bed.";
}
