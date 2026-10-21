#ifndef __K_CONFIG_H__
#define __K_CONFIG_H__

#include "hv/json.hpp"
#include "spdlog/spdlog.h"

#include <string>

using json = nlohmann::json;

class Config {
 private:
  static Config *instance;
  std::string path;
  // A complete copy written before the config itself is rewritten (save()).
  std::string journal_path;
  void recover_interrupted_save();

 protected:
  json data;
  std::string default_printer;

 public:
  Config();
  Config(Config &o) = delete;
  void operator=(const Config &) = delete;
  void init(std::string config_path, const std::string thumbdir);

  template<typename T> T get(const std::string &json_ptr) {
    return data[json::json_pointer(json_ptr)].template get<T>();
  };

  template<typename T> T set(const std::string &json_ptr, T v) {
    return data[json::json_pointer(json_ptr)] = v;
  };

  json &get_json(const std::string &json_path);

  void save();
  std::string& df();
  std::string get_thumbnail_path();
  std::string get_path();

  static Config *get_instance();

  // Where save() keeps its journal for config_path.
  static std::string journal_for(const std::string &config_path);

};

#endif // __K_CONFIG_H__
