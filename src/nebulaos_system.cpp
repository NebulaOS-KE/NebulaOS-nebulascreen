#include "nebulaos_system.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

const char *const SYSTEM_INFO_METHOD = "machine.nebulaos.info";
const char *const BOOT_DISPLAY_STATUS_METHOD = "machine.nebulaos.boot_display.status";
const char *const BOOT_DISPLAY_MODE_METHOD = "machine.nebulaos.boot_display.mode";
const char *const BOOT_DISPLAY_IMAGE_METHOD = "machine.nebulaos.boot_display.image";
const char *const BOOT_DISPLAY_RESET_METHOD = "machine.nebulaos.boot_display.reset_image";
const char *const BOOT_DISPLAY_RESTORE_METHOD = "machine.nebulaos.boot_display.restore_original_logo";
const char *const LOGS_STATUS_METHOD = "machine.nebulaos.logs.status";
const char *const LOGS_LEVEL_METHOD = "machine.nebulaos.logs.level";
const char *const LOGS_SAVE_METHOD = "machine.nebulaos.logs.save";
const char *const LOG_LEVEL_OPTIONS = "Info\nWarning\nDebug";
const char *const BOOT_DISPLAY_IMAGES_METHOD = "machine.nebulaos.boot_display.images";
const char *const BOOT_DISPLAY_PREVIEW_METHOD = "machine.nebulaos.boot_display.preview";
const char *const BOOT_DISPLAY_IMPORT_METHOD = "machine.nebulaos.boot_display.import_usb";
const char *const BOOT_DISPLAY_DELETE_METHOD = "machine.nebulaos.boot_display.delete_image";
const char *const CLEAN_RESET_METHOD = "machine.nebulaos.system.clean_reset";
const char *const SWAP_SLOT_METHOD = "machine.nebulaos.system.swap_slot";
const char *const ACK_CLEAN_RESET_METHOD = "machine.nebulaos.system.ack_clean_reset";
const char *const AFTER_RESET_TITLE = "NebulaOS was reset";
const char *const AFTER_RESET_TEXT =
  "Everything is back to how NebulaOS ships it, including Wi-Fi.\n\n"
  "Connect to Wi-Fi under Settings > Connections > Wi-Fi networks.";
// Console was removed (D10): NebulaOS reads a saved "console" as logo.


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

std::string human_bytes(double bytes) {
  char buf[32];
  if (!std::isfinite(bytes) || bytes < 0) bytes = 0;
  if (bytes >= 1e9) {
    std::snprintf(buf, sizeof(buf), "%.1f GB", bytes / 1e9);
  } else {
    std::snprintf(buf, sizeof(buf), "%.0f MB", bytes / 1e6);
  }
  return buf;
}

SystemInfo system_info(const json &reply) {
  SystemInfo s;
  const json &r = result_of(reply);
  if (r.empty()) {
    s.text = "System information unavailable.";
    return s;
  }
  s.ok = true;
  std::string head = str_at(r, "/build/firmware_head"_json_pointer);
  std::string date = str_at(r, "/build/build_date"_json_pointer);
  std::string kernel = str_at(r, "/kernel/release"_json_pointer);
  std::string host = str_at(r, "/hostname"_json_pointer);
  std::string text = "NebulaOS " + (head.size() >= 7 ? head.substr(0, 7) : (head.empty() ? "unknown" : head));
  if (!date.empty()) text += "  (" + date.substr(0, 10) + ")";
  std::string slot = str_at(r, "/slot"_json_pointer);
  if (!slot.empty()) text += ", slot " + slot;
  if (!kernel.empty()) text += "\nKernel " + kernel;
  if (!host.empty()) text += "\nHostname " + host + " (" + host + ".local)";

  const json &st = r.contains("storage") && r["storage"].is_object() ? r["storage"] : json::object();
  double free_b = st.contains("free_bytes") && st["free_bytes"].is_number() ? st["free_bytes"].template get<double>() : -1;
  double total_b = st.contains("total_bytes") && st["total_bytes"].is_number() ? st["total_bytes"].template get<double>() : -1;
  if (free_b >= 0 && total_b > 0) {
    text += "\nStorage " + human_bytes(free_b) + " free of " + human_bytes(total_b);
  }
  bool read_only = st.contains("read_only") && st["read_only"].is_boolean() && st["read_only"].template get<bool>();
  std::string check = str_at(st, "/boot_check/result"_json_pointer);
  if (read_only) {
    s.storage_warning = true;
    text += "\nWARNING: storage is read-only - settings and uploads cannot be saved.";
  } else if (check == "UNREPAIRED" || check == "CHECK_FAILED" || check == "TIMEOUT") {
    s.storage_warning = true;
    text += "\nWARNING: storage check at boot: " + check + ".";
  }
  s.text = text;
  return s;
}

bool after_clean_reset(const json &info_reply) {
  return !str_at(result_of(info_reply), "/after_clean_reset"_json_pointer).empty();
}

