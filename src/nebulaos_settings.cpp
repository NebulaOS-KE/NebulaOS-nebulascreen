#include "nebulaos_settings.h"

#include <algorithm>

const char *const SERVER_CONFIG_METHOD = "server.config";
const char *const WIFI_STATUS_METHOD = "machine.nebulaos.wifi.status";
const char *const WIFI_SCAN_METHOD = "machine.nebulaos.wifi.scan";
const char *const WIFI_CONNECT_METHOD = "machine.nebulaos.wifi.connect";
const char *const WIFI_FORGET_METHOD = "machine.nebulaos.wifi.forget";
const char *const UPDATE_STATUS_METHOD = "machine.update.status";
const char *const UPDATE_REFRESH_METHOD = "machine.update.refresh";
const char *const UPDATE_UPGRADE_METHOD = "machine.update.upgrade";
const char *const HOSTNAME_METHOD = "machine.nebulaos.network.hostname";
const char *const SERVICE_RESTART_METHOD = "machine.services.restart";
const char *const MCU_STATUS_METHOD = "machine.nebulaos.mcu.status";
const char *const MCU_FILES_METHOD = "machine.nebulaos.mcu.files";
const char *const MCU_FLASH_METHOD = "machine.nebulaos.mcu.flash";

static const json &result_of(const json &reply) {
  static const json empty = json::object();
  if (reply.is_object() && reply.contains("result") && reply["result"].is_object()) {
    return reply["result"];
  }
  return empty;
}

static std::string str_at(const json &j, const json::json_pointer &p) {
  if (j.contains(p) && j[p].is_string()) {
    return j[p].template get<std::string>();
  }
  return "";
}

static bool bool_at(const json &j, const json::json_pointer &p, bool fallback) {
  if (j.contains(p) && j[p].is_boolean()) {
    return j[p].template get<bool>();
  }
  return fallback;
}

// -- Connections ------------------------------------------------------------------

ConnectionsView connections_view(const json &network_status_reply, const json &server_config_reply) {
  ConnectionsView v;
  const json &net = result_of(network_status_reply);
  std::string ssid = str_at(net, "/live/ssid"_json_pointer);
  std::string ip = str_at(net, "/live/address"_json_pointer);
  std::string mode = str_at(net, "/config/ipv4/mode"_json_pointer);
  std::string host = str_at(net, "/config/hostname"_json_pointer);
  if (host.empty()) host = str_at(net, "/live/hostname"_json_pointer);

  v.wifi = ssid.empty() ? "not connected" : ssid;
  v.address = ip.empty() ? "-" : ip + (mode == "static" ? " (static)" : " (DHCP)");
  v.name = host.empty() ? "-" : host + ".local";
  v.hostname = host;

  const json &cfg = result_of(server_config_reply);
  int port = 7125;
  auto port_ptr = "/config/server/port"_json_pointer;
  if (cfg.contains(port_ptr) && cfg[port_ptr].is_number_integer()) {
    port = cfg[port_ptr].template get<int>();
  }
  if (!ip.empty()) {
    v.mainsail = "http://" + ip;
    v.moonraker = ip + ":" + std::to_string(port);
  }
  v.spoolman = str_at(cfg, "/config/spoolman/server"_json_pointer);
  return v;
}

// -- Wi-Fi --------------------------------------------------------------------------

std::vector<WifiNetwork> wifi_networks(const json &scan_reply) {
  std::vector<WifiNetwork> out;
  const json &r = result_of(scan_reply);
  if (!r.contains("networks") || !r["networks"].is_array()) {
    return out;
  }
  for (const auto &n : r["networks"]) {
    if (!n.is_object() || !n.contains("ssid") || !n["ssid"].is_string()) continue;
    WifiNetwork w;
    w.ssid = n["ssid"].template get<std::string>();
    if (w.ssid.empty()) continue;
    if (n.contains("signal") && n["signal"].is_number_integer()) {
      w.signal = n["signal"].template get<int>();
    }
    w.security = n.contains("security") && n["security"].is_string()
      ? n["security"].template get<std::string>() : "psk";
    w.saved = bool_at(n, "/saved"_json_pointer, false);
    w.current = bool_at(n, "/current"_json_pointer, false);
    out.push_back(w);
  }
  return out;
}

