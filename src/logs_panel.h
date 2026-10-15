#ifndef __LOGS_PANEL_H__
#define __LOGS_PANEL_H__

#include "button_container.h"
#include "lvgl/lvgl.h"
#include "websocket_client.h"

#include <mutex>
#include <string>
#include <vector>

// Read-only log viewer (decision D9): lists the files in Moonraker's "logs"
// root and shows the end of the selected one. The file is read from the local
// logs directory, so viewing works on the printer's own screen. A "Wi-Fi"
// entry first shows only the Wi-Fi and DHCP lines of system.log (G59).
class LogsPanel {
 public:
  LogsPanel(KWebSocketClient &ws, std::mutex &lock);
  ~LogsPanel();

  void foreground();

  static void _handle_callback(lv_event_t *e) {
    ((LogsPanel *)e->user_data)->handle_callback(e);
  }

 private:
  void handle_callback(lv_event_t *e);
  void show_file(const std::string &name);
  void show_wifi();

  KWebSocketClient &ws;
  std::mutex &lv_lock;

  lv_obj_t *cont;
  lv_obj_t *list;
  lv_obj_t *text_cont;
  lv_obj_t *text;
  ButtonContainer back_btn;
  std::vector<std::string> files;
};

#endif // __LOGS_PANEL_H__
