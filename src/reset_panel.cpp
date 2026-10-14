#include "reset_panel.h"
#include "config.h"
#include "nebulaos_display.h"
#include "nebulaos_image.h"
#include "nebulaos_system.h"
#include "settings_ui.h"
#include "utils.h"
#include "spdlog/spdlog.h"

#include <fstream>

LV_IMG_DECLARE(back);
LV_IMG_DECLARE(reset_img);
LV_IMG_DECLARE(touch_img);
LV_IMG_DECLARE(delete_img);
LV_IMG_DECLARE(swap_img);

// NebulaOS keeps the shipped guppyconfig.json here (the live one is bind-mounted
// over /opt/guppyscreen/guppyconfig.json); see NebulaOS-firmware post-build.
static const char *const GUPPY_DEFAULT_CONFIG =
  "/usr/share/nebulaos/guppyscreen/guppyconfig.default.json";

ResetPanel::ResetPanel(KWebSocketClient &c, std::mutex &lock)
  : ws(c)
  , lv_lock(lock)
  , cont(lv_obj_create(lv_scr_act()))
  , gui_btn(cont, &reset_img, "Reset GUI\nsettings", &ResetPanel::_handle_callback, this)
  , touch_btn(cont, &touch_img, "Reset touch\ncalibration", &ResetPanel::_handle_callback, this)
  , clean_btn(cont, &delete_img, "Clean\nNebulaOS reset", &ResetPanel::_handle_callback, this)
  , swap_btn(cont, &swap_img, "Swap boot\nslot (A/B)", &ResetPanel::_handle_callback, this)
  , back_btn(cont, &back, "Back", &ResetPanel::_handle_callback, this)
{
  lv_obj_move_background(cont);
  lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *title = lv_label_create(cont);
  lv_label_set_text(title, "Reset");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
  lv_obj_add_flag(title, LV_OBJ_FLAG_FLOATING);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, -6);

  static lv_coord_t rows[] = {LV_GRID_FR(1), LV_GRID_FR(5), LV_GRID_FR(5), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  static lv_coord_t cols[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(cont, cols, rows);
  lv_obj_set_grid_cell(gui_btn.get_container(), LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
  lv_obj_set_grid_cell(touch_btn.get_container(), LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
  lv_obj_set_grid_cell(clean_btn.get_container(), LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 2, 1);
  lv_obj_set_grid_cell(swap_btn.get_container(), LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 2, 1);
  lv_obj_set_grid_cell(back_btn.get_container(), LV_GRID_ALIGN_END, 2, 1, LV_GRID_ALIGN_END, 2, 2);

  // The two that change the whole system stand out: red and orange frames.
  lv_obj_set_style_border_color(clean_btn.get_container(), lv_color_hex(0xC62828), 0);
  lv_obj_set_style_border_color(swap_btn.get_container(), lv_color_hex(0xE65100), 0);
}

ResetPanel::~ResetPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void ResetPanel::foreground() {
  lv_obj_move_foreground(cont);
  ws.send_jsonrpc(IMAGE_STATUS_METHOD, json::object(), [this](json &reply) {
    std::lock_guard<std::mutex> lock(lv_lock);
    image_status = reply;
  });
}

void ResetPanel::restart_gui() {
  json params = {{"service", "guppyscreen"}};
  ws.send_jsonrpc("machine.services.restart", params, [this](json &reply) {
    auto err = jsonrpc_error_message(reply);
    if (err.empty()) {
      return;
    }
    spdlog::warn("GuppyScreen restart failed: {}", err);
    std::lock_guard<std::mutex> lock(lv_lock);
    KUtils::notify_toast("Saved. Restart failed (" + err + ") - reboot to apply.", 6000);
  });
}

void ResetPanel::reset_gui_settings() {
  // The config is a bind-mounted persistent file on NebulaOS: overwrite it in
  // place with the image's read-only default (never delete it).
  std::ifstream src(GUPPY_DEFAULT_CONFIG, std::ios::binary);
  if (!src) {
    KUtils::notify_toast("No default settings found on this system.", 4000);
    return;
  }
  Config *conf = Config::get_instance();
  {
    std::ofstream dst(conf->get_path(), std::ios::binary | std::ios::trunc);
    dst << src.rdbuf();
    if (!dst) {
      KUtils::notify_toast("Could not write the GUI settings.", 4000);
      return;
    }
  }
  spdlog::warn("GuppyScreen config reset to {}.", GUPPY_DEFAULT_CONFIG);
  restart_gui();
}

void ResetPanel::reset_touch() {
  spdlog::warn("Touch calibration reset requested.");
  Config *conf = Config::get_instance();
  conf->set<bool>("/touch_calibrated", true);
  conf->set<json>("/touch_calibration_coeff", json());
  conf->save();
  restart_gui();
}

// Clean reset and slot swap: NebulaOS arms the next boot and reboots; it
// undoes the change itself when the reboot cannot be started.
void ResetPanel::system_call(const char *method, const char *what) {
  if (KUtils::is_printing()) {
    KUtils::notify_locked();
    return;
  }
  ws.send_jsonrpc(method, json::object(), [this, what](json &reply) {
    auto err = jsonrpc_error_message(reply);
    if (err.empty()) {
      return;
    }
    spdlog::warn("{} failed: {}", what, err);
    std::lock_guard<std::mutex> lock(lv_lock);
    KUtils::notify_toast(std::string(what) + " not done: " + err, 6000);
  });
  KUtils::notify_toast("Restarting...", 4000);
}

void ResetPanel::handle_callback(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
    return;
  }
  lv_obj_t *obj = lv_event_get_current_target(e);
  using SettingsUi::Tone;
  if (obj == back_btn.get_container()) {
    lv_obj_move_background(cont);
  } else if (obj == gui_btn.get_container()) {
    SettingsUi::confirm("Reset GUI settings",
      "Restores the screen's own settings (theme, sleep, sensors shown, ...) to "
      "how NebulaOS ships them, then restarts the GUI. Klipper keeps running.",
      "Reset", Tone::Normal, [this]() { reset_gui_settings(); });
  } else if (obj == touch_btn.get_container()) {
    SettingsUi::confirm("Reset touch calibration",
      "Clears the saved touch calibration. The GUI restarts and asks you to "
      "calibrate again.",
      "Reset", Tone::Normal, [this]() { reset_touch(); });
  } else if (obj == clean_btn.get_container()) {
    if (KUtils::is_printing()) {
      KUtils::notify_locked();
      return;
    }
    SettingsUi::confirm("Clean NebulaOS reset",
      "Erases ALL NebulaOS data and restarts as a new install: printer "
      "configuration and calibration, Wi-Fi networks, network settings, root "
      "password, print history and screen settings.\n"
      "Your G-code files and the other boot slot (A/B) are not touched.\n"
      "Afterwards, connect to Wi-Fi again on this screen.",
      "Erase & restart", Tone::Danger,
      [this]() { system_call(CLEAN_RESET_METHOD, "Clean reset"); });
  } else if (obj == swap_btn.get_container()) {
    if (KUtils::is_printing()) {
      KUtils::notify_locked();
      return;
    }
    SettingsUi::confirm("Swap boot slot (A/B)", swap_confirm_text(image_status).c_str(),
      "Swap & restart", Tone::Warning,
      [this]() { system_call(SWAP_SLOT_METHOD, "Swap boot slot"); });
  }
}

#ifdef SIMULATOR
void ResetPanel::sim_tap(int tile) {
  ButtonContainer *tiles[] = {&gui_btn, &touch_btn, &clean_btn, &swap_btn};
  if (tile >= 0 && tile < 4) {
    lv_event_send(tiles[tile]->get_container(), LV_EVENT_CLICKED, NULL);
  }
}
#endif