WifiCurrent wifi_current(const json &reply) {
  WifiCurrent c;
  const json &r = result_of(reply);
  c.connected = str_at(r, "/status/state"_json_pointer) == "COMPLETED";
  if (c.connected) {
    c.ssid = str_at(r, "/status/ssid"_json_pointer);
    c.address = str_at(r, "/status/address"_json_pointer);
  }
  return c;
}

std::vector<std::string> wifi_saved(const json &status_reply) {
  std::vector<std::string> out;
  const json &r = result_of(status_reply);
  if (!r.contains("saved") || !r["saved"].is_array()) return out;
  for (const auto &n : r["saved"]) {
    if (n.is_object() && n.contains("ssid") && n["ssid"].is_string()) {
      out.push_back(n["ssid"].template get<std::string>());
    }
  }
  return out;
}

std::string wifi_signal_text(int dbm) {
  if (dbm >= -50) return "Excellent";
  if (dbm >= -60) return "Good";
  if (dbm >= -70) return "Fair";
  return "Weak";
}

bool wifi_supported(const WifiNetwork &n) {
  return n.security == "open" || n.security == "psk";
}

bool wifi_needs_password(const WifiNetwork &n) {
  return n.security != "open" && !n.saved;
}

std::string wifi_error_text(const json &reply) {
  if (!reply.is_object() || !reply.contains("error")) return "";
  std::string msg;
  const json &e = reply["error"];
  if (e.is_object() && e.contains("message") && e["message"].is_string()) {
    msg = e["message"].template get<std::string>();
  } else {
    msg = e.dump();
  }
  auto starts = [&msg](const char *code) { return msg.rfind(code, 0) == 0; };
  if (starts("WRONG_PASSWORD")) return "Wrong password. The previous network is connected again.";
  if (starts("CONNECT_TIMEOUT")) return "Could not connect. The previous network is connected again.";
  if (starts("PASSWORD_REQUIRED")) return "This network needs a password.";
  if (starts("INVALID_PASSWORD")) return "A Wi-Fi password has 8 to 63 characters.";
  if (starts("UNSUPPORTED_SECURITY")) return "The printer cannot join this type of network.";
  if (starts("WIFI_UNAVAILABLE")) return "Wi-Fi is not available right now.";
  if (starts("NOT_SAVED")) return "This network is not saved.";
  return msg;
}

json wifi_connect_params(const std::string &ssid, const std::string &password) {
  return {{"ssid", ssid}, {"password", password}};
}

std::string wifi_connected_text(const json &reply, const std::string &ssid, bool *ok) {
  const json &r = result_of(reply);
  std::string ip = str_at(r, "/address"_json_pointer);
  std::string note = str_at(r, "/address_note"_json_pointer);
  bool good = bool_at(r, "/address_ok"_json_pointer, true);
  if (ok != nullptr) *ok = good;
  // The network name on its own line: long names wrap cleanly (G9).
  std::string text = "Connected to\n" + ssid + "\n";
  if (!good) {
    return text + (note.empty() ? std::string("The address does not work on this network.") : note);
  }
  return text + "IP: " + (ip.empty() ? std::string("waiting for an address") : ip);
}

// -- Updates ------------------------------------------------------------------------

std::string update_label(const std::string &name) {
  if (name == "klipper") return "Klipper";
  if (name == "moonraker") return "Moonraker";
  if (name == "mainsail") return "Mainsail";
  if (name == "fluidd") return "Fluidd";
  if (name == "crowsnest") return "Crowsnest";
  if (name == "nebulaos-klipper-extensions" || name == "nebulaos_klipper_extensions") {
    return "NebulaOS extensions";
  }
  return name;
}

