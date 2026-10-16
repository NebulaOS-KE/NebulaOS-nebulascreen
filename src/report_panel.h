#ifndef __REPORT_PANEL_H__
#define __REPORT_PANEL_H__

#include "button_container.h"
#include "lvgl/lvgl.h"
#include "websocket_client.h"

#include <mutex>

// Settings > System > Report a problem (G69, 2026-10-16): the three steps of a
// useful bug report on one page - 1. the log level (Debug switches itself
// back to Info after 24 hours), 2. Save logs to USB, 3. a QR code and the
// address of the NebulaOS issue tracker. The log level used to sit on the
// System page and "Save logs to USB" on the Logs page (G60).
class ReportPanel {
 public:
  ReportPanel(KWebSocketClient &ws, std::mutex &lock);
  ~ReportPanel();

  void foreground();

  static void _handle_callback(lv_event_t *e) {
    ((ReportPanel *)e->user_data)->handle_callback(e);
  }

 private:
  void shown();
  void handle_callback(lv_event_t *e);
  void set_level();
  void save_logs();

  KWebSocketClient &ws;
  std::mutex &lv_lock;

  lv_obj_t *body;
  lv_obj_t *cont;
  lv_obj_t *loglevel_dd;
  lv_obj_t *debug_note_label;
  lv_obj_t *save_btn;
  bool updating_level = false;
  bool saving = false;
  ButtonContainer back_btn;
};

#endif // __REPORT_PANEL_H__
