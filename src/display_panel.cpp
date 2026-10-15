#include "display_panel.h"
#include "config.h"
#include "guppyscreen.h"
#include "nebulaos_display.h"
#include "nebulaos_system.h"
#include "settings_ui.h"
#include "theme.h"
#include "touch_beep.h"
#include "utils.h"
#include "spdlog/spdlog.h"

#include <experimental/filesystem>
#include <iterator>

namespace fs = std::experimental::filesystem;

LV_IMG_DECLARE(back);
LV_IMG_DECLARE(display_img);

struct SleepOption {
  const char *label;
  int32_t seconds;
};

static const SleepOption sleep_options[] = {
  {"Never", -1},
  {"30 seconds", 30},
  {"1 minute", 60},
  {"5 minutes", 300},
  {"10 minutes", 600},
  {"30 minutes", 1800},
  {"1 hour", 3600},
};

static const std::vector<std::string> themes = {
  "blue", "red", "green", "purple", "pink", "yellow"
};

static lv_obj_t *right_dropdown(lv_obj_t *row, const char *options) {
  lv_obj_t *dd = lv_dropdown_create(row);
  lv_dropdown_set_options(dd, options);
  lv_obj_set_width(dd, LV_PCT(52));
  lv_obj_align(dd, LV_ALIGN_RIGHT_MID, 0, 0);
  return dd;
}

DisplayPanel::DisplayPanel(KWebSocketClient &c, std::mutex &lock)
  : ws(c)
  , lv_lock(lock)
  , cont(SettingsUi::page("Display", &body))
  , back_btn(cont, &back, "Back", &DisplayPanel::_handle_callback, this)
  , boot_logo_panel(c, lock)
{
  SettingsUi::on_shown(cont, [this]() { shown(); });
  Config *conf = Config::get_instance();
  lv_obj_t *left = SettingsUi::column(body, 50);
  lv_obj_t *right = SettingsUi::column(body, 47);

  // Brightness: levels and the current one come from NebulaOS (refresh).
  brightness_dd = right_dropdown(SettingsUi::row(left, "Brightness"), "-");

  std::string opts;
  for (const auto &o : sleep_options) {
    opts += std::string(o.label) + "\n";
  }
  opts.pop_back();
  sleep_dd = right_dropdown(SettingsUi::row(left, "Sleep after"), opts.c_str());
  auto v = conf->get_json("/display_sleep_sec");
  if (!v.is_null()) {
    for (size_t i = 0; i < std::size(sleep_options); ++i) {
      if (sleep_options[i].seconds == v.template get<int32_t>()) {
        lv_dropdown_set_selected(sleep_dd, i);
      }
    }
  }

  theme_dd = right_dropdown(SettingsUi::row(left, "Theme colour"),
                            "Blue\nRed\nGreen\nPurple\nPink\nYellow");
  v = conf->get_json("/theme");
  if (!v.is_null()) {
    auto it = std::find(themes.begin(), themes.end(), v.template get<std::string>());
    if (it != themes.end()) {
      theme = std::distance(themes.begin(), it);
      lv_dropdown_set_selected(theme_dd, theme);
    }
  }

  lv_obj_t *beep_row = SettingsUi::row(left, "Touch beep");
  beep_sw = lv_switch_create(beep_row);
  lv_obj_align(beep_sw, LV_ALIGN_RIGHT_MID, 0, 0);
  v = conf->get_json("/touch_beep");
  if (!v.is_null() && v.template get<bool>()) {
    lv_obj_add_state(beep_sw, LV_STATE_CHECKED);
  }

  // G58 (2026-10-15): always a boot logo, NebulaOS's by default; the choice
  // is also the power-on logo (the RTOS of the running slot).
  // G61 (2026-10-16): the boot logo has its own page (submenu rule).
  boot_logo_btn = SettingsUi::icon_button(right, &display_img, "Boot logo",
                                          &DisplayPanel::_handle_callback, this);
  power_label = SettingsUi::hint(right, "Power-on logo: ...");

  for (lv_obj_t *w : {brightness_dd, sleep_dd, theme_dd, beep_sw}) {
    lv_obj_add_event_cb(w, &DisplayPanel::_handle_callback, LV_EVENT_VALUE_CHANGED, this);
  }

  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_FLOATING);
  lv_obj_align(back_btn.get_container(), LV_ALIGN_BOTTOM_RIGHT, 0, 0);
}