std::vector<UpdateRow> update_rows(const json &status_reply) {
  std::vector<UpdateRow> rows;
  const json &r = result_of(status_reply);
  if (!r.contains("version_info") || !r["version_info"].is_object()) return rows;
  for (auto it = r["version_info"].begin(); it != r["version_info"].end(); ++it) {
    if (it.key() == "system" || !it.value().is_object()) continue;
    const json &v = it.value();
    UpdateRow row;
    row.name = it.key();
    row.label = update_label(row.name);
    row.version = str_at(v, "/version"_json_pointer);
    row.remote = str_at(v, "/remote_version"_json_pointer);
    bool corrupt = bool_at(v, "/corrupt"_json_pointer, false);
    bool valid = bool_at(v, "/is_valid"_json_pointer, true) && !corrupt;
    row.valid = valid;
    int behind = 0;
    auto cbh = "/commits_behind_count"_json_pointer;
    if (v.contains(cbh) && v[cbh].is_number_integer()) {
      behind = v[cbh].template get<int>();
    } else if (v.contains("commits_behind") && v["commits_behind"].is_array()) {
      behind = (int)v["commits_behind"].size();
    }
    bool remote_known = !row.remote.empty() && row.remote != "?";
    row.available = valid && (behind > 0 || (remote_known && row.remote != row.version));
    if (!valid) {
      row.state = corrupt ? "repository damaged" : "needs attention (see Mainsail)";
    } else if (behind > 0) {
      row.state = std::to_string(behind) + " new";
    } else if (row.available) {
      row.state = row.remote + " available";
    } else {
      row.state = "up to date";
    }
    rows.push_back(row);
  }
  std::sort(rows.begin(), rows.end(), [](const UpdateRow &a, const UpdateRow &b) {
    return a.label < b.label;
  });
  return rows;
}

UpdateProgress update_progress(const json &notification) {
  UpdateProgress p;
  auto ptr = "/params/0"_json_pointer;
  if (!notification.is_object() || !notification.contains(ptr) || !notification[ptr].is_object()) {
    return p;
  }
  const json &n = notification[ptr];
  std::string app = str_at(n, "/application"_json_pointer);
  std::string msg = str_at(n, "/message"_json_pointer);
  p.complete = bool_at(n, "/complete"_json_pointer, false);
  // Moonraker sends multi-line git output; the last non-empty line is enough.
  auto end = msg.find_last_not_of(" \n\r");
  msg = end == std::string::npos ? "" : msg.substr(0, end + 1);
  auto nl = msg.rfind('\n');
  if (nl != std::string::npos) msg = msg.substr(nl + 1);
  if (msg.size() > 90) msg = msg.substr(0, 87) + "...";
  p.valid = !msg.empty() || p.complete;
  p.text = (app.empty() ? "" : update_label(app) + ": ") + (msg.empty() ? "done" : msg);
  return p;
}

bool updates_busy(const json &status_reply) {
  return bool_at(result_of(status_reply), "/busy"_json_pointer, false);
}

// -- shared -------------------------------------------------------------------------

// "" when the reply has no error; else Moonraker's message ("CODE: text").
static std::string error_of(const json &reply) {
  if (!reply.is_object() || !reply.contains("error")) return "";
  const json &e = reply["error"];
  if (e.is_object() && e.contains("message") && e["message"].is_string()) {
    return e["message"].template get<std::string>();
  }
  return e.dump();
}

static bool starts_with(const std::string &s, const char *prefix) {
  return s.rfind(prefix, 0) == 0;
}

// The text after "CODE: ", or the whole message.
static std::string after_code(const std::string &msg) {
  auto pos = msg.find(": ");
  if (pos == std::string::npos || pos == 0) return msg;
  for (size_t i = 0; i < pos; ++i) {
    char c = msg[i];
    if (!((c >= 'A' && c <= 'Z') || c == '_' || (c >= '0' && c <= '9'))) return msg;
  }
  return msg.substr(pos + 2);
}

// -- Printer name -------------------------------------------------------------------

