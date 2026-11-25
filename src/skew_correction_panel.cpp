#include "skew_correction_panel.h"
#include "settings_ui.h"
#include "state.h"
#include "utils.h"
#include "nebulaos_calibration_status.h"
#include "skew_gcode.h"
#include "spdlog/spdlog.h"

#include <string>
#include <cstdlib>

// XY-plane measurement labels (A-C, B-D are the diagonals; A-D the reference).
static const char *FIELD_LABELS[3] = {"A-C", "B-D", "A-D"};

bool SkewCorrectionPanel::is_enabled() {
  // Detect via the RAW config sections, not settings: [skew_correction] is a
  // bare section whose module reads no config options, so Klipper never lists
  // it under configfile.settings (unlike axis_twist_compensation, which has
  // options). It IS always present under configfile.config when in printer.cfg.
  auto cfg = State::get_instance()->get_data(
    "/printer_state/configfile/config/skew_correction"_json_pointer);
  return !cfg.is_null();
}

SkewCorrectionPanel::SkewCorrectionPanel(KWebSocketClient &websocket_client)
  : ws(websocket_client)
  , panel_cont(lv_obj_create(lv_scr_act()))
{
  lv_obj_move_background(panel_cont);
  lv_obj_set_size(panel_cont, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_pad_all(panel_cont, 8, 0);
  lv_obj_set_flex_flow(panel_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(panel_cont, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(panel_cont, 6, 0);

  // G94 (Run 18): the page title and the Back tile like every page; the
  // rows stop left of the tile
  SettingsUi::page_title(panel_cont, "Skew Correction");

  body = lv_label_create(panel_cont);
  lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(body, LV_PCT(96));
  lv_label_set_text(body,
    "Print a skew calibration object, measure the three lengths (mm) with "
    "calipers, and enter them. A-C and B-D are the diagonals; A-D the edge.");
  lv_obj_set_style_text_font(body, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_align(body, LV_TEXT_ALIGN_LEFT, 0);

  status_label = lv_label_create(panel_cont);
  lv_obj_set_style_text_font(status_label, &lv_font_montserrat_16, 0);
  update_status();
  SettingsUi::on_shown(panel_cont, [this]() { update_status(); });

  for (int i = 0; i < 3; i++) {
    lv_obj_t *row = lv_obj_create(panel_cont);
    rows[i] = row;
    lv_obj_set_size(row, LV_PCT(84), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(row, 2, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text(lbl, FIELD_LABELS[i]);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);

    ta[i] = lv_textarea_create(row);
    lv_textarea_set_one_line(ta[i], true);
    lv_textarea_set_placeholder_text(ta[i], "mm");
    lv_obj_set_width(ta[i], LV_PCT(72));
    lv_obj_add_event_cb(ta[i], &SkewCorrectionPanel::_handle_ta, LV_EVENT_ALL, this);
  }

  lv_obj_t *btnrow = lv_obj_create(panel_cont);
  lv_obj_set_size(btnrow, LV_PCT(84), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(btnrow, 2, 0);
  lv_obj_set_style_border_width(btnrow, 0, 0);
  lv_obj_clear_flag(btnrow, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(btnrow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(btnrow, LV_FLEX_ALIGN_SPACE_EVENLY,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  back_btn = SettingsUi::back_tile(panel_cont, &SkewCorrectionPanel::_handle_callback, this);

  apply_btn = lv_btn_create(btnrow);
  lv_obj_set_size(apply_btn, LV_PCT(48), 44);
  lv_obj_t *al = lv_label_create(apply_btn);
  lv_label_set_text(al, LV_SYMBOL_OK "  Apply & Save");
  lv_obj_center(al);
  SettingsUi::style_text_button(apply_btn);
  lv_obj_add_event_cb(apply_btn, &SkewCorrectionPanel::_handle_callback, LV_EVENT_CLICKED, this);

  clear_btn = lv_btn_create(btnrow);
  lv_obj_set_size(clear_btn, LV_PCT(48), 44);
  lv_obj_t *cl = lv_label_create(clear_btn);
  lv_label_set_text(cl, LV_SYMBOL_TRASH "  Clear correction");
  lv_obj_center(cl);
  SettingsUi::style_text_button(clear_btn);
  lv_obj_add_event_cb(clear_btn, &SkewCorrectionPanel::_handle_callback, LV_EVENT_CLICKED, this);

  // Own decimal numeric keyboard (the shared Numpad is integer-only). Digits +
  // "." + backspace + OK. Overlays the bottom; hidden until a field is tapped.
  kb = lv_keyboard_create(panel_cont);
  static const char *kb_map[] = {"1", "2", "3", "\n",
                                 "4", "5", "6", "\n",
                                 "7", "8", "9", "\n",
                                 LV_SYMBOL_BACKSPACE, "0", ".", LV_SYMBOL_OK, NULL};
  static const lv_btnmatrix_ctrl_t kb_ctrl[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_USER_1, kb_map, kb_ctrl);
  lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_USER_1);
  lv_obj_add_flag(kb, LV_OBJ_FLAG_FLOATING);
  // Side keypad like the guided calibrations: the fields stay visible left of it
  lv_obj_add_flag(kb, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_set_size(kb, LV_PCT(45), LV_PCT(62));
  lv_obj_align(kb, LV_ALIGN_TOP_RIGHT, 0, 22);
  show_kb(NULL);
  lv_obj_add_event_cb(kb, &SkewCorrectionPanel::_handle_kb, LV_EVENT_ALL, this);
}

SkewCorrectionPanel::~SkewCorrectionPanel() {
  if (panel_cont != NULL) {
    lv_obj_del(panel_cont);
    panel_cont = NULL;
  }
}

void SkewCorrectionPanel::update_status() {
  bool on = skew_is_on(State::get_instance()->get_data(
    "/printer_state/skew_correction"_json_pointer));
  lv_label_set_text(status_label, on ? "Skew correction: on" : "Skew correction: off");
}

void SkewCorrectionPanel::foreground() {
  update_status();
  show_kb(NULL);
  lv_obj_move_foreground(panel_cont);
}

void SkewCorrectionPanel::handle_ta(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_FOCUSED) {
    show_kb(lv_event_get_target(e));
  }
}

// #31 follow-up (2026-11-25): the keypad used to cover the bottom half, so
// B-D and A-D were typed blind. Now it sits on the right (as in the guided
// calibrations); while it is shown the description is hidden and the rows
// narrow, so all three fields stay visible. NULL hides it again.
void SkewCorrectionPanel::show_kb(lv_obj_t *field) {
  bool on = field != NULL;
  if (on) {
    lv_keyboard_set_textarea(kb, field);
    lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(kb);
    lv_obj_add_flag(body, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_HIDDEN);
  }
  // (LV_PCT does not parenthesize its argument: no ?: inside it)
  lv_coord_t w = on ? LV_PCT(53) : LV_PCT(84);
  for (int i = 0; i < 3; i++) lv_obj_set_width(rows[i], w);
  lv_obj_scroll_to_y(panel_cont, 0, LV_ANIM_OFF);
}

void SkewCorrectionPanel::handle_kb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
    show_kb(NULL);
  }
}

void SkewCorrectionPanel::handle_callback(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  lv_obj_t *btn = lv_event_get_current_target(event);
  if (btn == back_btn) {
    show_kb(NULL);
    lv_obj_move_background(panel_cont);
  } else if (btn == apply_btn) {
    apply_and_save();
  } else if (btn == clear_btn) {
    SettingsUi::confirm("Clear skew correction",
      "Removes the saved correction, for a new calibration print. "
      "Klipper restarts.", "Clear", SettingsUi::Tone::Warning,
      [this]() { clear_correction(); });
  }
}

void SkewCorrectionPanel::apply_and_save() {
  std::string s[3];
  for (int i = 0; i < 3; i++) {
    const char *txt = lv_textarea_get_text(ta[i]);
    s[i] = txt ? std::string(txt) : std::string();
    if (s[i].empty() || std::atof(s[i].c_str()) <= 0.0) {
      KUtils::notify_toast("Enter all three measurements (mm) first.", 4000);
      return;
    }
  }
  // #31: values, profile save, persist (restarts Klipper; loaded at start).
  // G105 (Run 19): the Calibration menu's status line (newer extensions)
  std::string record = cal_record_prefix(State::get_instance()->get_data("/printer_state/nebulaos_calibration"_json_pointer), "skew",
    fmt::format("A-C {}, B-D {}, A-D {} mm", s[0], s[1], s[2]));
  ws.gcode_script(skew_apply_gcode(s[0], s[1], s[2], record));
  KUtils::notify_toast("Skew applied. Saving and restarting Klipper.", 4000);
  show_kb(NULL);
  lv_obj_move_background(panel_cont);
}

void SkewCorrectionPanel::clear_correction() {
  ws.gcode_script(skew_clear_gcode());
  KUtils::notify_toast("Skew correction cleared. Restarting Klipper.", 4000);
  show_kb(NULL);
  lv_obj_move_background(panel_cont);
}