static bool image_name(const std::string &path) {
  std::string lower = path;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
  auto ends = [&lower](const char *suffix) {
    std::string s(suffix);
    return lower.size() > s.size() && lower.compare(lower.size() - s.size(), s.size(), s) == 0;
  };
  return path.find('/') == std::string::npos && (ends(".png") || ends(".bmp") ||
                                                  ends(".jpg") || ends(".jpeg"));
}

static std::vector<std::string> paths_of(const json &files_reply) {
  std::vector<std::string> out;
  if (!files_reply.is_object() || !files_reply.contains("result") || !files_reply["result"].is_array()) {
    return out;
  }
  for (const auto &f : files_reply["result"]) {
    if (f.is_object() && f.contains("path") && f["path"].is_string()) {
      out.push_back(f["path"].template get<std::string>());
    }
  }
  std::sort(out.begin(), out.end());
  return out;
}

std::vector<std::string> boot_images(const json &files_reply) {
  std::vector<std::string> out;
  for (const auto &p : paths_of(files_reply)) {
    if (image_name(p)) out.push_back(p);
  }
  return out;
}

std::string boot_image_options(const std::vector<std::string> &images) {
  std::string out = "NebulaOS";
  for (const auto &i : images) out += "\n" + i;
  return out;
}

int boot_image_index(const json &status_reply, const std::vector<std::string> &images) {
  std::string image = str_at(result_of(status_reply), "/config/image"_json_pointer);
  for (size_t i = 0; i < images.size(); ++i) {
    if (images[i] == image) return (int)i + 1;
  }
  return 0;
}

std::vector<std::string> log_files(const json &files_reply) {
  std::vector<std::string> out;
  for (const auto &p : paths_of(files_reply)) {
    if (p.find('/') != std::string::npos) continue;
    auto pos = p.find(".log");
    if (pos == std::string::npos) continue;
    std::string rest = p.substr(pos + 4);
    bool rotated = rest.empty() || (rest[0] == '.' && rest.size() > 1
      && std::all_of(rest.begin() + 1, rest.end(), ::isdigit));
    if (rotated) out.push_back(p);
  }
  return out;
}

std::string tail_text(const std::string &content, size_t max_bytes) {
  if (content.size() <= max_bytes) return content;
  size_t start = content.size() - max_bytes;
  size_t nl = content.find('\n', start);
  if (nl != std::string::npos && nl + 1 < content.size()) start = nl + 1;
  return content.substr(start);
}

std::string wifi_log_lines(const std::string &system_log) {
  std::string out;
  size_t pos = 0;
  while (pos < system_log.size()) {
    size_t nl = system_log.find('\n', pos);
    size_t end = nl == std::string::npos ? system_log.size() : nl;
    std::string line = system_log.substr(pos, end - pos);
    pos = end + 1;
    bool wifi = line.find("wpa_supplicant") != std::string::npos
      || line.find("udhcpc") != std::string::npos
      || (line.find("nebulaos-osconfig") != std::string::npos
          && (line.find(": wifi ") != std::string::npos || line.find(": network ") != std::string::npos));
    if (!wifi) continue;
    std::string lower = line;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower.find("psk") != std::string::npos || lower.find("passw") != std::string::npos) continue;
    out += line + "\n";
  }
  return out;
}

PowerOnView power_on_view(const json &status_reply) {
  PowerOnView v;
  const json &r = result_of(status_reply);
  if (!r.contains("power_on") || !r["power_on"].is_object()) {
    v.text = "Power-on logo: unknown";
    return v;
  }
  const json &p = r["power_on"];
  std::string slot = str_at(p, "/slot"_json_pointer);
  std::string logo = str_at(p, "/logo"_json_pointer);
  bool writable = p.contains("writable") && p["writable"].is_boolean() && p["writable"].get<bool>();
  bool original = p.contains("original_available") && p["original_available"].is_boolean()
                  && p["original_available"].get<bool>();
  std::string what = logo == "nebulaos" ? "NebulaOS" : logo == "original" ? "original"
                   : logo == "custom" ? "your image" : "unknown";
  v.text = "Power-on logo" + (slot.empty() ? std::string("") : " (slot " + slot + ")") + ": " + what;
  if (!writable) {
    std::string why = str_at(p, "/reason"_json_pointer);
    v.text += "\nCannot be changed here" + (why.empty() ? std::string(".") : ": " + why + ".");
    return v;
  }
  v.can_set_nebulaos = logo != "nebulaos";
  v.can_restore = original && logo != "original";
  return v;
}

std::string power_on_result(const json &reply) {
  std::string err;
  if (reply.is_object() && reply.contains("error")) {
    const json &e = reply["error"];
    err = e.is_object() && e.contains("message") && e["message"].is_string()
      ? e["message"].get<std::string>() : e.dump();
  }
  if (!err.empty()) return "Power-on logo not changed: " + err;
  const json &r = result_of(reply);
  const json &p = r.contains("power_on") && r["power_on"].is_object() ? r["power_on"] : r;
  std::string res = str_at(p, "/result"_json_pointer);
  if (res == "WRITTEN" || res == "UNCHANGED") return "";
  std::string msg = str_at(p, "/message"_json_pointer);
  return "The boot screen changed, but the power-on logo did not" +
         (msg.empty() ? std::string(".") : ": " + msg + ".");
}

