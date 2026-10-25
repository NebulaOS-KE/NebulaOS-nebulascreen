#include "settings_ui.h"
#include "guided_calibration_panel.h"
#include "state.h"
#include "utils.h"
#include "spdlog/spdlog.h"

LV_IMG_DECLARE(back);
LV_IMG_DECLARE(load_cell_img);

static lv_obj_t *make_text_btn(lv_obj_t *parent, const char *label, lv_event_cb_t cb, void *user) {
  lv_obj_t *b = lv_btn_create(parent);
  lv_obj_set_height(b, 44);
  lv_obj_set_flex_grow(b, 1);
  lv_obj_t *l = lv_label_create(b);
  lv_label_set_text(l, label);
  lv_obj_center(l);
  SettingsUi::style_text_button(b);   // G94: one text-button font
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
  return b;
}

GuidedCalibrationPanel::GuidedCalibrationPanel(KWebSocketClient &c, std::mutex &lock, CalFlow f)
  : NotifyConsumer(lock)
  , ws(c)
  , flow(f)
  , cont(lv_obj_create(lv_scr_act()))
  , title(lv_label_create(cont))
  , body(lv_obj_create(cont))
  , text(lv_label_create(body))
  , measure_row(lv_obj_create(body))
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

  // G94 (Run 18): the title at the same place as on every other page
  SettingsUi::place_title(title);

  // The scrolling part: step text + entry row. It takes the height left over
  // by the title and the pinned button row.
  lv_obj_set_width(body, LV_PCT(75));
  lv_obj_set_flex_grow(body, 1);
  lv_obj_set_style_pad_all(body, 0, 0);
  lv_obj_set_style_pad_right(body, 4, 0);  // room for the scrollbar
  lv_obj_set_style_pad_row(body, 8, 0);
  lv_obj_set_style_border_width(body, 0, 0);
  lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
  lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(body, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_AUTO);

  if (flow == CalFlow::LOAD_CELL) {
    static const char *mode_map[] = {"Automatic", "Manual", ""};
    mode_sel = lv_btnmatrix_create(body);
    lv_btnmatrix_set_map(mode_sel, mode_map);
    lv_btnmatrix_set_btn_ctrl_all(mode_sel, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_btnmatrix_set_one_checked(mode_sel, true);
    lv_btnmatrix_set_btn_ctrl(mode_sel, 0, LV_BTNMATRIX_CTRL_CHECKED);
    lv_obj_set_size(mode_sel, LV_PCT(100), 36);
    lv_obj_set_style_pad_all(mode_sel, 2, 0);
    lv_obj_set_style_pad_column(mode_sel, 4, 0);
    lv_obj_set_style_text_font(mode_sel, &lv_font_montserrat_12, LV_PART_ITEMS);
    lv_obj_move_to_index(mode_sel, 0);
    lv_obj_add_flag(mode_sel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(mode_sel, [](lv_event_t *e) {
      auto *self = (GuidedCalibrationPanel *)lv_event_get_user_data(e);
      uint16_t i = lv_btnmatrix_get_selected_btn(self->mode_sel);
      if (i == 0 || i == 1) self->select_mode(i == 0);
    }, LV_EVENT_VALUE_CHANGED, this);
  }

  if (!cal_flow_steps(flow).empty()) {
    prog_box = lv_obj_create(body);
    lv_obj_move_to_index(prog_box, 0);
    lv_obj_set_size(prog_box, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(prog_box, 0, 0);
    lv_obj_set_style_pad_column(prog_box, 6, 0);
    lv_obj_set_style_border_width(prog_box, 0, 0);
    lv_obj_set_style_bg_opa(prog_box, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(prog_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(prog_box, LV_FLEX_FLOW_ROW);
    lv_obj_t *left = lv_obj_create(prog_box);
    lv_obj_t *right = lv_obj_create(prog_box);
    for (lv_obj_t *c : {left, right}) {
      lv_obj_set_height(c, LV_SIZE_CONTENT);
      lv_obj_set_style_pad_all(c, 0, 0);
      lv_obj_set_style_pad_row(c, c == left ? 4 : 1, 0);
      lv_obj_set_style_border_width(c, 0, 0);
      lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
      lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    }
    lv_obj_set_width(left, LV_PCT(48));
    lv_obj_set_flex_grow(right, 1);
    steps_col = right;
    prog_counter = lv_label_create(left);
    lv_obj_set_style_text_font(prog_counter, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(prog_counter, lv_palette_main(LV_PALETTE_GREY), 0);
    prog_head = lv_label_create(left);
    lv_obj_set_width(prog_head, LV_PCT(100));
    lv_label_set_long_mode(prog_head, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(prog_head, &lv_font_montserrat_20, 0);
    prog_detail = lv_label_create(left);
    lv_obj_set_width(prog_detail, LV_PCT(100));
    lv_label_set_long_mode(prog_detail, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(prog_detail, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(prog_detail, lv_palette_lighten(LV_PALETTE_BLUE, 2), 0);
    for (int i = 0; i < kMaxSteps; i++) {
      step_lbls[i] = lv_label_create(right);
      lv_obj_set_width(step_lbls[i], LV_PCT(100));
      lv_label_set_long_mode(step_lbls[i], LV_LABEL_LONG_DOT);
      lv_obj_set_style_text_font(step_lbls[i], &lv_font_montserrat_12, 0);
    }
    lv_obj_add_flag(prog_box, LV_OBJ_FLAG_HIDDEN);
  }

  lv_obj_set_width(text, LV_PCT(100));
  lv_label_set_long_mode(text, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(text, &lv_font_montserrat_14, 0);

  for (lv_obj_t *row : {measure_row, btn_row}) {
    lv_obj_set_size(row, row == btn_row ? LV_PCT(75) : LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  }
  lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_HIDDEN);

  lv_textarea_set_one_line(measure_ta, true);
  lv_textarea_set_placeholder_text(measure_ta, cal_input_hint(flow));
  lv_textarea_set_accepted_chars(measure_ta, "0123456789.");
  lv_textarea_set_max_length(measure_ta, 6);
  lv_obj_set_flex_grow(measure_ta, 2);
  lv_obj_add_event_cb(measure_ta, &GuidedCalibrationPanel::_handle_kb, LV_EVENT_FOCUSED, this);
  lv_obj_add_event_cb(measure_ta, &GuidedCalibrationPanel::_handle_kb, LV_EVENT_CLICKED, this);
  measure_btn = make_text_btn(measure_row, (std::string(LV_SYMBOL_OK " ") + cal_input_button(flow)).c_str(),
                              &GuidedCalibrationPanel::_handle_callback, this);

  for (int i = 0; i < kMaxActions; i++) {
    action_btns[i] = make_text_btn(btn_row, "", &GuidedCalibrationPanel::_handle_callback, this);
    lv_obj_add_flag(action_btns[i], LV_OBJ_FLAG_HIDDEN);
  }
  start_btn = make_text_btn(btn_row, LV_SYMBOL_PLAY " Start", &GuidedCalibrationPanel::_handle_callback, this);
  cancel_btn = make_text_btn(btn_row, LV_SYMBOL_CLOSE " Cancel", &GuidedCalibrationPanel::_handle_callback, this);

  // Decimal keypad (the shared Numpad is integer-only). It opens on the right
  // half, between the title and the Back button, while the text, the entry
  // field and the buttons move into the left half - so it never covers the
  // field (Run 15 test, G90). The keyboard key closes it.
  static const char *kb_map[] = {"1", "2", "3", LV_SYMBOL_BACKSPACE, "\n",
                                 "4", "5", "6", LV_SYMBOL_KEYBOARD, "\n",
                                 "7", "8", "9", ".", "\n",
                                 "0", LV_SYMBOL_OK, NULL};
  static const lv_btnmatrix_ctrl_t kb_ctrl[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 1};
  lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_USER_1, kb_map, kb_ctrl);
  lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_USER_1);
  lv_obj_add_flag(kb, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_add_flag(kb, LV_OBJ_FLAG_FLOATING);
  lv_obj_set_size(kb, LV_PCT(45), LV_PCT(62));
  lv_obj_align(kb, LV_ALIGN_TOP_RIGHT, 0, 22);
  lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(kb, &GuidedCalibrationPanel::_handle_kb, LV_EVENT_ALL, this);

  if (flow == CalFlow::LOAD_CELL) {
    // Run 19: Tim's load-cell icon in the free top-right corner (46 px on
    // the small screen, 64 px otherwise - the Makefile's ASSET_DIR)
    lv_obj_t *icon = lv_img_create(cont);
    lv_img_set_src(icon, &load_cell_img);
    lv_obj_add_flag(icon, LV_OBJ_FLAG_FLOATING);
    lv_obj_align(icon, LV_ALIGN_TOP_RIGHT, -8, 30);
  }

  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_FLOATING);
  SettingsUi::place_back(back_btn.get_container());  // G94: one Back position

  ws.register_notify_update(this);
}

GuidedCalibrationPanel::~GuidedCalibrationPanel() {
  ws.unregister_notify_update(this);
  if (restart_timer != nullptr) {
    lv_timer_del(restart_timer);
    restart_timer = nullptr;
  }
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
  auto status = State::get_instance()->get_data(
    json::json_pointer(std::string("/printer_state/") + cal_status_object(flow)));
  CalView v = cal_view(flow, status, KUtils::is_printing());
  // G104: step list, restart notice, end / failure screen
  auto wh = State::get_instance()->get_data("/printer_state/webhooks/state"_json_pointer);
  bool klippy_ready = !wh.is_string() || wh.template get<std::string>() == "ready";
  CalFeedback fb = cal_feedback(flow, status, klippy_ready, v);
  render_feedback(fb);
  bool restarting = v.text == CAL_RESTART_NOTICE || v.text == CAL_RECONNECT_NOTICE;
  if (restarting && restart_timer == nullptr) {
    restart_timer = lv_timer_create([](lv_timer_t *t) {
      ((GuidedCalibrationPanel *)t->user_data)->refresh();
    }, 1000, this);
  } else if (!restarting && restart_timer != nullptr) {
    lv_timer_del(restart_timer);
    restart_timer = nullptr;
  }
  if (mode_sel != nullptr) {
    // G99: the tapped mode shows at once; the status confirms it
    bool want_auto = v.mode_auto;
    if (!pending_mode.empty()) {
      if ((pending_mode == "auto") == v.mode_auto) pending_mode.clear();
      else want_auto = pending_mode == "auto";
    }
    lv_btnmatrix_set_btn_ctrl(mode_sel, want_auto ? 0 : 1, LV_BTNMATRIX_CTRL_CHECKED);
    lv_btnmatrix_clear_btn_ctrl(mode_sel, want_auto ? 1 : 0, LV_BTNMATRIX_CTRL_CHECKED);
    if (v.show_modes) lv_obj_clear_flag(mode_sel, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(mode_sel, LV_OBJ_FLAG_HIDDEN);
    if (v.show_modes && want_auto && auto_checking) {
      cal_auto_check_running(v);
    }
  }
  lv_label_set_text(title, v.title.c_str());
  bool text_changed = false;
  if (v.text != shown_text) {
    // a new step starts at the top of its text
    lv_label_set_text(text, v.text.c_str());
    lv_obj_scroll_to_y(body, 0, LV_ANIM_OFF);
    shown_text = v.text;
    text_changed = true;
  }
  auto show = [](lv_obj_t *o, bool on) {
    if (on) lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
  };
  show(text, !v.text.empty());
  show(start_btn, v.can_start);
  show(cancel_btn, v.can_cancel);
  for (int i = 0; i < kMaxActions; i++) {
    bool on = i < (int)v.actions.size();
    if (on) {
      lv_label_set_text(lv_obj_get_child(action_btns[i], 0), v.actions[i].first.c_str());
      action_cmds[i] = v.actions[i].second;
    }
    show(action_btns[i], on);
  }
  show(measure_row, v.needs_measurement);
  if (!cal_input_keep(v) && *lv_textarea_get_text(measure_ta) != '\0') {
    lv_textarea_set_text(measure_ta, "");   // the flow is over: a new run starts empty
  }
  if (!v.needs_measurement) {
    show_keypad(false);
  } else if (text_changed && !lv_obj_has_flag(kb, LV_OBJ_FLAG_HIDDEN)) {
    // new text above the field while typing (e.g. a problem): keep the field
    // in view next to the keypad
    lv_obj_update_layout(cont);
    lv_obj_scroll_to_view(measure_row, LV_ANIM_OFF);
  }
}

void GuidedCalibrationPanel::render_feedback(const CalFeedback &f) {
  if (prog_box == nullptr) return;
  // While a flow runs, its prompt / the restart notice comes first (the step
  // list is long); after it, the headline first and the result lines below.
  lv_obj_move_to_index(text, f.progress.active ? 0 : 1);
  if (f.progress.restarting) {
    lv_obj_set_style_text_color(text, lv_palette_main(LV_PALETTE_AMBER), 0);
  } else {
    lv_obj_remove_local_style_prop(text, LV_STYLE_TEXT_COLOR, 0);
  }
  bool on = f.progress.active || f.result.present;
  if (!on) {
    lv_obj_add_flag(prog_box, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_obj_clear_flag(prog_box, LV_OBJ_FLAG_HIDDEN);
  if (f.progress.active) {
    lv_label_set_text(prog_counter, f.progress.counter.c_str());
    lv_label_set_text(prog_head, f.progress.label.c_str());
    lv_obj_set_style_text_color(prog_head, lv_color_white(), 0);
    lv_label_set_text(prog_detail, f.progress.detail.c_str());
    lv_obj_clear_flag(prog_counter, LV_OBJ_FLAG_HIDDEN);
    if (f.progress.detail.empty()) lv_obj_add_flag(prog_detail, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(prog_detail, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_label_set_text(prog_head, f.result.headline.c_str());
    lv_obj_set_style_text_color(prog_head, f.result.ok ? lv_palette_main(LV_PALETTE_GREEN)
                                                     : lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_add_flag(prog_counter, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(prog_detail, LV_OBJ_FLAG_HIDDEN);
  }
  for (int i = 0; i < kMaxSteps; i++) {
    bool show = f.progress.active && i < (int)f.progress.steps.size();
    if (!show) {
      lv_obj_add_flag(step_lbls[i], LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    const CalStepRow &r = f.progress.steps[i];
    std::string t = (r.state == CalStepState::DONE ? std::string(LV_SYMBOL_OK " ")
                     : r.state == CalStepState::CURRENT ? std::string(LV_SYMBOL_RIGHT " ")
                     : std::string("    ")) + r.label;
    lv_label_set_text(step_lbls[i], t.c_str());
    lv_obj_set_style_text_color(step_lbls[i],
      r.state == CalStepState::DONE ? lv_palette_main(LV_PALETTE_GREEN)
      : r.state == CalStepState::CURRENT ? lv_color_white()
      : lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_clear_flag(step_lbls[i], LV_OBJ_FLAG_HIDDEN);
  }
  if (f.progress.active) lv_obj_clear_flag(steps_col, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(steps_col, LV_OBJ_FLAG_HIDDEN);
}

void GuidedCalibrationPanel::show_keypad(bool on) {
  bool shown = !lv_obj_has_flag(kb, LV_OBJ_FLAG_HIDDEN);
  if (on == shown) {
    return;
  }
  if (on) {
    lv_keyboard_set_textarea(kb, measure_ta);
    lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(kb);
  } else {
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  }
  // (LV_PCT does not parenthesize its argument: no ?: inside it)
  lv_coord_t w = on ? LV_PCT(52) : LV_PCT(75);
  lv_obj_set_width(body, w);
  lv_obj_set_width(btn_row, w);
  if (on) {
    lv_obj_update_layout(cont);
    lv_obj_scroll_to_view(measure_row, LV_ANIM_OFF);
  }
}

void GuidedCalibrationPanel::consume(json &j) {
  // Read-only lookups: operator[] would insert nulls into the shared notification.
  const json &n = j;
  if (!n.contains(json::json_pointer(std::string("/params/0/") + cal_status_object(flow)))
      && !n.contains("/params/0/print_stats/state"_json_pointer)
      && !n.contains("/params/0/webhooks/state"_json_pointer)) {
    return;
  }
  std::lock_guard<std::mutex> lock(lv_lock);
  refresh();
  if (flow == CalFlow::LOAD_CELL || flow == CalFlow::BED_TRAM) {
    auto status = State::get_instance()->get_data(
      json::json_pointer(std::string("/printer_state/") + cal_status_object(flow)));
    bool active = cal_view(flow, status, false).can_cancel;
    if (active && !was_active) {
      lv_obj_move_foreground(cont);
    }
    was_active = active;
  }
}

// Called from LVGL callbacks; the reply arrives on the websocket thread.
void GuidedCalibrationPanel::send(const std::string &gcode, const char *what) {
  std::string label(what);
  spdlog::info("guided calibration: {}", gcode);
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
    : flow == CalFlow::BED_TRAM
    ? "The printer will home and measure the four bed corners, then tell you which corner to adjust, one at a time.\n\nThe bed must be empty."
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

// G99 (Run 18): Automatic / Manual, saved by the extensions at once.
void GuidedCalibrationPanel::select_mode(bool automatic) {
  if (KUtils::is_printing()) {
    KUtils::notify_locked();
    refresh();
    return;
  }
  pending_mode = automatic ? "auto" : "manual";
  std::string cmd = cal_mode_command(automatic);
  spdlog::info("guided calibration: {}", cmd);
  ws.gcode_script(cmd, [this](json &reply) {
    if (!reply.contains("error")) {
      return;
    }
    std::lock_guard<std::mutex> lock(lv_lock);
    pending_mode.clear();
    std::string msg = reply["/error/message"_json_pointer].is_string()
      ? reply["/error/message"_json_pointer].template get<std::string>() : reply["error"].dump();
    KUtils::notify_toast("Mode not changed: " + msg, 6000);
    refresh();
  });
  refresh();
}

// G99: the automatic check (no motion). Its result comes back in the status
// (auto_check, auto_check_reason), so a failure is shown on the page, not as
// a toast; other errors (printing, bottle flow running) are toasted.
void GuidedCalibrationPanel::run_auto_check() {
  if (KUtils::is_printing()) {
    KUtils::notify_locked();
    return;
  }
  auto_checking = true;
  spdlog::info("guided calibration: {}", CAL_AUTO_CHECK_COMMAND);
  ws.gcode_script(CAL_AUTO_CHECK_COMMAND, [this](json &reply) {
    std::lock_guard<std::mutex> lock(lv_lock);
    auto_checking = false;
    if (reply.contains("error")) {
      std::string msg = reply["/error/message"_json_pointer].is_string()
        ? reply["/error/message"_json_pointer].template get<std::string>() : reply["error"].dump();
      if (msg.find("automatic check FAILED") == std::string::npos) {
        KUtils::notify_toast("Check not run: " + msg, 6000);
      }
    }
    refresh();
  });
  refresh();
}

// G94 (Run 18): Weigh / Submit sends the typed value and keeps it in the
// field (a problem means "try again", usually with the same number).
void GuidedCalibrationPanel::submit_measurement() {
  std::string cmd = cal_input_command(flow, lv_textarea_get_text(measure_ta));
  if (cmd.empty()) {
    KUtils::notify_toast(cal_input_problem(flow), 3000);
    show_keypad(true);
    return;
  }
  show_keypad(false);
  send(cmd, "Measurement rejected");
}

// The keypad's OK key commits the typed value: the keypad closes, the value
// stays in the field for Weigh / Submit.
void GuidedCalibrationPanel::commit_input() {
  CalKeypadOk ok = cal_keypad_ok(flow, lv_textarea_get_text(measure_ta));
  if (!ok.close) {
    KUtils::notify_toast(ok.problem, 3000);
    return;
  }
  show_keypad(false);
}

void GuidedCalibrationPanel::handle_callback(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
    return;
  }
  lv_obj_t *btn = lv_event_get_current_target(e);
  if (btn == back_btn.get_container()) {
    show_keypad(false);
    lv_obj_move_background(cont);
  } else if (btn == start_btn) {
    if (KUtils::is_printing()) {
      KUtils::notify_locked();
      return;
    }
    confirm_start();
  } else if (btn == cancel_btn) {
    send(cal_cancel_command(flow), "Cancel failed");
  } else if (btn == action_btns[0] || btn == action_btns[1]) {
    int i = btn == action_btns[0] ? 0 : 1;
    if (action_cmds[i] == CAL_AUTO_CHECK_COMMAND) {
      run_auto_check();
    } else if (!action_cmds[i].empty()) {
      send(action_cmds[i], "Step failed");
    }
  } else if (btn == measure_btn) {
    submit_measurement();
  }
}

void GuidedCalibrationPanel::handle_kb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *target = lv_event_get_target(e);
  if (target == measure_ta && (code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED)) {
    show_keypad(true);
  } else if (target == kb && code == LV_EVENT_READY) {
    commit_input();
  } else if (target == kb && code == LV_EVENT_CANCEL) {
    show_keypad(false);
  }
}

#ifdef SIMULATOR
void GuidedCalibrationPanel::sim_type(const char *keys) {
  for (const char *k = keys; *k; k++) {
    std::string want = *k == 'K' ? std::string(LV_SYMBOL_OK) : std::string(1, *k);
    for (uint16_t i = 0; i < 32; i++) {
      const char *t = lv_btnmatrix_get_btn_text(kb, i);
      if (t == NULL) break;
      if (want == t) {
        lv_btnmatrix_set_selected_btn(kb, i);
        lv_event_send(kb, LV_EVENT_VALUE_CHANGED, NULL);
        break;
      }
    }
  }
}
#endif
