#ifndef __NEBULAOS_SYSTEM_H__
#define __NEBULAOS_SYSTEM_H__

#include "hv/json.hpp"

#include <string>
#include <vector>

using json = nlohmann::json;

// Pure, LVGL-free logic for the System, Display and Logs screens (decision D9,
// 2026-10-10; settings layout S1, 2026-10-11): system info and storage from
// machine.nebulaos.info, boot screen mode/image from
// machine.nebulaos.boot_display.status and the config/uploads/boot-logo folder, and log
// files from the logs root (Moonraker server.files.list).
//
// This file may be distributed under the terms of the GNU GPLv3 license.

extern const char *const SYSTEM_INFO_METHOD;          // machine.nebulaos.info
extern const char *const BOOT_DISPLAY_STATUS_METHOD;  // machine.nebulaos.boot_display.status
extern const char *const BOOT_DISPLAY_MODE_METHOD;    // machine.nebulaos.boot_display.mode {"mode"}
extern const char *const BOOT_DISPLAY_IMAGE_METHOD;   // machine.nebulaos.boot_display.image {"filename"}
extern const char *const BOOT_DISPLAY_RESET_METHOD;   // machine.nebulaos.boot_display.reset_image
extern const char *const BOOT_DISPLAY_RESTORE_METHOD; // machine.nebulaos.boot_display.restore_original_logo
extern const char *const BOOT_DISPLAY_IMAGES_METHOD;  // machine.nebulaos.boot_display.images
extern const char *const BOOT_DISPLAY_PREVIEW_METHOD; // machine.nebulaos.boot_display.preview {"filename"}
extern const char *const BOOT_DISPLAY_IMPORT_METHOD;  // machine.nebulaos.boot_display.import_usb {"file"}
extern const char *const BOOT_DISPLAY_DELETE_METHOD;  // machine.nebulaos.boot_display.delete_image {"filename"}
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

// G61 (2026-10-16): the Boot logo page. From boot_display.images: the
// dropdown ("NebulaOS" + every image in the boot logo folder), which entry is
// active, and the images on a USB stick for "Add from USB".
struct UsbImage {
  std::string id;     // "usb/sda1/pics/logo.png"
  std::string name;
};
struct BootLogoList {
  std::vector<std::string> names;   // uploads, in dropdown order after "NebulaOS"
  std::vector<bool> shipped;        // G116: per name - comes with NebulaOS, no Delete
  std::string options;              // "NebulaOS\nlogo.png\n..."
  int active = 0;                   // dropdown index of what is set now
  std::vector<UsbImage> usb;
  bool usb_present = false;
};
BootLogoList boot_logo_list(const json &images_reply);
// Dropdown index of `name` (0 = NebulaOS when not found).
int boot_logo_index(const BootLogoList &l, const std::string &name);
// "A:/run/.../preview-x.png" for lv_img from a preview reply, "" on error.
std::string boot_logo_preview_src(const json &preview_reply);

// G60 (2026-10-16): the general log level. Dropdown order Info, Warning,
// Debug; NebulaOS's names and the screen's own spdlog names for each.
extern const char *const LOGS_STATUS_METHOD;  // machine.nebulaos.logs.status
extern const char *const LOGS_LEVEL_METHOD;   // machine.nebulaos.logs.level {"level"}
extern const char *const LOGS_SAVE_METHOD;    // machine.nebulaos.logs.save
extern const char *const LOG_LEVEL_OPTIONS;   // "Info\nWarning\nDebug"
std::string log_level_at(int index);           // "info" | "warning" | "debug" | ""
std::string log_level_spdlog(const std::string &level);  // "info" | "warn" | "debug"
int log_level_index(const json &status_reply);  // index of result.level, default Info
// After logs.save: the line for the dialog.
std::string logs_save_text(const json &reply, bool *ok);

// G65 (2026-10-16): one-line messages NebulaOS leaves for the screen (info
// "notices", e.g. "Your changes to machine.cfg were replaced ..."), shown
// once each and acknowledged with ACK_NOTICE_METHOD {"id"}.
struct SystemNotice {
  std::string id;
  std::string text;
};
extern const char *const ACK_NOTICE_METHOD;   // machine.nebulaos.system.ack_notice
std::vector<SystemNotice> system_notices(const json &info_reply);

// G69 (2026-10-16): System > Report a problem.
extern const char *const REPORT_ISSUES_URL;   // github.com/NebulaOS-KE/NebulaOS/issues
// "Detailed logging switches back to Info at 14:05 (UTC)." while Debug is
// on (logs.status "debug_until"), else the standing rule.
std::string debug_note(const json &status_reply);

// G66 (2026-10-16): System > Storage and "Clean up Creality files".
extern const char *const STORAGE_METHOD;           // machine.nebulaos.storage
extern const char *const CLEANUP_PLAN_METHOD;      // machine.nebulaos.cleanup.plan {"videos"}
extern const char *const CLEANUP_CREALITY_METHOD;  // machine.nebulaos.cleanup.creality {"videos"}
// "Free 3.7 GB of 6.3 GB\nPrint files  12 MB\n..." from a storage reply.
std::string storage_text(const json &reply);
struct CleanupPlan {
  bool ok = false;
  std::string error;            // why there is no plan
  double bytes = 0;             // what the cleanup frees (videos excluded)
  double videos_bytes = 0;      // Creality's timelapse videos, opt-in
  bool other_is_creality = false;
};
CleanupPlan cleanup_plan(const json &plan_reply);
// The popup text for the plan, with or without the videos.
std::string cleanup_text(const CleanupPlan &p, bool videos);
// After cleanup.creality: the line for the dialog.
std::string cleanup_result_text(const json &reply);

// G34 (2026-10-16): after an MCU flash, what the board runs - from a
// printer.objects.query {"mcu": ["mcu_version"]} reply. Empty when unknown.
std::string mcu_running_version(const json &objects_reply);

// S2: whether a server.database.list reply names `ns`.
bool database_has_namespace(const json &list_reply, const std::string &ns);

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
