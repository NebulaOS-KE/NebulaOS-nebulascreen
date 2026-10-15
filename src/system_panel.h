#ifndef __SYSTEM_PANEL_H__
#define __SYSTEM_PANEL_H__

#include "button_container.h"
#include "logs_panel.h"
#include "lvgl/lvgl.h"
#include "websocket_client.h"

#include <mutex>
#include <string>

// Settings > System (D9 2026-10-10, S1 2026-10-11): NebulaOS build and boot
// slot, GuppyScreen version, kernel and storage (with a warning when /usr/data
// is read-only or its boot check failed), the screen's log level, the log
// viewer, Restart GUI, Reboot and Shut down (confirmed, refused while
// printing). Data through Moonraker: machine.nebulaos.info, machine.reboot /
// machine.shutdown, machine.services.restart.
class SystemPanel {
 public:
  SystemPanel(KWebSocketClient &ws, std::mutex &lock);
  ~SystemPanel();

  void foreground();
#ifdef SIMULATOR
  void sim_fill();
  void sim_tap_reboot() { confirm_power(true); }
#endif

  static void _handle_callback(lv_event_t *e) {
    ((SystemPanel *)e->user_data)->handle_callback(e);
  }

 private:
  void shown();
  void handle_callback(lv_event_t *e);
  void refresh();
  void show_info(const json &info_reply);
  void confirm_power(bool reboot);
  void restart_gui();
  void report_error(json &reply, const char *what);

  KWebSocketClient &ws;
  std::mutex &lv_lock;

  lv_obj_t *body;   // set by SettingsUi::page() while cont is built
  lv_obj_t *cont;
  lv_obj_t *info_label;
  lv_obj_t *loglevel_dd;
  lv_obj_t *logs_btn;
  lv_obj_t *restart_gui_btn;
  lv_obj_t *reboot_btn;
  lv_obj_t *shutdown_btn;
  ButtonContainer back_btn;
  LogsPanel logs_panel;
};

#endif // __SYSTEM_PANEL_H__
