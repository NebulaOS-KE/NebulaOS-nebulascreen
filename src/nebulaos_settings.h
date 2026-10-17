#ifndef __NEBULAOS_SETTINGS_H__
#define __NEBULAOS_SETTINGS_H__

#include "hv/json.hpp"

#include <string>
#include <vector>

using json = nlohmann::json;

// Pure, LVGL-free logic for the Connections, Wi-Fi and Updates screens
// (settings layout S1 and NebulaOS Wi-Fi API Q3, 2026-10-11). Every value comes
// from Moonraker:
//   machine.nebulaos.network.status   address, DHCP/static, hostname
//   server.config                     Moonraker port, Spoolman server
//   machine.nebulaos.wifi.*           status, scan, connect, forget
//   machine.update.*                  app updates (Moonraker's update manager)
//
// This file may be distributed under the terms of the GNU GPLv3 license.

extern const char *const SERVER_CONFIG_METHOD;   // server.config
extern const char *const WIFI_STATUS_METHOD;     // machine.nebulaos.wifi.status
extern const char *const WIFI_SCAN_METHOD;       // machine.nebulaos.wifi.scan
extern const char *const WIFI_CONNECT_METHOD;    // machine.nebulaos.wifi.connect {"ssid","password"}
extern const char *const WIFI_FORGET_METHOD;     // machine.nebulaos.wifi.forget {"ssid"}
extern const char *const UPDATE_STATUS_METHOD;   // machine.update.status
extern const char *const UPDATE_REFRESH_METHOD;  // machine.update.refresh
extern const char *const UPDATE_UPGRADE_METHOD;  // machine.update.upgrade {"name"} (none = all)
extern const char *const HOSTNAME_METHOD;        // machine.nebulaos.network.hostname {"hostname"}
extern const char *const SERVICE_RESTART_METHOD; // machine.services.restart {"service"}
extern const char *const MCU_STATUS_METHOD;      // machine.nebulaos.mcu.status
extern const char *const MCU_FILES_METHOD;       // machine.nebulaos.mcu.files
extern const char *const MCU_FLASH_METHOD;       // machine.nebulaos.mcu.flash {"file","sha256"}

// -- Connections ------------------------------------------------------------------

struct ConnectionsView {
  std::string wifi;       // "Office (connected)" / "not connected"
  std::string address;    // "192.168.0.98 (DHCP)" / "-"
  std::string name;       // "nebulaos.local"
  std::string hostname;   // "nebulaos" ("" when unknown)
  std::string mainsail;   // "http://192.168.0.98" / ""
  std::string moonraker;  // "192.168.0.98:7125" / ""
  std::string spoolman;   // the configured server, "" when not configured
};

ConnectionsView connections_view(const json &network_status_reply, const json &server_config_reply);

// -- Wi-Fi --------------------------------------------------------------------------

struct WifiNetwork {
  std::string ssid;
  int signal = -100;       // dBm
  std::string security;    // "open" | "psk" | "sae" | "wep" | "eap"
  bool saved = false;
  bool current = false;
};

struct WifiCurrent {
  bool connected = false;
  std::string ssid;
  std::string address;
};

// From machine.nebulaos.wifi.scan: strongest first, as NebulaOS sorted them.
std::vector<WifiNetwork> wifi_networks(const json &scan_reply);
// From machine.nebulaos.wifi.status or .scan (both carry "status").
WifiCurrent wifi_current(const json &reply);
// Saved network names from machine.nebulaos.wifi.status.
std::vector<std::string> wifi_saved(const json &status_reply);
// "Excellent" / "Good" / "Fair" / "Weak".
std::string wifi_signal_text(int dbm);
// Whether the printer can join it, and whether it needs a password.
bool wifi_supported(const WifiNetwork &n);
bool wifi_needs_password(const WifiNetwork &n);
// A message for the screen from a failed Wi-Fi call; "" when the call worked.
std::string wifi_error_text(const json &reply);
json wifi_connect_params(const std::string &ssid, const std::string &password,
                         bool hidden = false);
// The status text after a successful connect: the address, or why the address
// does not work on this network (static IP, G5). `ok` is false in that case.
std::string wifi_connected_text(const json &reply, const std::string &ssid, bool *ok);