std::string updates_os_note(const std::string &system_info_text) {
  std::string first = system_info_text.substr(0, system_info_text.find('\n'));
  std::string build, date;
  if (first.rfind("NebulaOS ", 0) == 0) {
    build = first.substr(9, first.find(' ', 9) == std::string::npos ? std::string::npos
                                                                     : first.find(' ', 9) - 9);
    auto open = first.find('('), close = first.find(')');
    if (open != std::string::npos && close != std::string::npos && close > open) {
      date = first.substr(open + 1, close - open - 1);
    }
  }
  std::string text = "This list updates Klipper, Moonraker and Mainsail.\nNebulaOS itself";
  if (!build.empty() && build != "unknown") {
    text += " (build " + build + (date.empty() ? "" : ", " + date) + ")";
  }
  return text + " is updated in System image.";
}

std::string boot_logo_error_text(const std::string &message) {
  // "CODE: text" from the helper; the text is already plain words.
  auto pos = message.find(": ");
  std::string code = pos == std::string::npos ? "" : message.substr(0, pos);
  std::string text = pos == std::string::npos ? message : message.substr(pos + 2);
  if (code == "PRINTING") return "Not possible while a print is active.";
  if (code.rfind("IMAGE_", 0) == 0) {
    return "This image cannot be used: " + text + ".\n\nUse a PNG, BMP or JPEG of at most "
           "4 MB and 2048 x 2048 pixels. Best: 480 x 272, landscape.";
  }
  return text.empty() ? message : text;
}

BootLogoList boot_logo_list(const json &images_reply) {
  BootLogoList l;
  const json &r = result_of(images_reply);
  l.options = "NebulaOS";
  if (r.contains("images") && r["images"].is_array()) {
    for (const auto &i : r["images"]) {
      std::string n = str_at(i, "/name"_json_pointer);
      if (n.empty()) continue;
      l.names.push_back(n);
      l.options += "\n" + n;
    }
  }
  l.active = boot_logo_index(l, str_at(r, "/selected"_json_pointer));
  if (r.contains("usb") && r["usb"].is_array()) {
    for (const auto &u : r["usb"]) {
      UsbImage img{str_at(u, "/id"_json_pointer), str_at(u, "/name"_json_pointer)};
      if (!img.id.empty()) l.usb.push_back(img);
    }
  }
  l.usb_present = r.contains("usb_present") && r["usb_present"].is_boolean() && r["usb_present"].get<bool>();
  return l;
}

int boot_logo_index(const BootLogoList &l, const std::string &name) {
  for (size_t i = 0; i < l.names.size(); ++i) {
    if (!name.empty() && l.names[i] == name) return (int)i + 1;
  }
  return 0;
}

std::string boot_logo_preview_src(const json &preview_reply) {
  std::string path = str_at(result_of(preview_reply), "/path"_json_pointer);
  return path.empty() || path[0] != '/' ? "" : "A:" + path;
}

static const char *const LOG_LEVELS[] = {"info", "warning", "debug"};

std::string log_level_at(int index) {
  return index >= 0 && index < 3 ? LOG_LEVELS[index] : "";
}

std::string log_level_spdlog(const std::string &level) {
  return level == "warning" ? "warn" : level == "debug" ? "debug" : "info";
}

int log_level_index(const json &status_reply) {
  std::string level = str_at(result_of(status_reply), "/level"_json_pointer);
  for (int i = 0; i < 3; ++i) {
    if (level == LOG_LEVELS[i]) return i;
  }
  return 0;
}

std::string logs_save_text(const json &reply, bool *ok) {
  *ok = false;
  if (reply.is_object() && reply.contains("error")) {
    const json &e = reply["error"];
    std::string msg = e.is_object() && e.contains("message") && e["message"].is_string()
      ? e["message"].get<std::string>() : e.dump();
    if (msg.rfind("NO_USB", 0) == 0) return "No USB stick found. Plug one in and try again.";
    if (msg.rfind("USB_READ_ONLY", 0) == 0) return "The USB stick is read-only.";
    auto pos = msg.find(": ");
    return "Logs not saved: " + (pos == std::string::npos ? msg : msg.substr(pos + 2)) + ".";
  }
  const json &r = result_of(reply);
  std::string file = str_at(r, "/file"_json_pointer);
  if (file.empty()) return "Logs not saved: no answer from the printer.";
  *ok = true;
  long bytes = r.contains("bytes") && r["bytes"].is_number() ? r["bytes"].get<long>() : 0;
  return "Saved to the USB stick:\n" + file + "\n(" + human_bytes((double)bytes) +
         "). Send this file when you report a problem.";
}
