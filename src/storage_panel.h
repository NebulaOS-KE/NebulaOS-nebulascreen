#ifndef __STORAGE_PANEL_H__
#define __STORAGE_PANEL_H__

#include "button_container.h"
#include "lvgl/lvgl.h"
#include "websocket_client.h"

#include <mutex>

// Settings > System > Storage (G66, 2026-10-16): what uses the data drive
// (machine.nebulaos.storage) and "Clean up Creality files": NebulaOS first
// computes what it would free (machine.nebulaos.cleanup.plan), a dialog says
// plainly what happens - the Creality system in the other slot stops working
// - with Creality's timelapse videos as an opt-in checkbox, then
// machine.nebulaos.cleanup.creality (refused while printing).
class StoragePanel {
 public:
  StoragePanel(KWebSocketClient &ws, std::mutex &lock);
  ~StoragePanel();

  void foreground();

  static void _handle_callback(lv_event_t *e) {
    ((StoragePanel *)e->user_data)->handle_callback(e);
  }

 private:
  void shown();
  void handle_callback(lv_event_t *e);
  void start_cleanup();
  void run_cleanup(bool videos);

  KWebSocketClient &ws;
  std::mutex &lv_lock;

  lv_obj_t *body;
  lv_obj_t *cont;
  lv_obj_t *usage_label;
  lv_obj_t *cleanup_btn;
  bool busy = false;
  ButtonContainer back_btn;
};

#endif // __STORAGE_PANEL_H__
