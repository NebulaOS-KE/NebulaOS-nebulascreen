#ifndef __SYSTEM_PANEL_H__
#define __SYSTEM_PANEL_H__

#include "button_container.h"
#include "logs_panel.h"
#include "lvgl/lvgl.h"
#include "websocket_client.h"

#include <mutex>
#include <string>
#include <vector>

// "Power & System" (decision D9): NebulaOS build, kernel and storage (with a
// warning when /usr/data is read-only or its boot check failed), the boot
// screen mode and image, the log viewer, and reboot / shutdown (confirmed,
// refused while printing). All data through Moonraker: machine.nebulaos.*,
// server.files.list, machine.reboot / machine.shutdown.
class SystemPanel {
 public:
  SystemPanel(KWebSocketClient &ws, std::mutex &lock);
  ~SystemPanel();

  void foreground();

  static void _handle_callback(lv_event_t *e) {
    ((SystemPanel *)e->user_data)->handle_callback(e);
  }

 private:
  void handle_callback(lv_event_t *e);
  void refresh();
  void refresh_boot_display();
  void confirm_power(bool reboot);
  void report_error(json &reply, const char *what);

  KWebSocketClient &ws;
  std::mutex &lv_lock;

  lv_obj_t *cont;
  lv_obj_t *left_cont;
  lv_obj_t *right_cont;
  lv_obj_t *info_label;
  lv_obj_t *mode_dd;
  lv_obj_t *image_dd;
  lv_obj_t *logs_btn;
  lv_obj_t *reboot_btn;
  lv_obj_t *shutdown_btn;
  ButtonContainer back_btn;
  LogsPanel logs_panel;

  std::vector<std::string> images;
  bool updating = false;  // suppress VALUE_CHANGED while filling the dropdowns
};

#endif // __SYSTEM_PANEL_H__
