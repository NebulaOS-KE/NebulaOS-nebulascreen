#ifndef __PID_CALIBRATION_PANEL_H__
#define __PID_CALIBRATION_PANEL_H__

#include <mutex>
#include <string>

#include "lvgl/lvgl.h"
#include "nebulaos_calibration_status.h"
#include "notify_consumer.h"
#include "websocket_client.h"

#include <mutex>
#include <string>

// G105 (Run 19): Calibration > 2 Heaters (PID). Two buttons: the bed at 65 C,
// the hotend at 230 C (upstream PID_CALIBRATE). After a successful run the
// page records the result for the Calibration menu
// (NEBULAOS_CALIBRATION_RECORD FLOW=pid, newer extensions) and saves with
// SAVE_CONFIG (one Klipper restart). Texts and commands are host-tested in
// nebulaos_calibration_status.
class PidCalibrationPanel : public NotifyConsumer {
 public:
  PidCalibrationPanel(KWebSocketClient &ws, std::mutex &lock);
  ~PidCalibrationPanel();

  void foreground();
  void consume(json &j);
#ifdef SIMULATOR
  void sim_running(PidHeater h) { running = true; heater = h; result.clear(); refresh(); }
  void sim_result(const std::string &r) { running = false; result = r; refresh(); }
#endif

  static void _handle_callback(lv_event_t *e) {
    ((PidCalibrationPanel *)e->user_data)->handle_callback(e);
  }

 private:
  void refresh();
  void handle_callback(lv_event_t *e);
  void start(PidHeater h);

  KWebSocketClient &ws;
  lv_obj_t *cont;
  lv_obj_t *text;
  lv_obj_t *btn_row;
  lv_obj_t *bed_btn;
  lv_obj_t *hotend_btn;
  lv_obj_t *back_btn;
  bool running = false;
  PidHeater heater = PidHeater::BED;
  std::string result;   // the last run's outcome, shown above the description
};

#endif // __PID_CALIBRATION_PANEL_H__
