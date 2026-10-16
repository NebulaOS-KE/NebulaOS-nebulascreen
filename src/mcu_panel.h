#ifndef __MCU_PANEL_H__
#define __MCU_PANEL_H__

#include "button_container.h"
#include "nebulaos_settings.h"
#include "websocket_client.h"
#include "lvgl/lvgl.h"

#include <cstdint>
#include <mutex>
#include <vector>

// Settings > Printer > MCU firmware (run 4, 2026-10-11): "plug in USB, pick a
// compiled .bin, install". Lists the .bin files uploaded through Mainsail
// (uploads/mainboard-firmware) and on a USB stick, each already checked by NebulaOS
// (machine.nebulaos.mcu.files); installs one after a confirmation that names
// the risk (machine.nebulaos.mcu.flash - refused while printing; Klipper is
// stopped and restarted around it); "Reinstall NebulaOS firmware" puts the
// built-in firmware back. While it runs a box above everything says not to
// switch the printer off.
class McuPanel {
 public:
  McuPanel(KWebSocketClient &ws, std::mutex &lock);
  ~McuPanel();

  void foreground();
#ifdef SIMULATOR
  void sim_fill();
  void sim_install(size_t index);       // the confirmation for files[index]
  void sim_busy();
  void sim_result(const char *reply_json);
#endif

  static void _handle_callback(lv_event_t *e) {
    ((McuPanel *)e->user_data)->handle_callback(e);
  }

 private:
  void shown();
  void handle_callback(lv_event_t *e);
  void refresh();
  void show_files(const json &files_reply);
  void show_status(const json &status_reply);
  void install(const McuFile &f);
  void start_busy();
  void tick_busy();
  void finish(const json &reply);

  KWebSocketClient &ws;
  std::mutex &lv_lock;

  lv_obj_t *body;   // set by SettingsUi::page() while cont is built
  lv_obj_t *cont;
  lv_obj_t *running_value;
  lv_obj_t *reported_value;
  lv_obj_t *owner_value;
  lv_obj_t *list;
  lv_obj_t *refresh_btn;
  lv_obj_t *native_btn;
  ButtonContainer back_btn;

  McuFiles files;
  lv_obj_t *busy_label = NULL;
  lv_timer_t *busy_timer = NULL;
  uint32_t busy_since = 0;
  bool flashing = false;
};

#endif // __MCU_PANEL_H__