HostnameCheck hostname_check(const std::string &input, const std::string &current) {
  HostnameCheck c;
  size_t b = input.find_first_not_of(" \t");
  size_t e = input.find_last_not_of(" \t");
  std::string name = b == std::string::npos ? "" : input.substr(b, e - b + 1);
  for (auto &ch : name) {
    if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
  }
  c.name = name;
  if (name.empty()) {
    c.message = "Enter a name.";
    return c;
  }
  if (name.size() > 63) {
    c.message = "At most 63 characters.";
    return c;
  }
  bool digits_only = true;
  for (char ch : name) {
    bool digit = ch >= '0' && ch <= '9';
    if (!((ch >= 'a' && ch <= 'z') || digit || ch == '-')) {
      c.message = "Use only letters a-z, digits 0-9 and '-'.";
      return c;
    }
    if (!digit) digits_only = false;
  }
  if (name.front() == '-' || name.back() == '-') {
    c.message = "The name cannot start or end with '-'.";
    return c;
  }
  if (digits_only) {
    c.message = "The name needs at least one letter.";
    return c;
  }
  if (name == "localhost") {
    c.message = "'localhost' is reserved.";
    return c;
  }
  if (name == current) {
    c.message = "This is the current name.";
    return c;
  }
  c.ok = true;
  c.message = "Reachable as " + name + ".local after saving.";
  return c;
}

json hostname_params(const std::string &name) {
  return {{"hostname", name}};
}

bool hostname_restart_needed(const json &reply) {
  const json &r = result_of(reply);
  return str_at(r, "/result"_json_pointer) == "APPLIED" &&
         bool_at(r, "/mdns_restart_required"_json_pointer, false);
}

json moonraker_restart_params() {
  return {{"service", "moonraker"}};
}

std::string hostname_error_text(const json &reply) {
  std::string msg = error_of(reply);
  if (msg.empty()) return "";
  if (starts_with(msg, "INVALID_HOSTNAME")) return "The printer refused the name: " + after_code(msg);
  if (starts_with(msg, "HOSTNAME_APPLY_FAILED")) return "Could not apply the name. The old name is kept.";
  return after_code(msg);
}

// -- MCU firmware -------------------------------------------------------------------

std::string human_size(long bytes) {
  if (bytes < 1024) return std::to_string(bytes) + " B";
  long tenths = (bytes * 10 + 512) / 1024;
  return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10) + " KB";
}

static McuFile mcu_file(const json &f) {
  McuFile m;
  m.id = str_at(f, "/id"_json_pointer);
  m.name = str_at(f, "/name"_json_pointer);
  m.source = str_at(f, "/source"_json_pointer);
  m.sha256 = str_at(f, "/sha256"_json_pointer);
  m.kind = str_at(f, "/kind"_json_pointer);
  m.problem = str_at(f, "/problem"_json_pointer);
  if (f.contains("size") && f["size"].is_number_integer()) m.size = f["size"].template get<long>();
  m.valid = bool_at(f, "/valid"_json_pointer, false) && m.sha256.size() == 64;
  m.native = bool_at(f, "/native"_json_pointer, false);
  m.klipper_version = str_at(f, "/klipper_version"_json_pointer);
  m.version_note = str_at(f, "/version_note"_json_pointer);
  std::string where = m.source == "usb" ? "USB" : (m.source == "upload" ? "Uploaded" : "Built in");
  std::string state;
  if (!m.valid) {
    state = m.problem.empty() ? "cannot be installed" : m.problem;
  } else if (m.native) {
    state = "NebulaOS firmware";
  } else {
    state = m.kind == "raw" ? "ready (raw build, packed on install)" : "ready";
  }
  m.detail = where + "  -  " + human_size(m.size) + "  -  " + state;
  if (m.valid && !m.klipper_version.empty()) {
    m.detail += "  -  Klipper " + m.klipper_version;
  }
  if (m.valid && !m.version_note.empty()) {
    m.detail += "\n" + m.version_note;
  }
  m.warn = !m.valid || !m.version_note.empty();
  return m;
}

McuFiles mcu_files(const json &files_reply) {
  McuFiles out;
  const json &r = result_of(files_reply);
  if (r.contains("files") && r["files"].is_array()) {
    for (const auto &f : r["files"]) {
      if (f.is_object() && f.contains("id") && f["id"].is_string()) out.files.push_back(mcu_file(f));
    }
  }
  if (r.contains("builtin") && r["builtin"].is_object()) {
    out.builtin = mcu_file(r["builtin"]);
    out.has_builtin = out.builtin.valid;
  }
  out.usb_present = r.contains("usb") && r["usb"].is_array() && !r["usb"].empty();
  return out;
}

