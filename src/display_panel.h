#ifndef __DISPLAY_PANEL_H__
#define __DISPLAY_PANEL_H__

#include "boot_logo_panel.h"
#include "camera_panel.h"
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
  CameraPanel &sim_camera() { return camera_panel; }
  void sim_camera_present(bool present) { show_camera_present(present); }
#endif

  static void _handle_callback(lv_event_t *e) {
    ((DisplayPanel *)e->user_data)->handle_callback(e);
  }

 private:
  void shown();
  void handle_callback(lv_event_t *e);
  void refresh_brightness();
  void show_brightness(const json &display_status_reply);
  void show_camera_present(bool present);
  void report_error(json &reply, const char *what);

  KWebSocketClient &ws;
  std::mutex &lv_lock;

  lv_obj_t *body;   // set by SettingsUi::page() while cont is built
  lv_obj_t *cont;
  lv_obj_t *brightness_dd;
  lv_obj_t *sleep_dd;
  lv_obj_t *theme_dd;
  lv_obj_t *beep_sw;
  lv_obj_t *boot_logo_btn;
  lv_obj_t *camera_btn;
  lv_obj_t *power_label;
  ButtonContainer back_btn;
  BootLogoPanel boot_logo_panel;  // G61: its own page
  CameraPanel camera_panel;       // G121: Nebula Camera controls

  std::vector<int> brightness_levels;
  uint32_t theme = 0;
  bool updating = false;  // suppress VALUE_CHANGED while filling the dropdowns
};

#endif // __DISPLAY_PANEL_H__