DisplayPanel::~DisplayPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void DisplayPanel::foreground() {
  lv_obj_move_foreground(cont);
}

// Every time the page comes on top (SettingsUi::on_shown).
void DisplayPanel::shown() {
  refresh_brightness();
  ws.send_jsonrpc(BOOT_DISPLAY_STATUS_METHOD, json::object(), [this](json &status) {
    std::lock_guard<std::mutex> lock(lv_lock);
    lv_label_set_text(power_label, power_on_view(status).text.c_str());
  });
}

void DisplayPanel::report_error(json &reply, const char *what) {
  auto err = jsonrpc_error_message(reply);
  if (err.empty()) {
    return;
  }
  spdlog::warn("display panel: {}: {}", what, err);
  std::lock_guard<std::mutex> lock(lv_lock);
  KUtils::notify_toast(std::string(what) + ": " + err, 5000);
}

// Called on the LVGL thread; replies arrive on the websocket thread.
void DisplayPanel::refresh_brightness() {
  ws.send_jsonrpc(DISPLAY_STATUS_METHOD, json::object(), [this](json &reply) {
    std::lock_guard<std::mutex> lock(lv_lock);
    show_brightness(reply);
  });
}

// LVGL lock held by the caller.
void DisplayPanel::show_brightness(const json &reply) {
  {
    auto status = parse_display_status(reply);
    updating = true;
    if (status.available) {
      brightness_levels = status.levels;
      lv_dropdown_set_options(brightness_dd, brightness_dropdown_options(status).c_str());
      lv_dropdown_set_selected(brightness_dd, brightness_dropdown_index(status));
      lv_obj_clear_state(brightness_dd, LV_STATE_DISABLED);
    } else {
      brightness_levels.clear();
      lv_dropdown_set_options(brightness_dd, "n/a");
      lv_obj_add_state(brightness_dd, LV_STATE_DISABLED);
    }
    updating = false;
  }
}

#ifdef SIMULATOR
void DisplayPanel::sim_fill() {
  show_brightness(json::parse(R"({"result":{"levels":[25,50,75,100],
    "config":{"brightness":100},"backlight":{"available":true}}})"));
}
#endif

void DisplayPanel::handle_callback(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *obj = lv_event_get_current_target(e);

  if (code == LV_EVENT_CLICKED && obj == back_btn.get_container()) {
    lv_obj_move_background(cont);
    return;
  }
  if (code == LV_EVENT_CLICKED && obj == boot_logo_btn) {
    boot_logo_panel.foreground();
    return;
  }
  if (code != LV_EVENT_VALUE_CHANGED || updating) {
    return;
  }
  Config *conf = Config::get_instance();
  if (obj == brightness_dd) {
    // NebulaOS applies and persists the level (and restores it at boot).
    auto idx = lv_dropdown_get_selected(brightness_dd);
    if (idx < brightness_levels.size()) {
      json params = {{"level", brightness_levels[idx]}};
      ws.send_jsonrpc(DISPLAY_BRIGHTNESS_METHOD, params, [this](json &reply) {
        report_error(reply, "Brightness not changed");
      });
    }
  } else if (obj == sleep_dd) {
    auto idx = lv_dropdown_get_selected(sleep_dd);
    if (idx < std::size(sleep_options)) {
      conf->set<int32_t>("/display_sleep_sec", sleep_options[idx].seconds);
      conf->save();
    }
  } else if (obj == theme_dd) {
    auto idx = lv_dropdown_get_selected(theme_dd);
    if (idx != theme && idx < themes.size()) {
      theme = idx;
      conf->set<std::string>("/theme", themes[theme]);
      conf->save();
      auto theme_config = fs::canonical(conf->get_path()).parent_path() / "themes" / (themes[theme] + ".json");
      ThemeConfig::get_instance()->init(theme_config);
      GuppyScreen::refresh_theme();
    }
  } else if (obj == beep_sw) {
    bool enabled = lv_obj_has_state(beep_sw, LV_STATE_CHECKED);
    conf->set<bool>("/touch_beep", enabled);
    conf->save();
    TouchBeep::set_enabled(enabled);
    if (enabled) {
      TouchBeep::beep();  // immediate confirmation that it works
    }
  }
}

