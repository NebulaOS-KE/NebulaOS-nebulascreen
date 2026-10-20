#ifndef __GUIDED_CALIBRATION_PANEL_H__
#define __GUIDED_CALIBRATION_PANEL_H__

#include "button_container.h"
#include "lvgl/lvgl.h"
#include "nebulaos_calibration_flow.h"
#include "websocket_client.h"

#include <mutex>
#include <string>

#include "notify_consumer.h"

// One NebulaOS guided calibration (E-steps, Z-offset & bed mesh, input shaper -
// decisions D3-D5). The workflow runs in Klipper and talks to the operator
// through action:prompt dialogs (PromptPanel). This panel starts it, shows its
// live state, offers Cancel, and takes the E-steps measurement. What it shows
// for each backend state is decided in nebulaos_calibration_flow (host-tested).
class GuidedCalibrationPanel : public NotifyConsumer {
 public:
  GuidedCalibrationPanel(KWebSocketClient &ws, std::mutex &lock, CalFlow flow);
  ~GuidedCalibrationPanel();

  void foreground();
  void consume(json &j);

  static void _handle_callback(lv_event_t *e) {
    ((GuidedCalibrationPanel *)e->user_data)->handle_callback(e);
  }
  static void _handle_kb(lv_event_t *e) {
    ((GuidedCalibrationPanel *)e->user_data)->handle_kb(e);
  }

 private:
  void refresh();
  void handle_callback(lv_event_t *e);
  void handle_kb(lv_event_t *e);
  void send(const std::string &gcode, const char *what);
  void confirm_start();
  void submit_measurement();

  KWebSocketClient &ws;
  CalFlow flow;

  lv_obj_t *cont;
  lv_obj_t *title;
  lv_obj_t *text;
  lv_obj_t *measure_row;
  lv_obj_t *measure_ta;
  lv_obj_t *measure_btn;
  lv_obj_t *btn_row;
  lv_obj_t *start_btn;
  lv_obj_t *cancel_btn;
  // G89/G90: the current step's own buttons (CalView::actions)
  static constexpr int kMaxActions = 2;
  lv_obj_t *action_btns[kMaxActions];
  std::string action_cmds[kMaxActions];
  // G90: a page flow (no popups on the screen) comes to the front when it
  // starts from elsewhere (the guard popup, Mainsail).
  bool was_active = false;
  lv_obj_t *kb;
  ButtonContainer back_btn;
};

#endif // __GUIDED_CALIBRATION_PANEL_H__
