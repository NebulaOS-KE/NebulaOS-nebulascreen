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

}  // namespace nebulascreen
