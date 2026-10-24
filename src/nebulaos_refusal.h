#ifndef __NEBULAOS_REFUSAL_H__
#define __NEBULAOS_REFUSAL_H__

#include "hv/json.hpp"

#include <cstdint>
#include <string>

using json = nlohmann::json;

// Pure, LVGL-free logic for NebulaOS refusals (G91 v3.4, Run 17).
//
// NebulaOS refuses an update or maintenance action at once while the printer is busy
// (one shared busy check in the firmware). A refusal arrives two ways:
//   - as the JSON-RPC error of the machine.nebulaos.* call the screen made:
//       {"error": {"code": 409, "message": "<REASON>: <text>"}}
//     REASON: PRINTING, PAUSED, ACTIVE, CALIBRATING, UPDATING, LOW_MEMORY, UNKNOWN;
//   - as a console line "!! NebulaOS refused: <text>" for every client (Mainsail shows it
//     as a red console line / toast).
// The screen shows ONE popup ("Not now" + the reason text) for the error of its own call,
// and keeps the "!! NebulaOS refused:" lines out of the Klipper-error popup and the M600
// runout handling (the line is about an action, never a printer error).
//
// This file may be distributed under the terms of the GNU GPLv3 license.

extern const char *const REFUSAL_LINE_PREFIX;  // "!! NebulaOS refused: "
extern const char *const REFUSAL_TITLE;        // "Not now"

struct Refusal {
  bool refused = false;
  std::string reason;   // PRINTING, PAUSED, ... (legacy BUSY -> UPDATING, REFUSED -> PRINTING)
  std::string text;     // what the popup says
};

// The one mapping table: reason code -> text. Unknown codes get UNKNOWN's text.
std::string refusal_reason_text(const std::string &reason);

// A JSON-RPC reply (or its error message) that is a NebulaOS refusal: "<REASON>: <text>"
// with a known reason code. The text is the table's, except for UPDATING, where the
// printer's own text says what is running ("The new system is still being checked").
Refusal refusal_of(const json &reply);
Refusal refusal_from_message(const std::string &message);

// The screen's own pre-check before it asks NebulaOS (a hint only; NebulaOS decides):
// a print running or paused.
Refusal refusal_hint(bool printing, bool paused);

// True for a "!! NebulaOS refused: ..." console line.
bool is_refusal_line(const std::string &line);

// G98 (Run 18): the screen's Restart Klipper / Restart MCU follow the same rule
// as Mainsail. Before the restart the screen asks
//   machine.nebulaos.system.busy {"profile": "restart"}
//   -> {"busy": bool, "reason": "PRINTING"|..., "message": str,
//       "klippy": "ready"|"not_ready"|"unknown", "profile": "restart"}
// (NebulaOS-firmware 3088ea5; the restart profile follows G95: a hung Klippy
// or a failed check is not busy).
//   - busy: the "Not now" popup, nothing is restarted;
//   - the endpoint is missing or fails (older firmware, timeout): the restart
//     is allowed - a restart is never blocked because the check is unavailable
//     (older firmware: the screen's own "a print is running" confirmation);
//   - Klippy not ready / unknown (hung, shut down, disconnected): Klipper is
//     restarted as a service (machine.services.restart klipper), which works
//     even when Klippy does not answer (Tim G95). Restart MCU always tries
//     printer.firmware_restart first (what a shut-down MCU needs);
//   - every printer.* restart falls back to the service restart when Klippy
//     does not take it (error reply); NebulaOS's supervisorctl shim still
//     refuses that while a print runs.
extern const char *const BUSY_METHOD;   // machine.nebulaos.system.busy
json busy_params_restart();             // {"profile": "restart"}

enum class RestartKind { KLIPPER, MCU };

struct RestartDecision {
  bool refused = false;            // show `refusal`, restart nothing
  Refusal refusal;
  bool check_unavailable = false;  // endpoint missing/failed: confirm while printing, as before
  std::string method;              // the JSON-RPC call that restarts
  json params = json::object();
  std::string fallback_method;     // "" or the call to use if `method` fails
  json fallback_params = json::object();
};

RestartDecision restart_decision(RestartKind kind, const json &busy_reply);

// G107 (Run 19): one user action gives ONE refusal popup.
// A single refused action can produce several "!! NebulaOS refused:" lines:
// Moonraker's "Update" runs several git commands and retries a refused one;
// each refusal is one line. The screen shows one "Not now" popup for them.
//
// A console line as a refusal: "[<action> - ]<text>[ (<REASON>)]", e.g.
//   "!! NebulaOS refused: update (git reset) - Not possible while a print is paused. (PAUSED)"
//   "!! NebulaOS refused: restart klipper - Not possible while the printer is moving or heating."
//   "!! NebulaOS refused: Not possible while a print is running."
// The action prefix is dropped (the popup says "Not now"), the reason comes
// from the "(REASON)" suffix or, without one, from the firmware's own text.
// The popup text follows refusal_from_message: the table's words, the
// printer's own text for UPDATING or a text this screen does not know.
// Not a refusal line -> refused = false.
Refusal refusal_from_line(const std::string &line);

// Identical refusals = same reason + same popup text.
std::string refusal_key(const Refusal &r);

// The dedupe rule, pure and time-injected (now_ms: any monotonic ms clock,
// wrap-safe in 32 bits, e.g. lv_tick_get()).
//   - own(): a refusal of the screen's own call. Its popup is shown at once
//     (by the caller, as before); it is only remembered here, so the console
//     line the same refusal produces does not pop up a second one.
//   - line(): a "!! NebulaOS refused:" line. DROP when the same refusal was
//     seen (own or line) less than REFUSAL_DEDUPE_MS before - the window
//     slides with every identical one, so a retry chain stays one popup, also
//     after the popup was closed. A different refusal SHOWs a popup, or
//     REPLACEs the one still open (never a second popup on top).
//   - closed(): the line popup was dismissed.
extern const uint32_t REFUSAL_DEDUPE_MS;  // 10 s

class RefusalGate {
 public:
  enum class Action { SHOW, REPLACE, DROP };
  void own(const Refusal &r, uint32_t now_ms);
  Action line(const Refusal &r, uint32_t now_ms);
  void closed() { open_ = false; }
  bool popup_open() const { return open_; }

 private:
  bool seen_ = false;
  std::string key_;
  uint32_t last_ms_ = 0;
  bool open_ = false;
};

// The screen's one gate (LVGL thread, under lv_lock): settings_ui notes its
// own refusals, NotificationManager asks it for the console lines.
RefusalGate &refusal_gate();

#endif  // __NEBULAOS_REFUSAL_H__
