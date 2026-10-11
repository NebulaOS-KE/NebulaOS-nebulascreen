#include "nebulaos_system.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

const char *const SYSTEM_INFO_METHOD = "machine.nebulaos.info";
const char *const BOOT_DISPLAY_STATUS_METHOD = "machine.nebulaos.boot_display.status";
const char *const BOOT_DISPLAY_MODE_METHOD = "machine.nebulaos.boot_display.mode";
const char *const BOOT_DISPLAY_IMAGE_METHOD = "machine.nebulaos.boot_display.image";
const char *const BOOT_DISPLAY_RESET_METHOD = "machine.nebulaos.boot_display.reset_image";
const char *const CLEAN_RESET_METHOD = "machine.nebulaos.system.clean_reset";
const char *const SWAP_SLOT_METHOD = "machine.nebulaos.system.swap_slot";
const char *const ACK_CLEAN_RESET_METHOD = "machine.nebulaos.system.ack_clean_reset";
const char *const AFTER_RESET_TITLE = "NebulaOS was reset";
const char *const AFTER_RESET_TEXT =
  "Everything is back to how NebulaOS ships it, including Wi-Fi.\n\n"
  "Connect to Wi-Fi under Settings > Connections > Wi-Fi networks.";
// Console was removed (D10): NebulaOS reads a saved "console" as logo.
const char *const BOOT_MODE_OPTIONS = "Logo\nBlank";

static const char *const MODES[] = {"logo", "blank"};
static const int MODE_COUNT = 2;

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

std::string boot_mode_at(int index) {
  if (index < 0 || index >= MODE_COUNT) return "";
  return MODES[index];
}

int boot_mode_index(const json &status_reply) {
  std::string mode = str_at(result_of(status_reply), "/config/mode"_json_pointer);
  for (int i = 0; i < MODE_COUNT; ++i) {
    if (mode == MODES[i]) return i;
  }
  return 0;
}

static bool image_name(const std::string &path) {
  std::string lower = path;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
  auto ends = [&lower](const char *suffix) {
    std::string s(suffix);
    return lower.size() > s.size() && lower.compare(lower.size() - s.size(), s.size(), s) == 0;
  };
  return path.find('/') == std::string::npos && (ends(".png") || ends(".bmp"));
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
  std::string out = "Built-in";
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
