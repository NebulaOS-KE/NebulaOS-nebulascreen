#include "guided_calibration_panel.h"
#include "state.h"
#include "utils.h"
#include "spdlog/spdlog.h"

LV_IMG_DECLARE(back);

static lv_obj_t *make_text_btn(lv_obj_t *parent, const char *label, lv_event_cb_t cb, void *user) {
  lv_obj_t *b = lv_btn_create(parent);
  lv_obj_set_height(b, 44);
  lv_obj_set_flex_grow(b, 1);
  lv_obj_t *l = lv_label_create(b);
  lv_label_set_text(l, label);
  lv_obj_center(l);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
  return b;
}

GuidedCalibrationPanel::GuidedCalibrationPanel(KWebSocketClient &c, std::mutex &lock, CalFlow f)
  : NotifyConsumer(lock)
  , ws(c)
  , flow(f)
  , cont(lv_obj_create(lv_scr_act()))
  , title(lv_label_create(cont))
  , text(lv_label_create(cont))
  , measure_row(lv_obj_create(cont))
  , measure_ta(lv_textarea_create(measure_row))
  , measure_btn(nullptr)
  , btn_row(lv_obj_create(cont))
  , start_btn(nullptr)
  , cancel_btn(nullptr)
  , kb(lv_keyboard_create(cont))
  , back_btn(cont, &back, "Back", &GuidedCalibrationPanel::_handle_callback, this)
{
  lv_obj_move_background(cont);
  lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_pad_all(cont, 10, 0);
  lv_obj_set_style_pad_row(cont, 8, 0);
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);

  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);

  lv_obj_set_width(text, LV_PCT(75));
  lv_label_set_long_mode(text, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(text, &lv_font_montserrat_14, 0);

  for (lv_obj_t *row : {measure_row, btn_row}) {
    lv_obj_set_size(row, LV_PCT(75), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  }

  lv_textarea_set_one_line(measure_ta, true);
  lv_textarea_set_placeholder_text(measure_ta, cal_input_hint(flow));
  lv_textarea_set_accepted_chars(measure_ta, "0123456789.");
  lv_textarea_set_max_length(measure_ta, 6);
  lv_obj_set_flex_grow(measure_ta, 2);
  lv_obj_add_event_cb(measure_ta, &GuidedCalibrationPanel::_handle_kb, LV_EVENT_FOCUSED, this);
  measure_btn = make_text_btn(measure_row, LV_SYMBOL_OK " Submit", &GuidedCalibrationPanel::_handle_callback, this);

  start_btn = make_text_btn(btn_row, LV_SYMBOL_PLAY " Start", &GuidedCalibrationPanel::_handle_callback, this);
  cancel_btn = make_text_btn(btn_row, LV_SYMBOL_CLOSE " Cancel", &GuidedCalibrationPanel::_handle_callback, this);

  // Decimal keypad (the shared Numpad is integer-only), same construction as
  // skew_correction_panel.cpp's.
  static const char *kb_map[] = {"1", "2", "3", "\n",
                                 "4", "5", "6", "\n",
                                 "7", "8", "9", "\n",
                                 LV_SYMBOL_BACKSPACE, "0", ".", LV_SYMBOL_OK, NULL};
  static const lv_btnmatrix_ctrl_t kb_ctrl[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_USER_1, kb_map, kb_ctrl);
  lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_USER_1);
  lv_obj_add_flag(kb, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_set_size(kb, LV_PCT(45), LV_PCT(55));
  lv_obj_align(kb, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(kb, &GuidedCalibrationPanel::_handle_kb, LV_EVENT_ALL, this);

  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_FLOATING);
  lv_obj_align(back_btn.get_container(), LV_ALIGN_BOTTOM_RIGHT, 0, -10);

  ws.register_notify_update(this);
}

GuidedCalibrationPanel::~GuidedCalibrationPanel() {
  ws.unregister_notify_update(this);
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void GuidedCalibrationPanel::foreground() {
  refresh();
  lv_obj_move_foreground(cont);
}

void GuidedCalibrationPanel::refresh() {
  auto status = State::get_instance()->get_data("/printer_state/nebulaos_calibration"_json_pointer);
  CalView v = cal_view(flow, status, KUtils::is_printing());
  lv_label_set_text(title, v.title.c_str());
  lv_label_set_text(text, v.text.c_str());
  auto show = [](lv_obj_t *o, bool on) {
    if (on) lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
  };
  show(start_btn, v.can_start);
  show(cancel_btn, v.can_cancel);
  show(measure_row, v.needs_measurement);
  if (!v.needs_measurement) {
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  }
}

void GuidedCalibrationPanel::consume(json &j) {
  // Read-only lookups: operator[] would insert nulls into the shared notification.
  const json &n = j;
  if (!n.contains("/params/0/nebulaos_calibration"_json_pointer)
      && !n.contains("/params/0/print_stats/state"_json_pointer)) {
    return;
  }
  std::lock_guard<std::mutex> lock(lv_lock);
  refresh();
}

// Called from LVGL callbacks; the reply arrives on the websocket thread.
void GuidedCalibrationPanel::send(const std::string &gcode, const char *what) {
  std::string label(what);
  ws.gcode_script(gcode, [this, label](json &reply) {
    if (!reply.contains("error")) {
      return;
    }
    std::string msg = reply["/error/message"_json_pointer].is_string()
      ? reply["/error/message"_json_pointer].template get<std::string>()
      : reply["error"].dump();
    spdlog::warn("guided calibration: {}: {}", label, msg);
    std::lock_guard<std::mutex> lock(lv_lock);
    KUtils::notify_toast(label + ": " + msg, 6000);
    refresh();
  });
}

void GuidedCalibrationPanel::confirm_start() {
  // E-steps and the load cell start with a prompt of their own; nothing moves yet.
  if (flow == CalFlow::ESTEPS || flow == CalFlow::LOAD_CELL) {
    send(cal_start_command(flow), "Start failed");
    return;
  }
  // Z-offset/bed mesh and input shaper home and move the printer right away.
  static const char *btns[] = {"Cancel", "Start", ""};
  const char *msg = flow == CalFlow::AUTO_CALIBRATE
    ? "The printer will home, clean the nozzle, probe and mesh the bed, then restart Klipper.\n\nThe bed must be empty."
    : "The printer will home and shake each axis.\n\nThe accelerometer must be connected and the bed clear.";
  lv_obj_t *mbox = lv_msgbox_create(NULL, NULL, msg, btns, false);
  lv_obj_add_event_cb(mbox, [](lv_event_t *e) {
    lv_obj_t *obj = lv_obj_get_parent(lv_event_get_target(e));
    auto *panel = (GuidedCalibrationPanel *)lv_event_get_user_data(e);
    bool start = lv_msgbox_get_active_btn(obj) == 1;
    lv_msgbox_close(obj);
    if (start) {
      panel->send(cal_start_command(panel->flow), "Start failed");
    }
  }, LV_EVENT_VALUE_CHANGED, this);
  KUtils::style_lock_mbox(mbox, 90);
}

void GuidedCalibrationPanel::submit_measurement() {
  std::string cmd = cal_input_command(flow, lv_textarea_get_text(measure_ta));
  if (cmd.empty()) {
    KUtils::notify_toast(flow == CalFlow::LOAD_CELL
                           ? "Enter the weight in grams (50 or more), e.g. 1500"
                           : "Enter the distance in mm, e.g. 98.5", 3000);
    return;
  }
  lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  lv_textarea_set_text(measure_ta, "");
  send(cmd, "Measurement rejected");
}

void GuidedCalibrationPanel::handle_callback(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
    return;
  }
  lv_obj_t *btn = lv_event_get_current_target(e);
  if (btn == back_btn.get_container()) {
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_background(cont);
  } else if (btn == start_btn) {
    if (KUtils::is_printing()) {
      KUtils::notify_locked();
      return;
    }
    confirm_start();
  } else if (btn == cancel_btn) {
    send(cal_cancel_command(flow), "Cancel failed");
  } else if (btn == measure_btn) {
    submit_measurement();
  }
}

void GuidedCalibrationPanel::handle_kb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *target = lv_event_get_target(e);
  if (target == measure_ta && code == LV_EVENT_FOCUSED) {
    lv_keyboard_set_textarea(kb, measure_ta);
    lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
  } else if (target == kb && code == LV_EVENT_READY) {
    submit_measurement();
  } else if (target == kb && code == LV_EVENT_CANCEL) {
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  }
}
