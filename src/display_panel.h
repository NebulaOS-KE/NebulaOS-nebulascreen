#ifndef __DISPLAY_PANEL_H__
#define __DISPLAY_PANEL_H__

#include "button_container.h"
#include "websocket_client.h"
#include "lvgl/lvgl.h"

#include <mutex>
#include <string>
#include <vector>

// Settings > Display (S1, 2026-10-11): brightness (NebulaOS backlight), sleep
// timer, theme colour and touch beep (GuppyScreen settings), and the boot screen
// mode and image (machine.nebulaos.boot_display.*).
class DisplayPanel {
 public:
  DisplayPanel(KWebSocketClient &ws, std::mutex &lock);
  ~DisplayPanel();

  void foreground();
#ifdef SIMULATOR
  void sim_fill();
#endif

  static void _handle_callback(lv_event_t *e) {
    ((DisplayPanel *)e->user_data)->handle_callback(e);
  }

 private:
  void handle_callback(lv_event_t *e);
  void refresh_brightness();
  void refresh_boot_display();
  void show_brightness(const json &display_status_reply);
  void show_boot_display(const std::vector<std::string> &imgs, const json &status_reply);
  void report_error(json &reply, const char *what);

  KWebSocketClient &ws;
  std::mutex &lv_lock;

  lv_obj_t *body;   // set by SettingsUi::page() while cont is built
  lv_obj_t *cont;
  lv_obj_t *brightness_dd;
  lv_obj_t *sleep_dd;
  lv_obj_t *theme_dd;
  lv_obj_t *beep_sw;
  lv_obj_t *mode_dd;
  lv_obj_t *image_dd;
  ButtonContainer back_btn;

  std::vector<int> brightness_levels;
  std::vector<std::string> images;
  uint32_t theme = 0;
  bool updating = false;  // suppress VALUE_CHANGED while filling the dropdowns
};

#endif // __DISPLAY_PANEL_H__
