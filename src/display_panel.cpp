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
  image_dd = right_dropdown(SettingsUi::row(right, "Boot logo"), "NebulaOS");
  power_label = SettingsUi::hint(right, "Power-on logo: ...");
  lv_obj_t *btns = lv_obj_create(right);
  lv_obj_set_size(btns, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(btns, 0, 0);
  lv_obj_set_style_pad_column(btns, 4, 0);
  lv_obj_set_style_border_width(btns, 0, 0);
  lv_obj_set_style_bg_opa(btns, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(btns, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(btns, LV_FLEX_FLOW_ROW);
  auto small_btn = [&](const char *text) {
    lv_obj_t *b = lv_btn_create(btns);
    lv_obj_set_height(b, 30);
    lv_obj_set_flex_grow(b, 1);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, text);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_10, 0);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, &DisplayPanel::_handle_callback, LV_EVENT_CLICKED, this);
    lv_obj_add_state(b, LV_STATE_DISABLED);
    return b;
  };
  set_nebulaos_btn = small_btn("Set NebulaOS logo");
  restore_btn = small_btn("Restore Creality logo");
  SettingsUi::hint(right, "Upload PNG or BMP images in Mainsail (folder nebulaos_boot). "
                          "Shown from the next start.");

  for (lv_obj_t *w : {brightness_dd, sleep_dd, theme_dd, beep_sw, image_dd}) {
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
  refresh_boot_display();
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

void DisplayPanel::refresh_boot_display() {
  json params = {{"root", "nebulaos_boot"}};
  ws.send_jsonrpc("server.files.list", params, [this](json &files_reply) {
    auto imgs = boot_images(files_reply);
    ws.send_jsonrpc(BOOT_DISPLAY_STATUS_METHOD, json::object(), [this, imgs](json &status) {
      std::lock_guard<std::mutex> lock(lv_lock);
      show_boot_display(imgs, status);
    });
  });
}

// LVGL lock held by the caller.
void DisplayPanel::show_boot_display(const std::vector<std::string> &imgs, const json &status) {
  images = imgs;
  updating = true;
  lv_dropdown_set_options(image_dd, boot_image_options(images).c_str());
  lv_dropdown_set_selected(image_dd, boot_image_index(status, images));
  auto p = power_on_view(status);
  lv_label_set_text(power_label, p.text.c_str());
  if (p.can_set_nebulaos) lv_obj_clear_state(set_nebulaos_btn, LV_STATE_DISABLED);
  else lv_obj_add_state(set_nebulaos_btn, LV_STATE_DISABLED);
  if (p.can_restore) lv_obj_clear_state(restore_btn, LV_STATE_DISABLED);
  else lv_obj_add_state(restore_btn, LV_STATE_DISABLED);
  updating = false;
}

#ifdef SIMULATOR
void DisplayPanel::sim_fill() {
  show_brightness(json::parse(R"({"result":{"levels":[25,50,75,100],
    "config":{"brightness":100},"backlight":{"available":true}}})"));
  show_boot_display({"nebula-dark.png"}, json::parse(R"({"result":{"config":{"mode":"logo",
    "image":"builtin","schema":1}}})"));
}
#endif

void DisplayPanel::handle_callback(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *obj = lv_event_get_current_target(e);

  if (code == LV_EVENT_CLICKED && obj == back_btn.get_container()) {
    lv_obj_move_background(cont);
    return;
  }
  if (code == LV_EVENT_CLICKED && obj == set_nebulaos_btn) {
    SettingsUi::confirm("Set NebulaOS logo",
      "The NebulaOS logo becomes the boot screen and the power-on logo of this boot slot "
      "(written into its RTOS). Shown from the next start.",
      "Set", SettingsUi::Tone::Warning, [this]() {
        boot_logo_action(BOOT_DISPLAY_RESET_METHOD, json::object(), "Boot logo not changed");
      });
    return;
  }
  if (code == LV_EVENT_CLICKED && obj == restore_btn) {
    SettingsUi::confirm("Restore Creality logo",
      "The original power-on logo goes back into this boot slot's RTOS. The boot screen "
      "after it stays as it is. Shown from the next start.",
      "Restore", SettingsUi::Tone::Warning, [this]() {
        boot_logo_action(BOOT_DISPLAY_RESTORE_METHOD, json::object(), "Power-on logo not changed");
      });
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
  } else if (obj == image_dd) {
    uint16_t idx = lv_dropdown_get_selected(image_dd);
    // Back to what is set until confirmed (confirm() has no cancel hook).
    refresh_boot_display();
    if (idx == 0) {
      SettingsUi::confirm("Boot logo: NebulaOS",
        "The NebulaOS logo becomes the boot screen and the power-on logo of this boot slot "
        "(written into its RTOS). Shown from the next start.",
        "Use", SettingsUi::Tone::Warning, [this]() {
          boot_logo_action(BOOT_DISPLAY_RESET_METHOD, json::object(), "Boot logo not changed");
        });
    } else if ((size_t)(idx - 1) < images.size()) {
      std::string name = images[idx - 1];
      SettingsUi::confirm(("Boot logo: " + name).c_str(),
        "This image becomes the boot screen and the power-on logo of this boot slot "
        "(converted and written into its RTOS). Shown from the next start.",
        "Use", SettingsUi::Tone::Warning, [this, name]() {
          boot_logo_action(BOOT_DISPLAY_IMAGE_METHOD, json{{"filename", name}},
                           "Boot logo not changed");
        });
    }
  }
}

// LVGL thread; the reply arrives on the websocket thread.
void DisplayPanel::boot_logo_action(const char *method, const json &params, const char *what) {
  ws.send_jsonrpc(method, params, [this, what](json &reply) {
    report_error(reply, what);
    std::string msg = jsonrpc_error_message(reply).empty() ? power_on_result(reply) : "";
    {
      std::lock_guard<std::mutex> lock(lv_lock);
      if (!msg.empty()) {
        KUtils::notify_toast(msg, 8000);
      }
      refresh_boot_display();
    }
  });
}