McuStatusView mcu_status_view(const json &status_reply) {
  McuStatusView v;
  const json &r = result_of(status_reply);
  std::string cls = str_at(r, "/guard/application_class"_json_pointer);
  std::string ident = str_at(r, "/guard/application_identity"_json_pointer);
  if (!error_of(status_reply).empty() || r.empty()) {
    v.running = "unknown (no answer)";
  } else if (!r.contains("guard") || r["guard"].is_null()) {
    v.running = "not checked yet this boot";
  } else if (cls == "NATIVE_CANDIDATE_001") {
    v.running = "NebulaOS firmware";
  } else if (cls == "KNOWN_STOCK") {
    v.running = "Creality firmware";
  } else if (cls == "UNKNOWN_APPLICATION") {
    v.running = "custom firmware" + (ident.empty() || ident == "unknown" ? "" : " (" + ident + ")");
  } else {
    v.running = "unknown";
  }
  bool managed = bool_at(r, "/managed"_json_pointer, true);
  v.ownership = managed ? "NebulaOS (auto-restore on)" : "you (custom firmware)";
  if (r.contains("last_flash") && r["last_flash"].is_object()) {
    const json &l = r["last_flash"];
    std::string file = str_at(l, "/file"_json_pointer);
    auto slash = file.rfind('/');
    if (slash != std::string::npos) file = file.substr(slash + 1);
    bool ok = bool_at(l, "/ok"_json_pointer, false);
    v.last = file + (ok ? ", installed" : ", failed: " + str_at(l, "/error"_json_pointer));
  }
  return v;
}

json mcu_flash_params(const McuFile &f) {
  return {{"file", f.id}, {"sha256", f.sha256}};
}

std::string mcu_confirm_text(const McuFile &f) {
  std::string what = f.native ? "the NebulaOS firmware" : f.name;
  std::string text = "Install " + what + " on the printer's mainboard?\n\n"
    "Klipper stops for about a minute. Do not switch the printer off.";
  if (!f.native) {
    if (!f.klipper_version.empty()) {
      text += "\n\nBuilt from Klipper " + f.klipper_version + ".";
    }
    if (!f.version_note.empty()) {
      text += "\nWarning: " + f.version_note + ".";
    } else {
      text += " Firmware built from another Klipper version leaves Klipper unable to "
              "connect - then reinstall the NebulaOS firmware here.";
    }
  }
  return text;
}

McuFlashResult mcu_flash_result(const json &reply) {
  McuFlashResult res;
  std::string msg = error_of(reply);
  if (msg.empty()) {
    const json &r = result_of(reply);
    res.ok = str_at(r, "/result"_json_pointer) == "FLASHED";
    if (res.ok) {
      res.title = "Firmware installed";
      res.text = bool_at(r, "/native"_json_pointer, false)
        ? "The NebulaOS firmware is installed and Klipper is restarting."
        : "The firmware is installed and Klipper is restarting. If Klipper reports an "
          "MCU error, reinstall the NebulaOS firmware here.";
      return res;
    }
    res.title = "Install failed";
    res.text = "Unexpected answer from the printer.";
    return res;
  }
  res.title = "Install failed";
  if (starts_with(msg, "PRINTING") || starts_with(msg, "REFUSED")) {
    res.title = "Not installed";
    res.text = "Not possible while a print is active. Nothing was changed.";
  } else if (starts_with(msg, "BUSY")) {
    res.title = "Not installed";
    res.text = "Another firmware install is still running.";
  } else if (starts_with(msg, "FILE_CHANGED") || starts_with(msg, "FILE_NOT_FOUND")) {
    res.title = "Not installed";
    res.text = "The file changed or is gone (USB stick removed?). Nothing was changed.";
  } else if (starts_with(msg, "INVALID_FIRMWARE") || starts_with(msg, "INVALID_FILE")) {
    res.title = "Not installed";
    res.text = "This file cannot be installed: " + after_code(msg) + ". Nothing was changed.";
  } else if (starts_with(msg, "TIMEOUT")) {
    res.title = "No answer";
    res.text = after_code(msg) + ".";
  } else {
    res.text = after_code(msg) + "\n\nKlipper was restarted. If it cannot reach the "
      "mainboard, reinstall the NebulaOS firmware here.";
  }
  return res;
}
