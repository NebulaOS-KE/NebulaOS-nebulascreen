// NebulaOS refusals (G91 v3.4). See nebulaos_refusal.h.
//
// This file may be distributed under the terms of the GNU GPLv3 license.
#include "nebulaos_refusal.h"

#include <cstring>

const char *const REFUSAL_LINE_PREFIX = "!! NebulaOS refused: ";
const char *const REFUSAL_TITLE = "Not now";

namespace {

struct ReasonText {
  const char *reason;
  const char *text;
};

// The screen's words for the firmware's busy reasons (nebulaos_osconfig.common).
const ReasonText REASONS[] = {
  {"PRINTING", "Not possible while a print is running."},
  {"PAUSED", "Not possible while a print is paused."},
  // includes a heater target above 0: a preheated printer refuses too
  {"ACTIVE", "Not while the printer is heating or moving."},
  {"CALIBRATING", "Not possible while a calibration is running."},
  {"UPDATING", "Another update is running."},
  {"LOW_MEMORY", "Not possible right now: the printer is low on memory."},
  {"UNKNOWN", "Printer state unknown - not possible right now."},
};

const char *known(const std::string &code) {
  for (const auto &r : REASONS) {
    if (code == r.reason) return r.reason;
  }
  // older firmware: "BUSY: an MCU flash is already running", "REFUSED: printing"
  if (code == "BUSY") return "UPDATING";
  if (code == "REFUSED") return "PRINTING";
  return nullptr;
}

std::string trim(const std::string &s) {
  size_t b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos) return "";
  size_t e = s.find_last_not_of(" \t\r\n");
  return s.substr(b, e - b + 1);
}

}  // namespace

std::string refusal_reason_text(const std::string &reason) {
  for (const auto &r : REASONS) {
    if (reason == r.reason) return r.text;
  }
  return REASONS[sizeof(REASONS) / sizeof(REASONS[0]) - 1].text;  // UNKNOWN
}

Refusal refusal_from_message(const std::string &message) {
  Refusal r;
  size_t colon = message.find(':');
  std::string code = trim(colon == std::string::npos ? message : message.substr(0, colon));
  const char *reason = known(code);
  if (reason == nullptr) {
    return r;
  }
  r.refused = true;
  r.reason = reason;
  std::string text = colon == std::string::npos ? "" : trim(message.substr(colon + 1));
  if (!text.empty() && text[0] >= 'a' && text[0] <= 'z') {
    text[0] = (char)(text[0] - 'a' + 'A');   // "an MCU flash is already running"
  }
  // The table's words; only UPDATING keeps the printer's own text, which says
  // what runs ("The new system is still being checked", "an MCU flash ...").
  r.text = r.reason == "UPDATING" && !text.empty() ? text : refusal_reason_text(r.reason);
  return r;
}

Refusal refusal_of(const json &reply) {
  if (!reply.is_object() || !reply.contains("error")) {
    return Refusal();
  }
  const json &e = reply["error"];
  if (e.is_string()) {
    return refusal_from_message(e.get<std::string>());
  }
  if (e.is_object() && e.contains("message") && e["message"].is_string()) {
    return refusal_from_message(e["message"].get<std::string>());
  }
  return Refusal();
}

Refusal refusal_hint(bool printing, bool paused) {
  Refusal r;
  if (paused) {
    r.reason = "PAUSED";
  } else if (printing) {
    r.reason = "PRINTING";
  } else {
    return r;
  }
  r.refused = true;
  r.text = refusal_reason_text(r.reason);
  return r;
}

bool is_refusal_line(const std::string &line) {
  // the prefix without its trailing space, so "!! NebulaOS refused:" alone matches too
  size_t n = std::strlen(REFUSAL_LINE_PREFIX) - 1;
  return line.compare(0, n, REFUSAL_LINE_PREFIX, n) == 0;
}

const char *const BUSY_METHOD = "machine.nebulaos.system.busy";

json busy_params_restart() {
  return {{"profile", "restart"}};
}

RestartDecision restart_decision(RestartKind kind, const json &busy_reply) {
  RestartDecision d;
  const json service_params = {{"service", "klipper"}};
  const json *res = nullptr;
  if (busy_reply.is_object() && busy_reply.contains("result")
      && busy_reply["result"].is_object()) {
    res = &busy_reply["result"];
  }
  bool busy = res != nullptr && res->contains("busy") && (*res)["busy"].is_boolean()
              && (*res)["busy"].get<bool>();
  if (busy) {
    d.refused = true;
    d.refusal.refused = true;
    std::string reason = res->contains("reason") && (*res)["reason"].is_string()
                         ? (*res)["reason"].get<std::string>() : "";
    std::string message = res->contains("message") && (*res)["message"].is_string()
                          ? trim((*res)["message"].get<std::string>()) : "";
    const char *k = known(reason);
    d.refusal.reason = k != nullptr ? k : "UNKNOWN";
    // the table's words; the printer's own text where the table has none
    // that says what runs (UPDATING) or the reason is new to this screen
    d.refusal.text = (k == nullptr || d.refusal.reason == "UPDATING") && !message.empty()
                     ? message : refusal_reason_text(d.refusal.reason);
    return d;
  }
  d.check_unavailable = res == nullptr;
  std::string klippy = res != nullptr && res->contains("klippy") && (*res)["klippy"].is_string()
                       ? (*res)["klippy"].get<std::string>() : "";
  // no answer at all: Klippy's state is not known either - the plain call,
  // as before the check existed
  bool ready = d.check_unavailable || klippy == "ready";
  // Restart Klipper: anything but a ready Klippy -> the service restart (G95).
  // Restart MCU: a shut-down MCU (Klippy answers, "not_ready") needs
  // FIRMWARE_RESTART; but a Klippy that does not answer at all ("unknown" or a
  // state this screen does not know) cannot run it and the call may never
  // return - restart the service instead (Run 18 verifier finding 2).
  bool mcu_unreachable = kind == RestartKind::MCU && !d.check_unavailable
                         && klippy != "ready" && klippy != "not_ready";
  if ((kind == RestartKind::KLIPPER && !ready) || mcu_unreachable) {
    d.method = "machine.services.restart";
    d.params = service_params;
    return d;
  }
  d.method = kind == RestartKind::KLIPPER ? "printer.restart" : "printer.firmware_restart";
  // Klippy did not take it (hung, disconnected): restart the service. NebulaOS's
  // supervisorctl shim still refuses that while a print runs.
  d.fallback_method = "machine.services.restart";
  d.fallback_params = service_params;
  return d;
}
