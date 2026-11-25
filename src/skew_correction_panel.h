#ifndef __SKEW_CORRECTION_PANEL_H__
#define __SKEW_CORRECTION_PANEL_H__

#include "lvgl/lvgl.h"
#include "websocket_client.h"

// Skew correction helper, reached from the Tune tab. Software-compensates a
// frame that isn't perfectly square (parts come out as faint parallelograms).
// The user prints a skew calibration object, measures the three XY diagonals
// (A-C, B-D, A-D) with calipers, types them in, and Apply & Save issues
//   SET_SKEW XY=<ac>,<bd>,<ad>, SKEW_PROFILE SAVE=nebulaos, SAVE_CONFIG
// (#31: stores [skew_correction nebulaos]; calibration.cfg loads it at every
// Klipper start). Clear correction removes the profile before a new
// calibration print. The status line shows whether the profile is loaded.
// Detection is via the config section (matches the Axis Twist pattern); the
// Tune panel shows a toast instead of opening this if it isn't enabled.
class SkewCorrectionPanel {
 public:
  SkewCorrectionPanel(KWebSocketClient &ws);
  ~SkewCorrectionPanel();

  static bool is_enabled();   // [skew_correction] present in printer.cfg

  void foreground();
  // simulator tour: tap field i and type text (keyboard shown)
  void sim_type(int i, const char *text) { show_kb(ta[i]); lv_textarea_set_text(ta[i], text); }

  void handle_callback(lv_event_t *event);
  static void _handle_callback(lv_event_t *e) {
    ((SkewCorrectionPanel*)e->user_data)->handle_callback(e);
  }
  void handle_kb(lv_event_t *event);
  static void _handle_kb(lv_event_t *e) {
    ((SkewCorrectionPanel*)e->user_data)->handle_kb(e);
  }
  void handle_ta(lv_event_t *event);
  static void _handle_ta(lv_event_t *e) {
    ((SkewCorrectionPanel*)e->user_data)->handle_ta(e);
  }

 private:
  void show_kb(lv_obj_t *field);
  void apply_and_save();
  void clear_correction();
  void update_status();

  KWebSocketClient &ws;

  lv_obj_t *panel_cont;
  lv_obj_t *ta[3];        // A-C, B-D, A-D measurement inputs (mm)
  lv_obj_t *body;         // description (hidden while the keypad is shown)
  lv_obj_t *rows[3];      // the A-C, B-D, A-D rows (narrow while the keypad is shown)
  lv_obj_t *kb;           // decimal numeric keyboard (hidden until a field is tapped)
  lv_obj_t *status_label;  // "Skew correction: on/off"
  lv_obj_t *apply_btn;
  lv_obj_t *clear_btn;
  lv_obj_t *back_btn;
};

#endif  // __SKEW_CORRECTION_PANEL_H__
