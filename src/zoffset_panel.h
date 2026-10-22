#ifndef __ZOFFSET_PANEL_H__
#define __ZOFFSET_PANEL_H__

#include "websocket_client.h"
#include "notify_consumer.h"
#include "selector.h"
#include "lvgl/lvgl.h"

#include <mutex>

// First-layer Z-offset / baby-stepping aid. A focused live control reachable
// from the Tune tab and by tapping the Z-offset value on the print-status
// screen. Adjusts the live gcode offset, same as FineTune's Z column, but with a
// big readout, clear closer/farther direction, and fine first-layer steps.
//
// G92 (Run 17): the buttons send NEBULAOS_Z_ADJUST DELTA=/RESET=1. NebulaOS
// (extensions, status object nebulaos_z_adjust) applies the change live, saves
// the adjustment in its own file and re-applies it at every Klipper start, so
// there is no Save button or SAVE_CONFIG/restart. Without that object (older
// extensions) the panel falls back to SET_GCODE_OFFSET and says nothing about
// saving, because nothing is saved then. Logic: src/nebulaos_z_adjust.cpp.
class ZOffsetPanel : public NotifyConsumer {
 public:
  ZOffsetPanel(KWebSocketClient &ws, std::mutex &lock);
  ~ZOffsetPanel();

  void foreground();
  void consume(json &j);
  void handle_callback(lv_event_t *e);
  static void _handle_callback(lv_event_t *e) {
    ((ZOffsetPanel *)e->user_data)->handle_callback(e);
  }

 private:
  double cur_offset();
  void update_value(double z);
  bool saved_adjust();         // nebulaos_z_adjust present
  void update_saved_text();
  void apply_step(bool raise); // raise = nozzle up / farther from bed

  KWebSocketClient &ws;
  lv_obj_t *panel_cont;
  lv_obj_t *value_label;       // big live readout
  lv_obj_t *up_btn;            // raise — farther from bed / less squish
  lv_obj_t *down_btn;          // lower — closer to bed / more squish
  lv_obj_t *reset_btn;
  lv_obj_t *auto_save_hint;    // "Z offset adjustment: <value> mm, saved" (G92)
  lv_obj_t *back_btn;
  Selector step_selector;
};

#endif  // __ZOFFSET_PANEL_H__