// -- Updates ------------------------------------------------------------------------

struct UpdateRow {
  std::string name;       // Moonraker updater name ("klipper", "mainsail", ...)
  std::string label;      // for the screen
  std::string version;
  std::string remote;
  bool available = false;
  bool valid = true;      // false: the updater reports a problem
  bool checking = false;  // G56: Moonraker has not read this repository yet
  std::string state;      // "up to date" / "3 new" / "v2.14 available" / problem text
  bool waiting = false;   // G73: the package index cannot install this update yet
};

// From machine.update.status; the OS ("system") updater is left out - NebulaOS
// updates the OS as a whole image.
std::vector<UpdateRow> update_rows(const json &status_reply);
bool updates_busy(const json &status_reply);
// G73: NebulaOS's readiness check (machine.nebulaos.updates.readiness /
// check_readiness): a pending Klipper/Moonraker update whose packages (or
// Klipper's compiled helper) the NebulaOS package index does not have yet is
// marked "waiting for packages". Advisory - the Update button stays.
// Returns the check's message when something waits, else "".
extern const char *const UPDATE_READINESS_METHOD;        // last result
extern const char *const UPDATE_CHECK_READINESS_METHOD;  // check now
std::string apply_readiness(std::vector<UpdateRow> &rows, const json &readiness_reply);
// One notify_update_response notification (params[0] {application, message,
// complete}): the line for the screen and whether the update has finished (G6).
struct UpdateProgress {
  bool valid = false;
  bool complete = false;
  std::string text;
};
UpdateProgress update_progress(const json &notification);
std::string update_label(const std::string &name);

// -- Printer name (run 4) -------------------------------------------------------------
// Mirrors nebulaos_osconfig.network.validate_hostname: 1-63 characters a-z,
// 0-9 and '-', not starting/ending with '-', not all digits, not "localhost".
// Upper case is accepted and lowered, as the helper does.

struct HostnameCheck {
  bool ok = false;        // valid AND different from the current name
  std::string name;       // normalised (trimmed, lower case)
  std::string message;    // for the screen
};

HostnameCheck hostname_check(const std::string &input, const std::string &current);
json hostname_params(const std::string &name);
// After a successful rename: does mDNS need a Moonraker restart?
bool hostname_restart_needed(const json &reply);
json moonraker_restart_params();
// A message for a failed rename or restart; "" when the call worked.
std::string hostname_error_text(const json &reply);

// -- MCU firmware (run 4) ----------------------------------------------------------------
// machine.nebulaos.mcu.* (nebulaos_osconfig/mcu.py).

struct McuFile {
  std::string id;         // "upload/klipper.bin", "usb/sda1/KE.BIN", "builtin/..."
  std::string name;
  std::string source;     // "upload" | "usb" | "builtin"
  std::string sha256;
  long size = 0;
  bool valid = false;
  bool native = false;    // byte-identical to the built-in NebulaOS firmware
  std::string kind;       // "packed" | "raw"
  std::string problem;
  std::string klipper_version;  // from the firmware's data dictionary, "" if unknown
  std::string version_note;     // warning when the Klipper release differs (G32)
  std::string detail;     // one line for the list
  bool warn = false;      // show the detail line in orange
};

struct McuFiles {
  std::vector<McuFile> files;  // uploads, then USB
  bool has_builtin = false;
  McuFile builtin;
  bool usb_present = false;
};

McuFiles mcu_files(const json &files_reply);

struct McuStatusView {
  std::string running;    // "built-in MCU firmware (NebulaOS)" / "custom firmware" / ...
  std::string reported;   // G47: the version the firmware reports about itself
  std::string ownership;  // "Auto-restore on" / "Custom firmware, auto-restore off"
  std::string last;       // "x.bin, installed 2026-10-15, SHA-256 abcd1234..." / ""
};

McuStatusView mcu_status_view(const json &status_reply);
json mcu_flash_params(const McuFile &f);
std::string mcu_confirm_text(const McuFile &f);
// Result for the screen after machine.nebulaos.mcu.flash returned.
struct McuFlashResult {
  bool ok = false;
  std::string title;
  std::string text;
};
McuFlashResult mcu_flash_result(const json &reply);
std::string human_size(long bytes);

#endif // __NEBULAOS_SETTINGS_H__
