#include "setting_panel.h"
#include "spdlog/spdlog.h"

LV_IMG_DECLARE(network_img);
LV_IMG_DECLARE(display_img);
LV_IMG_DECLARE(print);
LV_IMG_DECLARE(sysinfo_img);
LV_IMG_DECLARE(update_img);
LV_IMG_DECLARE(reset_img);

SettingPanel::SettingPanel(KWebSocketClient &c, std::mutex &l, lv_obj_t *parent, SpoolmanPanel &sm)
  : ws(c)
  , lv_lock(l)
  , cont(lv_obj_create(parent))
  , connections_panel(c, l, sm)
  , display_panel(c, l)
  , printer_panel(c, l)
  , system_panel(c, l)
  , updates_panel(c, l)
  , reset_panel(c, l)
  , connections_btn(cont, &network_img, "Connections", &SettingPanel::_handle_callback, this)
  , display_btn(cont, &display_img, "Display", &SettingPanel::_handle_callback, this)
  , printer_btn(cont, &print, "Printer", &SettingPanel::_handle_callback, this)
  , system_btn(cont, &sysinfo_img, "System", &SettingPanel::_handle_callback, this)
  , updates_btn(cont, &update_img, "Updates", &SettingPanel::_handle_callback, this)
  , reset_btn(cont, &reset_img, "Reset", &SettingPanel::_handle_callback, this)
{
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));

  static lv_coord_t rows[] = {LV_GRID_FR(1), LV_GRID_FR(5), LV_GRID_FR(5), LV_GRID_FR(1),
    LV_GRID_TEMPLATE_LAST};
  static lv_coord_t cols[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
    LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(cont, cols, rows);
  lv_obj_set_style_pad_column(cont, 14, 0);

  ButtonContainer *tiles[] = {&connections_btn, &display_btn, &printer_btn,
                              &system_btn, &updates_btn, &reset_btn};
  for (int i = 0; i < 6; ++i) {
    lv_obj_set_grid_cell(tiles[i]->get_container(), LV_GRID_ALIGN_STRETCH, i % 3, 1,
                         LV_GRID_ALIGN_STRETCH, 1 + i / 3, 1);
  }
}

SettingPanel::~SettingPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

lv_obj_t *SettingPanel::get_container() {
  return cont;
}

void SettingPanel::enable_spoolman() {
  connections_panel.enable_spoolman();
}

void SettingPanel::handle_callback(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
    return;
  }
  lv_obj_t *btn = lv_event_get_current_target(event);
  if (btn == connections_btn.get_container()) {
    connections_panel.foreground();
  } else if (btn == display_btn.get_container()) {
    display_panel.foreground();
  } else if (btn == printer_btn.get_container()) {
    printer_panel.foreground();
  } else if (btn == system_btn.get_container()) {
    system_panel.foreground();
  } else if (btn == updates_btn.get_container()) {
    updates_panel.foreground();
  } else if (btn == reset_btn.get_container()) {
    reset_panel.foreground();
  }
}

#ifdef SIMULATOR
#include "settings_ui.h"
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

void sim_screenshot(const char *path);  // main.cpp

struct SimTour {
  std::string dir;
  std::vector<std::pair<std::string, std::function<void()>>> steps;
  size_t next = 0;
  bool shoot = false;
};

void SettingPanel::sim_tour(const char *dir, lv_obj_t *home) {
  auto *t = new SimTour();
  t->dir = dir;
  auto close_dialogs = []() { SettingsUi::dismiss_confirms(); };
  t->steps = {
    {"01-settings", [home]() { lv_obj_move_foreground(home); }},
    {"02-connections", [this]() { connections_panel.foreground(); connections_panel.sim_fill(); }},
    {"03-wifi", [this]() { connections_panel.sim_wifi(); }},
    {"04-display", [this]() { display_panel.foreground(); display_panel.sim_fill(); }},
    {"05-printer", [this]() { printer_panel.foreground(); }},
    {"06-system", [this]() { system_panel.foreground(); system_panel.sim_fill(); }},
    {"07-updates", [this]() { updates_panel.foreground(); updates_panel.sim_fill(); }},
    {"08-reset", [this]() { reset_panel.foreground(); }},
    {"09-clean-reset-confirm", [this]() { reset_panel.sim_tap(2); }},
    {"10-swap-confirm", [this, close_dialogs]() { close_dialogs(); reset_panel.sim_tap(3); }},
    {"11-reboot-confirm", [this, close_dialogs]() {
      close_dialogs(); system_panel.foreground(); system_panel.sim_fill(); system_panel.sim_tap_reboot(); }},
    // Run 4: printer name and MCU firmware.
    {"12-hostname", [this, close_dialogs]() {
      close_dialogs(); connections_panel.foreground(); connections_panel.sim_fill();
      connections_panel.sim_rename(); connections_panel.sim_hostname().sim_type("ke-workshop"); }},
    {"13-hostname-invalid", [this]() { connections_panel.sim_hostname().sim_type("ke_workshop"); }},
    {"14-hostname-confirm", [this]() {
      connections_panel.sim_hostname().sim_type("ke-workshop"); connections_panel.sim_hostname().sim_save(); }},
    {"15-printer", [this, close_dialogs]() { close_dialogs(); printer_panel.foreground(); }},
    {"16-mcu", [this]() { printer_panel.sim_mcu().foreground(); printer_panel.sim_mcu().sim_fill(); }},
    {"17-mcu-confirm", [this]() { printer_panel.sim_mcu().sim_install(0); }},
    {"18-mcu-busy", [this, close_dialogs]() { close_dialogs(); printer_panel.sim_mcu().sim_busy(); }},
    {"19-mcu-done", [this]() {
      printer_panel.sim_mcu().sim_result(R"({"result":{"result":"FLASHED","native":false}})"); }},
    {"20-mcu-failed", [this, close_dialogs]() {
      close_dialogs(); printer_panel.sim_mcu().sim_result(
        R"({"error":{"code":500,"message":"FLASH_FAILED: Could not enter bootloader: no handshake"}})"); }},
  };
  lv_timer_create([](lv_timer_t *timer) {
    auto *t = (SimTour *)timer->user_data;
    if (t->shoot) {
      std::string path = t->dir + "/" + t->steps[t->next].first + ".ppm";
      sim_screenshot(path.c_str());
      t->shoot = false;
      if (++t->next >= t->steps.size()) {
        exit(0);
      }
      return;
    }
    t->steps[t->next].second();
    t->shoot = true;
  }, 600, t);
}
#endif
