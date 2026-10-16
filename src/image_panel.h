#ifndef __IMAGE_PANEL_H__
#define __IMAGE_PANEL_H__

#include "button_container.h"
#include "nebulaos_image.h"
#include "websocket_client.h"
#include "lvgl/lvgl.h"

#include <cstdint>
#include <mutex>

// Settings > Updates > System image (on-printer image updates, 2026-10-14).
// Lists the KE .img packages on a USB stick and in Mainsail's
// config/uploads/system-image (machine.nebulaos.image.list) and what each boot slot
// (A/B) holds. Install: NebulaOS checks the package (image.check), the
// confirmation says what it is and which slot it replaces, then NebulaOS
// writes it into the slot that is not running (image.install) while a box
// above everything follows the progress (image.status) until the printer
// restarts into it. Only offered here, never in Mainsail.
class ImagePanel {
 public:
  ImagePanel(KWebSocketClient &ws, std::mutex &lock);
  ~ImagePanel();

  void foreground();
#ifdef SIMULATOR
  void sim_fill();
  void sim_check(const char *reply_json);   // the confirmation for a check reply
  void sim_progress(const char *status_json);
#endif

  static void _handle_callback(lv_event_t *e) {
    ((ImagePanel *)e->user_data)->handle_callback(e);
  }

 private:
  void shown();
  void handle_callback(lv_event_t *e);
  void refresh();
  void show_files(const json &list_reply);
  void check(const ImageFile &f);
  void checked(const json &check_reply);
  void start_busy(const char *title, const std::string &text);
  void poll();
  void show_job(const json &status_reply);
  void stop_busy();

  KWebSocketClient &ws;
  std::mutex &lv_lock;

  lv_obj_t *body;   // set by SettingsUi::page() while cont is built
  lv_obj_t *cont;
  lv_obj_t *slot_a;
  lv_obj_t *slot_b;
  lv_obj_t *list;
  lv_obj_t *refresh_btn;
  ButtonContainer back_btn;

  ImageFiles files;
  lv_obj_t *busy_label = NULL;
  lv_timer_t *poll_timer = NULL;
  uint32_t busy_since = 0;
  bool working = false;     // checking or installing: no second start
};

#endif // __IMAGE_PANEL_H__
