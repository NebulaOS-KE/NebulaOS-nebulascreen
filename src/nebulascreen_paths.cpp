#include "nebulascreen_paths.h"

#include <string>

namespace nebulascreen {

namespace {

struct LegacyValue {
  const char *key;
  const char *old_value;
  const char *new_value;
};

const LegacyValue kLegacyValues[] = {
  {"log_path", "/opt/printer_data/logs/guppyscreen.log", kLogPath},
  {"thumbnail_path", "/opt/guppyscreen/thumbnails", kThumbnailPath},
  {"config_path", "/opt/guppyscreen/guppyconfig.json", kConfigPath},
  {kInitScriptKey, "/etc/init.d/S58guppyscreen", kInitScript},
};

}  // namespace

bool normalize_legacy_config(nlohmann::json &config) {
  if (!config.is_object()) {
    return false;
  }
  bool changed = false;
  if (config.contains("guppy_init_script")) {
    if (!config.contains(kInitScriptKey)) {
      config[kInitScriptKey] = config["guppy_init_script"];
    }
    config.erase("guppy_init_script");
    changed = true;
  }
  for (const auto &lv : kLegacyValues) {
    if (config.contains(lv.key) && config[lv.key].is_string()
        && config[lv.key].get<std::string>() == lv.old_value) {
      config[lv.key] = lv.new_value;
      changed = true;
    }
  }
  return changed;
}

std::string ui_api_problem(const nlohmann::json &version_info) {
  if (!version_info.is_object() || !version_info.contains("nebulaos_ui_api")) {
    return "";
  }
  const auto &level = version_info["nebulaos_ui_api"];
  if (!level.is_number_integer()) {
    return "the image states an unreadable nebulaos_ui_api";
  }
  if (level.get<int>() != kRequiredUiApi) {
    return "this NebulaScreen needs image contract " + std::to_string(kRequiredUiApi)
      + ", the image provides " + std::to_string(level.get<int>());
  }
  return "";
}

const char *version() {
#if defined(NEBULASCREEN_VERSION)
  return NEBULASCREEN_VERSION;
#elif defined(GUPPYSCREEN_VERSION)
  return GUPPYSCREEN_VERSION;
#else
  return "dev-snapshot";
#endif
}

nlohmann::json report_params() {
  return {{"name", kService}, {"version", version()},
          {"nebulaos_ui_api", kRequiredUiApi}};
}

}  // namespace nebulascreen
