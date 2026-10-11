#include "printer_settings_panel.h"
#include "config.h"
#include "settings_ui.h"
#include "utils.h"
#include "spdlog/spdlog.h"

LV_IMG_DECLARE(back);
LV_IMG_DECLARE(refresh_img);
LV_IMG_DECLARE(chip_img);

static lv_obj_t *right_switch(lv_obj_t *row, bool on) {
  lv_obj_t *sw = lv_switch_create(row);
  lv_obj_align(sw, LV_ALIGN_RIGHT_MID, 0, 0);
  if (on) {
    lv_obj_add_state(sw, LV_STATE_CHECKED);
  }
  return sw;
}

static bool config_bool(const char *path, bool fallback) {
  auto v = Config::get_instance()->get_json(path);
  return v.is_boolean() ? v.template get<bool>() : fallback;
}

PrinterSettingsPanel::PrinterSettingsPanel(KWebSocketClient &c, std::mutex &lock)
  : ws(c)
  , lv_lock(lock)
  , cont(SettingsUi::page("Printer", &body))
  , mcu_panel(c, lock)
  , back_btn(cont, &back, "Back", &PrinterSettingsPanel::_handle_callback, this)
{
  lv_obj_t *left = SettingsUi::column(body, 58);
  lv_obj_t *right = SettingsUi::column(body, 39);

  temp_dd = lv_dropdown_create(SettingsUi::row(left, "Default extruder temp"));
  lv_dropdown_set_options(temp_dd, "180\n200\n220\n240\n260\n280\n300");
  lv_obj_set_width(temp_dd, 80);
  lv_obj_align(temp_dd, LV_ALIGN_RIGHT_MID, 0, 0);
  auto t = Config::get_instance()->get_json("/default_extruder_temp");
  if (t.is_number_integer()) {
    int32_t idx = lv_dropdown_get_option_index(temp_dd, std::to_string(t.template get<int>()).c_str());
    if (idx >= 0) {
      lv_dropdown_set_selected(temp_dd, idx);
    }
  }
  estop_sw = right_switch(SettingsUi::row(left, "Ask before emergency stop"),
                          config_bool("/prompt_emergency_stop", true));
  invert_y_sw = right_switch(SettingsUi::row(left, "Invert Y arrows"),
                             config_bool("/invert_y_direction", false));
  invert_z_sw = right_switch(SettingsUi::row(left, "Invert Z arrows"),
                             config_bool("/invert_z_direction", false));

  restart_klipper_btn = SettingsUi::icon_button(right, &refresh_img, "Restart Klipper",
                                                &PrinterSettingsPanel::_handle_callback, this);
  restart_mcu_btn = SettingsUi::icon_button(right, &refresh_img, "Restart MCU",
                                            &PrinterSettingsPanel::_handle_callback, this);
  mcu_fw_btn = SettingsUi::icon_button(right, &chip_img, "MCU firmware",
                                       &PrinterSettingsPanel::_handle_callback, this);
  SettingsUi::hint(right, "Restart MCU restarts the printer board's firmware, then Klipper. "
                          "MCU firmware installs a new one.");

  for (lv_obj_t *w : {temp_dd, estop_sw, invert_y_sw, invert_z_sw}) {
    lv_obj_add_event_cb(w, &PrinterSettingsPanel::_handle_callback, LV_EVENT_VALUE_CHANGED, this);
  }

  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_FLOATING);
  lv_obj_align(back_btn.get_container(), LV_ALIGN_BOTTOM_RIGHT, 0, 0);
}

PrinterSettingsPanel::~PrinterSettingsPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void PrinterSettingsPanel::foreground() {
  lv_obj_move_foreground(cont);
}

void PrinterSettingsPanel::handle_callback(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *obj = lv_event_get_current_target(e);
  Config *conf = Config::get_instance();

  if (code == LV_EVENT_CLICKED) {
    if (obj == back_btn.get_container()) {
      lv_obj_move_background(cont);
    } else if (obj == mcu_fw_btn) {
      mcu_panel.foreground();
    } else if (obj == restart_klipper_btn || obj == restart_mcu_btn) {
      bool mcu = obj == restart_mcu_btn;
      KUtils::confirm_if_printing(
        mcu ? "A print is running. Restart the MCU and stop it?"
            : "A print is running. Restart Klipper and stop it?",
        [this, mcu]() {
          ws.send_jsonrpc(mcu ? "printer.firmware_restart" : "printer.restart");
          KUtils::notify_toast(mcu ? "Restarting the MCU..." : "Restarting Klipper...", 3000);
        });
    }
    return;
  }
  if (code != LV_EVENT_VALUE_CHANGED) {
    return;
  }
  if (obj == temp_dd) {
    char buf[16];
    lv_dropdown_get_selected_str(temp_dd, buf, sizeof(buf));
    conf->set<int>("/default_extruder_temp", std::stoi(buf));
  } else if (obj == estop_sw) {
    conf->set<bool>("/prompt_emergency_stop", lv_obj_has_state(estop_sw, LV_STATE_CHECKED));
  } else if (obj == invert_y_sw) {
    conf->set<bool>("/invert_y_direction", lv_obj_has_state(invert_y_sw, LV_STATE_CHECKED));
  } else if (obj == invert_z_sw) {
    conf->set<bool>("/invert_z_direction", lv_obj_has_state(invert_z_sw, LV_STATE_CHECKED));
  } else {
    return;
  }
  conf->save();
}
