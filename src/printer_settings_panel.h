#ifndef __PRINTER_SETTINGS_PANEL_H__
#define __PRINTER_SETTINGS_PANEL_H__

#include "button_container.h"
#include "websocket_client.h"
#include "lvgl/lvgl.h"

#include <mutex>

// Settings > Printer (S1, 2026-10-11): Restart Klipper, Restart MCU (Klipper's
// FIRMWARE_RESTART), and the screen's printer preferences: default extruder
// temperature, ask before an emergency stop, inverted Y/Z jog arrows.
class PrinterSettingsPanel {
 public:
  PrinterSettingsPanel(KWebSocketClient &ws, std::mutex &lock);
  ~PrinterSettingsPanel();

  void foreground();

  static void _handle_callback(lv_event_t *e) {
    ((PrinterSettingsPanel *)e->user_data)->handle_callback(e);
  }

 private:
  void handle_callback(lv_event_t *e);

  KWebSocketClient &ws;
  std::mutex &lv_lock;

  lv_obj_t *body;   // set by SettingsUi::page() while cont is built
  lv_obj_t *cont;
  lv_obj_t *restart_klipper_btn;
  lv_obj_t *restart_mcu_btn;
  lv_obj_t *temp_dd;
  lv_obj_t *estop_sw;
  lv_obj_t *invert_y_sw;
  lv_obj_t *invert_z_sw;
  ButtonContainer back_btn;
};

#endif // __PRINTER_SETTINGS_PANEL_H__
