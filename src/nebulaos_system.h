#ifndef __NEBULAOS_SYSTEM_H__
#define __NEBULAOS_SYSTEM_H__

#include "hv/json.hpp"

#include <string>
#include <vector>

using json = nlohmann::json;

// Pure, LVGL-free logic for the System, Display and Logs screens (decision D9,
// 2026-10-10; settings layout S1, 2026-10-11): system info and storage from
// machine.nebulaos.info, boot screen mode/image from
// machine.nebulaos.boot_display.status and the nebulaos_boot file root, and log
// files from the logs root (Moonraker server.files.list).
//
// This file may be distributed under the terms of the GNU GPLv3 license.

extern const char *const SYSTEM_INFO_METHOD;          // machine.nebulaos.info
extern const char *const BOOT_DISPLAY_STATUS_METHOD;  // machine.nebulaos.boot_display.status
extern const char *const BOOT_DISPLAY_MODE_METHOD;    // machine.nebulaos.boot_display.mode {"mode"}
extern const char *const BOOT_DISPLAY_IMAGE_METHOD;   // machine.nebulaos.boot_display.image {"filename"}
extern const char *const BOOT_DISPLAY_RESET_METHOD;   // machine.nebulaos.boot_display.reset_image
extern const char *const BOOT_DISPLAY_RESTORE_METHOD; // machine.nebulaos.boot_display.restore_original_logo
// Both refuse while printing, reboot at once, and undo themselves when the
// reboot cannot be started (S2/S3).
extern const char *const CLEAN_RESET_METHOD;          // machine.nebulaos.system.clean_reset
extern const char *const SWAP_SLOT_METHOD;            // machine.nebulaos.system.swap_slot
extern const char *const ACK_CLEAN_RESET_METHOD;      // machine.nebulaos.system.ack_clean_reset

struct SystemInfo {
  bool ok = false;
  std::string text;       // multi-line summary for the screen
  bool storage_warning = false;
};

// From a machine.nebulaos.info JSON-RPC reply.
SystemInfo system_info(const json &reply);

// G2: true once after a clean NebulaOS reset (info "after_clean_reset"), until
// the screen acknowledges it with ACK_CLEAN_RESET_METHOD.
bool after_clean_reset(const json &info_reply);
extern const char *const AFTER_RESET_TITLE;
extern const char *const AFTER_RESET_TEXT;

// "12.3 GB" / "512 MB" style.
std::string human_bytes(double bytes);

// Boot logo dropdown (G58: always a logo, no Blank): "NebulaOS" first, then
// uploaded PNG/BMP files (sorted). The choice is the Linux boot screen AND the
// power-on logo the RTOS of the running slot shows.
std::vector<std::string> boot_images(const json &files_reply);
std::string boot_image_options(const std::vector<std::string> &images);
int boot_image_index(const json &status_reply, const std::vector<std::string> &images);

// G58/G40: the power-on logo of the running slot (boot_display.status
// "power_on"): a line for the page and which buttons apply.
struct PowerOnView {
  std::string text;              // "Power-on logo (slot B): NebulaOS"
  bool can_set_nebulaos = false; // the RTOS is known and does not show NebulaOS's logo
  bool can_restore = false;      // a copy of the original exists and it is not shown
};
PowerOnView power_on_view(const json &status_reply);
// After set-image / reset-image / restore: "" when the power-on logo was
// written (or already right), else what to tell the user.
std::string power_on_result(const json &reply);
// A refused boot logo change ("IMAGE_FORMAT: this file is not ...") in
// plain words, with what is accepted.
std::string boot_logo_error_text(const std::string &message);

// Log files from a server.files.list {"root":"logs"} reply: *.log and rotated
// *.log.N, sorted by name.
std::vector<std::string> log_files(const json &files_reply);

// Updates page note under the app list: what the list covers and where
// NebulaOS itself is updated, from system_info()'s text (first line
// "NebulaOS 701a888  (2026-10-15), slot B").
std::string updates_os_note(const std::string &system_info_text);

// The last `max_bytes` of a text, starting at a line boundary.
std::string tail_text(const std::string &content, size_t max_bytes);

// G59: the Wi-Fi lines of system.log - wpa_supplicant, udhcpc (DHCP) and the
// nebulaos-osconfig wifi/network changes. A line naming a psk or password is
// dropped, whatever its source.
std::string wifi_log_lines(const std::string &system_log);

#endif // __NEBULAOS_SYSTEM_H__
