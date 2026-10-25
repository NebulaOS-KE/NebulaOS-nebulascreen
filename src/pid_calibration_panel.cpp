#include "pid_calibration_panel.h"
#include "settings_ui.h"
#include "state.h"
#include "utils.h"
#include "spdlog/spdlog.h"

// This file may be distributed under the terms of the GNU GPLv3 license.

static lv_obj_t *make_text_btn(lv_obj_t *parent, const char *label, lv_event_cb_t cb, void *user) {
  lv_obj_t *b = lv_btn_create(parent);
  lv_obj_set_height(b, 44);
  lv_obj_set_flex_grow(b, 1);
  lv_obj_t *l = lv_label_create(b);
  lv_label_set_text(l, label);
  lv_obj_center(l);
  SettingsUi::style_text_button(b);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
  return b;
}

PidCalibrationPanel::PidCalibrationPanel(KWebSocketClient &c, std::mutex &lock)
  : NotifyConsumer(lock)
  , ws(c)
  , cont(lv_obj_create(lv_scr_act()))
{
  lv_obj_move_background(cont);
  lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_pad_all(cont, 10, 0);
  lv_obj_set_style_pad_row(cont, 8, 0);
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);

  SettingsUi::page_title(cont, "Heaters (PID)");

  lv_obj_t *body = lv_obj_create(cont);
  lv_obj_set_width(body, LV_PCT(75));
  lv_obj_set_flex_grow(body, 1);
  lv_obj_set_style_pad_all(body, 0, 0);
  lv_obj_set_style_border_width(body, 0, 0);
  lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
  lv_obj_set_scroll_dir(body, LV_DIR_VER);
  text = lv_label_create(body);
  lv_obj_set_width(text, LV_PCT(100));
  lv_label_set_long_mode(text, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(text, &lv_font_montserrat_14, 0);

  btn_row = lv_obj_create(cont);
  lv_obj_set_size(btn_row, LV_PCT(75), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(btn_row, 0, 0);
  lv_obj_set_style_pad_column(btn_row, 8, 0);
  lv_obj_set_style_border_width(btn_row, 0, 0);
  lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
  bed_btn = make_text_btn(btn_row, LV_SYMBOL_PLAY " Bed 65 C", &PidCalibrationPanel::_handle_callback, this);
  hotend_btn = make_text_btn(btn_row, LV_SYMBOL_PLAY " Hotend 230 C", &PidCalibrationPanel::_handle_callback, this);

  back_btn = SettingsUi::back_tile(cont, &PidCalibrationPanel::_handle_callback, this);

  ws.register_notify_update(this);
}

PidCalibrationPanel::~PidCalibrationPanel() {
  ws.unregister_notify_update(this);
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void PidCalibrationPanel::foreground() {
  refresh();
  lv_obj_move_foreground(cont);
}

void PidCalibrationPanel::refresh() {
  std::string t;
  if (running) {
    const char *obj = heater == PidHeater::BED ? "/printer_state/heater_bed" : "/printer_state/extruder";
    t = pid_running_text(heater, State::get_instance()->get_data(json::json_pointer(obj)));
  } else {
    t = result.empty() ? std::string(PID_DESCRIPTION) : result + "\n\n" + PID_DESCRIPTION;
    if (KUtils::is_printing()) t += "\n\nNot available while printing.";
  }
  lv_label_set_text(text, t.c_str());
  if (running) {
    lv_obj_add_flag(btn_row, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_HIDDEN);
  }
}

void PidCalibrationPanel::consume(json &j) {
  if (!running) return;
  const json &n = j;
  const char *ptr = heater == PidHeater::BED ? "/params/0/heater_bed" : "/params/0/extruder";
  if (!n.contains(json::json_pointer(ptr))) return;
  std::lock_guard<std::mutex> lock(lv_lock);
  refresh();
}

// PID_CALIBRATE answers when it is done (several minutes); then record + save.
void PidCalibrationPanel::start(PidHeater h) {
  if (KUtils::is_printing()) {
    KUtils::notify_locked();
    return;
  }
  running = true;
  heater = h;
  result.clear();
  refresh();
  std::string cmd = pid_command(h);
  spdlog::info("pid calibration: {}", cmd);
  ws.gcode_script(cmd, [this, h](json &reply) {
    std::lock_guard<std::mutex> lock(lv_lock);
    running = false;
    if (reply.contains("error")) {
      std::string msg = reply["/error/message"_json_pointer].is_string()
        ? reply["/error/message"_json_pointer].template get<std::string>() : reply["error"].dump();
      spdlog::warn("pid calibration failed: {}", msg);
      result = "Failed: " + msg + "\nNothing was saved. Let the heater cool down and try again.";
      refresh();
      return;
    }
    result = pid_summary(h) + " - saving, Klipper restarts now. The screen reconnects by itself.";
    refresh();
    auto status = State::get_instance()->get_data("/printer_state/nebulaos_calibration"_json_pointer);
    ws.gcode_script(pid_save_script(status, h));
  });
}

void PidCalibrationPanel::handle_callback(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  lv_obj_t *btn = lv_event_get_current_target(e);
  if (btn == back_btn) {
    lv_obj_move_background(cont);
  } else if (btn == bed_btn || btn == hotend_btn) {
    if (KUtils::is_printing()) {
      KUtils::notify_locked();
      return;
    }
    PidHeater h = btn == bed_btn ? PidHeater::BED : PidHeater::HOTEND;
    const char *msg = h == PidHeater::BED
      ? "The bed heats to 65 C a few times (about 5 minutes), then Klipper restarts to save."
      : "The hotend heats to 230 C a few times (about 5 minutes), then Klipper restarts to save.\n\n"
        "Keep away from the nozzle.";
    SettingsUi::confirm("Heaters (PID)", msg, "Start", SettingsUi::Tone::Warning,
                        [this, h]() { start(h); });
  }
}
