#ifndef __CALIBRATION_MENU_PANEL_H__
#define __CALIBRATION_MENU_PANEL_H__

#include "lvgl/lvgl.h"
#include "websocket_client.h"
#include "guided_calibration_panel.h"
#include "bedmesh_panel.h"
#include "axis_twist_panel.h"
#include "skew_correction_panel.h"
#include "tmc_tune_panel.h"
#include "pid_calibration_panel.h"
#include "nebulaos_calibration_status.h"
#include "notify_consumer.h"

#include <mutex>
#include <vector>

// Entry point for every calibration flow, reached from the Tune tab's single
// "Calibration" button. G105 (Run 19, Tim): "an idiot just does 1-2-3" -
// IN ORDER 1 Bed Tramming (mechanical first), 2 Heaters (PID) (before Auto
// Calibration: its nozzle clean removes the ooze of the PID dwell), 3 Axis
// Twist (before the mesh: Klipper applies its correction to every probe),
// 4 Auto Calibration, 5 Input Shaper, 6 E-Steps, 7 Skew Correction (needs a
// printed part). OTHER: Load Cell Calibration, TMC Autotune, Bed Mesh.
// Every row has a status line from nebulaos_calibration.history (done /
// description / "Redo: <reason>"), see nebulaos_calibration_status.
//
// Bed Mesh / Input Shaper / Axis Twist / Skew / TMC Autotune panels are owned
// by PrinterTunePanel (Bed Mesh doubles as a Tune-tab top-level tile too,
// since viewing it is routine even though calibrating it isn't) and passed in
// by reference here, the same pattern PrinterTunePanel itself already uses
// for FineTunePanel/ZOffsetPanel.
class CalibrationMenuPanel : public NotifyConsumer {
 public:
  CalibrationMenuPanel(KWebSocketClient &, std::mutex &,
                        BedMeshPanel &,
                        AxisTwistPanel &, SkewCorrectionPanel &,
                        TmcTunePanel &);
  ~CalibrationMenuPanel();

  // Detects motor_database.cfg the same way PrinterTunePanel::init does, to
  // enable/init the TMC Autotune row.
  void init(json &j);

  void foreground();
  void consume(json &j);
  void handle_callback(lv_event_t *event);
  static void _handle_callback(lv_event_t *e) { ((CalibrationMenuPanel*)e->user_data)->handle_callback(e); }

  // right-side Up/OK/Down/Back nav column, same idea as macros_panel.cpp's
  // nav_cont: move a highlight through the rows and activate/leave with OK/Back,
  // without needing to land a precise tap on a specific row.
  void handle_nav(lv_event_t *event);
#ifdef SIMULATOR
  // Screenshot tour: open a row (on-screen order) / reach a guided page.
  void sim_tap_row(size_t i) { if (i < rows.size()) activate_row(rows[i]); }
  // Run 19: highlight a row and scroll it into view without animation
  void sim_show_row(int i) { set_highlight(i); lv_obj_scroll_to_view(rows[i], LV_ANIM_OFF); }
  PidCalibrationPanel &sim_pid() { return pid_panel; }
  GuidedCalibrationPanel &sim_guided(CalFlow f) {
    return f == CalFlow::ESTEPS ? esteps_panel : f == CalFlow::LOAD_CELL ? load_cell_panel
      : f == CalFlow::BED_TRAM ? bed_tram_panel : f == CalFlow::INPUT_SHAPER ? input_shaper_panel
      : auto_calibrate_panel;
  }
#endif
  static void _handle_nav(lv_event_t *e) { ((CalibrationMenuPanel*)e->user_data)->handle_nav(e); }

 private:
  void move_highlight(int delta);
  void set_highlight(int idx);
  void activate_row(lv_obj_t *row);
  void refresh_status();

  KWebSocketClient &ws;

  lv_obj_t *panel_cont;
  lv_obj_t *list_cont;
  lv_obj_t *nav_cont;
  lv_obj_t *up_btn;
  lv_obj_t *ok_btn;
  lv_obj_t *down_btn;
  lv_obj_t *back_btn;

  // numbered, in recommended order
  lv_obj_t *bed_tram_row;
  lv_obj_t *pid_row;
  lv_obj_t *axis_twist_row;
  lv_obj_t *wizard_row;
  lv_obj_t *inputshaper_row;
  lv_obj_t *esteps_row;
  lv_obj_t *skew_row;
  // OTHER
  lv_obj_t *load_cell_row;
  lv_obj_t *tmc_tune_row;
  lv_obj_t *bedmesh_row;

  // every row above, in on-screen order, for Up/Down to walk through
  std::vector<lv_obj_t*> rows;
  // G105: each row's status line and its history key (same order as rows)
  std::vector<lv_obj_t*> status_labels;
  std::vector<const CalMenuEntry*> entries;
  int highlight_index;

  // NebulaOS guided flows (D3-D5): the workflows run in Klipper
  // (nebulaos_calibration) and prompt the operator; these panels start them,
  // show their state and offer Cancel / the E-steps measurement.
  GuidedCalibrationPanel auto_calibrate_panel;
  GuidedCalibrationPanel esteps_panel;
  GuidedCalibrationPanel load_cell_panel;
  GuidedCalibrationPanel bed_tram_panel;
  GuidedCalibrationPanel input_shaper_panel;
  PidCalibrationPanel pid_panel;
  BedMeshPanel &bedmesh_panel;
  AxisTwistPanel &axis_twist_panel;
  SkewCorrectionPanel &skew_correction_panel;
  TmcTunePanel &tmc_tune_panel;

  bool tmc_tune_available;
};

#endif  // __CALIBRATION_MENU_PANEL_H__
