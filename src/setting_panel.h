#ifndef __SETTING_PANEL_H__
#define __SETTING_PANEL_H__

#include "button_container.h"
#include "connections_panel.h"
#include "display_panel.h"
#include "printer_settings_panel.h"
#include "reset_panel.h"
#include "spoolman_panel.h"
#include "system_panel.h"
#include "updates_panel.h"
#include "websocket_client.h"
#include "lvgl/lvgl.h"

#include <mutex>

#ifdef SIMULATOR
#include <functional>
#include <string>
#include <utility>
#include <vector>
// Screenshot tours (GUPPY_SIM_SHOTS): named steps, one screenshot each.
using SimSteps = std::vector<std::pair<std::string, std::function<void()>>>;
void sim_run_tour(const std::string &dir, SimSteps steps, std::function<void()> done);
#endif

// Settings home (S1, 2026-10-11): six tiles, each opening one page -
// Connections, Display, Printer, System, Updates, Reset.
class SettingPanel {
 public:
  SettingPanel(KWebSocketClient &c, std::mutex &l, lv_obj_t *parent, SpoolmanPanel &sm);
  ~SettingPanel();

  lv_obj_t *get_container();
  void enable_spoolman();

  void handle_callback(lv_event_t *event);

  static void _handle_callback(lv_event_t *event) {
    SettingPanel *panel = (SettingPanel*)event->user_data;
    panel->handle_callback(event);
  };

#ifdef SIMULATOR
  // Screenshot tour of every settings page (GUPPY_SIM_SHOTS=<dir>); exits
  // the simulator when done. `home` is what to raise for the home screenshot.
  void sim_tour(const char *dir, lv_obj_t *home);
#endif

 private:
  KWebSocketClient &ws;
  std::mutex &lv_lock;
  lv_obj_t *cont;

  ConnectionsPanel connections_panel;
  DisplayPanel display_panel;
  PrinterSettingsPanel printer_panel;
  SystemPanel system_panel;
  UpdatesPanel updates_panel;
  ResetPanel reset_panel;

  ButtonContainer connections_btn;
  ButtonContainer display_btn;
  ButtonContainer printer_btn;
  ButtonContainer system_btn;
  ButtonContainer updates_btn;
  ButtonContainer reset_btn;
};

#endif // __SETTING_PANEL_H__
