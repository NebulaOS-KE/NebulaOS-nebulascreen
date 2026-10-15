#ifndef __RESET_PANEL_H__
#define __RESET_PANEL_H__

#include "button_container.h"
#include "hv/json.hpp"
#include "websocket_client.h"
#include "lvgl/lvgl.h"

#include <mutex>

// Settings > Reset (S1-S3, 2026-10-11), each behind a confirmation:
//   Reset GUI settings       the image's guppyconfig.json, then Restart GUI
//   Reset touch calibration  clears the coefficients, then Restart GUI
//   Clean NebulaOS reset     machine.nebulaos.system.clean_reset: wipes all
//                            NebulaOS data at the next boot (red)
//   Swap boot slot (A/B)     machine.nebulaos.system.swap_slot: the other slot
//                            starts next (orange); the text says what that
//                            slot holds (machine.nebulaos.image.status)
// The last two are refused while printing and reboot at once.
class ResetPanel {
 public:
  ResetPanel(KWebSocketClient &ws, std::mutex &lock);
  ~ResetPanel();

  void foreground();
#ifdef SIMULATOR
  // Taps tile 0-3 (screenshots of the confirmations).
  void sim_tap(int tile);
  void sim_slots(const char *status_json) { image_status = nlohmann::json::parse(status_json); }
#endif

  static void _handle_callback(lv_event_t *e) {
    ((ResetPanel *)e->user_data)->handle_callback(e);
  }

 private:
  void shown();
  void handle_callback(lv_event_t *e);
  void restart_gui();
  void reset_gui_settings();
  void reset_touch();
  void system_call(const char *method, const char *what);

  KWebSocketClient &ws;
  std::mutex &lv_lock;

  lv_obj_t *cont;
  ButtonContainer gui_btn;
  ButtonContainer touch_btn;
  ButtonContainer clean_btn;
  ButtonContainer swap_btn;
  ButtonContainer back_btn;
  // machine.nebulaos.image.status: what the other slot holds, for the swap text.
  nlohmann::json image_status;
};

#endif // __RESET_PANEL_H__
