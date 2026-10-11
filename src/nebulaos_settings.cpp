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

bool updates_busy(const json &status_reply) {
  return bool_at(result_of(status_reply), "/busy"_json_pointer, false);
}
