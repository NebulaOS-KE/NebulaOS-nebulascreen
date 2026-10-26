#ifndef __GUIDED_CALIBRATION_PANEL_H__
#define __GUIDED_CALIBRATION_PANEL_H__

#include "button_container.h"
#include "lvgl/lvgl.h"
#include "nebulaos_calibration_flow.h"
#include "nebulaos_calibration_status.h"
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
#ifdef SIMULATOR
  void sim_keypad() { show_keypad(true); }
  // presses keypad keys like a finger would ('K' = the OK key)
  void sim_type(const char *keys);
  void sim_weigh() { lv_event_send(measure_btn, LV_EVENT_CLICKED, NULL); }
  void sim_auto_checking(bool on) { auto_checking = on; refresh(); }
  std::string sim_input() const { return lv_textarea_get_text(measure_ta); }
  void sim_open_graph(int i) { open_graph(i); }
  void sim_close_graph() { close_graph(); }
  void sim_sync_switch() {
    if (graph_sw == nullptr) return;
    if (graphs_wanted()) lv_obj_add_state(graph_sw, LV_STATE_CHECKED);
    else lv_obj_clear_state(graph_sw, LV_STATE_CHECKED);
  }
#endif

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
  void commit_input();
  void select_mode(bool automatic);
  void run_auto_check();
  void show_keypad(bool on);
  // G109 (Run 20): Input Shaper graphs
  bool graphs_wanted() const;
  void show_graphs(const CalView &v);
  void open_graph(int i);
  void close_graph();

  KWebSocketClient &ws;
  CalFlow flow;

  lv_obj_t *cont;
  lv_obj_t *title;
  // G89/G90 (Run 17): the step text and the entry row scroll inside `body`;
  // btn_row stays pinned at the bottom, so Next/Cancel/Done never leave the
  // screen, and the keypad opens on the right half, never over the field.
  lv_obj_t *body;
  // G99 (Run 18): Load Cell only - Automatic / Manual (hidden with older
  // extensions and while the bottle flow runs)
  lv_obj_t *mode_sel = nullptr;
  std::string pending_mode;    // tapped, not yet reported back ("auto"/"manual")
  bool auto_checking = false;  // NEBULAOS_LOAD_CELL_AUTO_CHECK sent, no answer yet
  // G104 (Run 19): step list / end screen. Left: "Step 7 of 10", the step
  // in large type, its live detail; right: every step with check marks.
  lv_obj_t *prog_box = nullptr;
  lv_obj_t *prog_counter = nullptr;
  lv_obj_t *prog_head = nullptr;
  lv_obj_t *prog_detail = nullptr;
  lv_obj_t *steps_col = nullptr;
  static constexpr int kMaxSteps = 12;
  lv_obj_t *step_lbls[kMaxSteps] = {};
  // refreshes the page while Klipper restarts (the reconnect is not always
  // announced by a status notification for this object)
  lv_timer_t *restart_timer = nullptr;
  void render_feedback(const CalFeedback &f);
  lv_obj_t *text;
  std::string shown_text;      // to scroll back to the top on a new step
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
  // G109 (Run 20), Input Shaper only: the "Show graphs" switch (remembered in
  // the screen config, /input_shaper_graphs, default on; hidden during a run),
  // a thumbnail per measured axis (tap = full screen, tap again = close).
  lv_obj_t *graph_sw_row = nullptr;
  lv_obj_t *graph_sw = nullptr;
  lv_obj_t *graph_row = nullptr;
  static constexpr int kMaxGraphs = 2;
  static constexpr lv_coord_t kGraphW = 480, kGraphH = 272;   // the extension's screen PNG
  lv_obj_t *graph_imgs[kMaxGraphs] = {};
  std::string graph_src[kMaxGraphs];     // "A:<path>" shown in each slot
  long long graph_stamp[kMaxGraphs] = {}; // mtime+size of the shown file
  lv_obj_t *graph_full = nullptr;        // full-screen view on the top layer
};

#endif // __GUIDED_CALIBRATION_PANEL_H__
