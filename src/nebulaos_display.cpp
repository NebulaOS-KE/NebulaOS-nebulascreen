#include "nebulaos_display.h"

const char *const DISPLAY_STATUS_METHOD = "machine.nebulaos.display.status";
const char *const DISPLAY_BRIGHTNESS_METHOD = "machine.nebulaos.display.brightness";
const char *const DISPLAY_SLEEP_METHOD = "machine.nebulaos.display.sleep";
const char *const DISPLAY_WAKE_METHOD = "machine.nebulaos.display.wake";

DisplayStatus parse_display_status(const json &reply) {
  DisplayStatus s;
  if (!reply.is_object() || !reply.contains("result") || !reply["result"].is_object()) {
    return s;
  }
  const json &r = reply["result"];
  if (r.contains("levels") && r["levels"].is_array()) {
    for (const auto &l : r["levels"]) {
      if (l.is_number_integer() && l.template get<int>() > 0) {
        s.levels.push_back(l.template get<int>());
      }
    }
  }
  if (r.contains("config") && r["config"].is_object()) {
    const json &c = r["config"];
    if (c.contains("brightness") && c["brightness"].is_number_integer()) {
      s.brightness = c["brightness"].template get<int>();
    }
  }
  bool backlight = r.contains("backlight") && r["backlight"].is_object()
    && r["backlight"].contains("available") && r["backlight"]["available"].is_boolean()
    && r["backlight"]["available"].template get<bool>();
  s.available = backlight && !s.levels.empty();
  return s;
}

std::string brightness_dropdown_options(const DisplayStatus &s) {
  std::string out;
  for (size_t i = 0; i < s.levels.size(); ++i) {
    if (i > 0) {
      out += '\n';
    }
    out += std::to_string(s.levels[i]) + "%";
  }
  return out;
}

int brightness_dropdown_index(const DisplayStatus &s) {
  if (s.levels.empty()) {
    return 0;
  }
  for (size_t i = 0; i < s.levels.size(); ++i) {
    if (s.levels[i] == s.brightness) {
      return (int)i;
    }
  }
  int best = 0;
  for (size_t i = 1; i < s.levels.size(); ++i) {
    if (s.levels[i] > s.levels[best]) {
      best = (int)i;
    }
  }
  return best;
}

std::string jsonrpc_error_message(const json &reply) {
  if (!reply.is_object() || !reply.contains("error")) {
    return "";
  }
  const json &e = reply["error"];
  if (e.is_object() && e.contains("message") && e["message"].is_string()) {
    return e["message"].template get<std::string>();
  }
  return e.dump();
}
