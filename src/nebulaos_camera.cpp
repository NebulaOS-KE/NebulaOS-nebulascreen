#include "nebulaos_camera.h"

const char *const CAMERA_STATUS_METHOD = "machine.nebulaos.camera.status";
const char *const CAMERA_SETTINGS_METHOD = "machine.nebulaos.camera.settings";
const char *const CAMERA_DEFAULTS_METHOD = "machine.nebulaos.camera.defaults";
const char *const CAMERA_RESET_METHOD = "machine.nebulaos.camera.reset";
const char *const CAMERA_VERSION_METHOD = "machine.nebulaos.camera.version";

const char *const CAMERA_IR_OPTIONS = "Auto\nAlways colour\nAlways black-and-white";
const char *const CAMERA_FLICKER_OPTIONS = "Off\n50 Hz\n60 Hz";

const std::vector<std::string> &camera_ir_values() {
  static const std::vector<std::string> v = {"auto", "color", "bw"};
  return v;
}

const std::vector<std::string> &camera_flicker_values() {
  static const std::vector<std::string> v = {"off", "50", "60"};
  return v;
}

int camera_index_of(const std::vector<std::string> &values, const std::string &v) {
  for (size_t i = 0; i < values.size(); ++i) {
    if (values[i] == v) return (int)i;
  }
  return 0;
}

const std::vector<CameraSlider> &camera_sliders() {
  static const std::vector<CameraSlider> s = {
    {"brightness", "Brightness", 50, 160}, {"contrast", "Contrast", 50, 160},
    {"saturation", "Saturation", 50, 160}, {"sharpness", "Sharpness", 50, 160},
    {"zoom", "Zoom", 100, 200}, {"wb_temperature", "Colour temp.", 300, 600},
    {"exposure_time", "Exposure", 1, 1000}};
  return s;
}

static const json &result_of(const json &reply) {
  static const json empty = json::object();
  if (reply.is_object() && reply.contains("result") && reply["result"].is_object()) {
    return reply["result"];
  }
  return empty;
}

static std::string error_of(const json &reply) {
  if (!reply.is_object() || !reply.contains("error")) return "";
  const json &e = reply["error"];
  if (e.is_object() && e.contains("message") && e["message"].is_string()) {
    return e["message"].get<std::string>();
  }
  return e.dump();
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

CameraView camera_view(const json &reply) {
  CameraView v;
  const json &r = result_of(reply);
  if (!r.contains("config") || !r["config"].is_object()) return v;
  v.ok = true;
  v.config = r["config"];
  v.defaults = r.contains("defaults") && r["defaults"].is_object() ? r["defaults"] : json::object();
  v.present = r.contains("present") && r["present"].is_boolean() && r["present"].get<bool>();
  if (r.contains("firmware") && r["firmware"].is_string()) v.firmware = r["firmware"].get<std::string>();
  return v;
}

int camera_int(const json &c, const char *key, int fallback) {
  if (c.is_object() && c.contains(key) && c[key].is_number_integer()) return c[key].get<int>();
  return fallback;
}

bool camera_bool(const json &c, const char *key, bool fallback) {
  if (c.is_object() && c.contains(key) && c[key].is_boolean()) return c[key].get<bool>();
  return fallback;
}

std::string camera_str(const json &c, const char *key, const std::string &fallback) {
  if (c.is_object() && c.contains(key) && c[key].is_string()) return c[key].get<std::string>();
  return fallback;
}

json camera_settings_params(const json &changes) {
  return {{"settings", changes}};
}

std::string camera_result_text(const json &reply) {
  std::string err = error_of(reply);
  if (!err.empty()) {
    if (err.rfind("EXPOSURE_ONE_WAY", 0) == 0) {
      return "Automatic exposure comes back only with Restart camera.";
    }
    if (err.rfind("CAMERA_ABSENT", 0) == 0) return "No Nebula Camera is connected.";
    return after_code(err);
  }
  const json &r = result_of(reply);
  std::string res = r.contains("result") && r["result"].is_string() ? r["result"].get<std::string>() : "";
  if (res == "SAVED_APPLIES_LATER" || res == "RESET_APPLIES_LATER") {
    return "Saved - applied when the camera is connected.";
  }
  if (res == "RESET") return "Camera restarted.";
  return "";
}

std::string camera_version_of(const json &reply) {
  const json &r = result_of(reply);
  return r.contains("firmware") && r["firmware"].is_string() ? r["firmware"].get<std::string>() : "";
}
