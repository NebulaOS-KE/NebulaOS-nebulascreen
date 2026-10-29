#ifndef __PLR_STATUS_H__
#define __PLR_STATUS_H__

#include "hv/json.hpp"

#include <string>

using json = nlohmann::json;

// Pure, LVGL-free logic for the NebulaOS power-loss recovery panel. The source of truth is
// the Klipper object `nebulaos_power_loss_recovery` (NebulaOS-klipper-extensions,
// extras/nebulaos_power_loss_recovery.py get_status()), which InitPanel already subscribes
// with every other object, so the merged snapshot is in State at
// /printer_state/nebulaos_power_loss_recovery. Factored out of PowerPanel so what the panel
// offers for each backend state is unit-testable on the host (tests/test_plr_status.cpp).
//
// This replaces the Creality-stock path (print_file_name.json + SDCARD_PRINT_FILE
// ISCONTINUEPRINT=1): upstream Klipper ignores ISCONTINUEPRINT, so that "resume" restarted
// the print from the beginning on top of the part.
//
// This file may be distributed under the terms of the GNU GPLv3 license.

struct PlrStatus {
  bool present = false;            // object exists in printer_state
  std::string state;               // NONE, RECORDING, RECOVERY_AVAILABLE, RECOVERY_INCOMPATIBLE,
                                   // RESUME_PREPARED, ERROR (anything else is treated as unknown)
  std::string resume_mode;         // "physical" | "state_restore_only"
  std::string file;                // gcodes-relative path of the interrupted print
  double progress = 0.0;           // 0..1, file position / size
  std::string reason;              // why a record is incompatible
  std::string last_error;
  bool resume_in_progress = false;
};

// Parses one merged snapshot (null/absent => present=false). Wrongly typed fields are
// treated as absent, never thrown.
PlrStatus parse_plr_status(const json &snapshot);

struct PlrView {
  std::string text;                // status line shown in the panel
  bool show_resume = false;        // physical resume offered (always behind a confirmation)
  bool show_discard = false;       // a stored record can be discarded
};

// What the panel shows. `printing` = print_stats is printing/paused. Resume is offered
// only for RECOVERY_AVAILABLE with resume_mode "physical": the state-only restore leaves a
// cold, unhomed printer that cannot be finished safely from the touchscreen.
PlrView plr_view(const PlrStatus &s, bool printing);

// Commands: the public NebulaOS macro wrappers (NebulaOS-firmware overlay
// etc/nebulaos/klipper/calibration.cfg) over the backend's NEBULAOS_PLR_RESUME /
// NEBULAOS_PLR_DISCARD. Resume always carries ALLOW_UNSAFE=1 (position_safe is never
// true: Z is assumed to have held while unpowered) and PHYSICAL=1.
extern const char *const PLR_RESUME_COMMAND;
extern const char *const PLR_DISCARD_COMMAND;

// G80 (Run 22): the popup after a power loss ("Resume?") - asked once per
// record. "" = nothing to ask (no physical resume available, a print running,
// a resume already going); else a key that changes with the record.
std::string plr_offer_key(const PlrStatus &s, bool printing);

// Confirmation text for the resume dialog.
std::string plr_resume_confirm_text(const PlrStatus &s);

// Basename of a gcodes-relative path.
std::string plr_display_name(const std::string &path);

#endif // __PLR_STATUS_H__
