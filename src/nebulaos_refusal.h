#ifndef __NEBULAOS_REFUSAL_H__
#define __NEBULAOS_REFUSAL_H__

#include "hv/json.hpp"

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

#endif  // __NEBULAOS_REFUSAL_H__
