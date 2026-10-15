#include "power_panel.h"
#include "settings_ui.h"
#include "plr_status.h"
#include "state.h"
#include "utils.h"
#include "spdlog/spdlog.h"

#include <map>

LV_IMG_DECLARE(back);

PowerPanel::PowerPanel(KWebSocketClient &websocket_client, std::mutex &l)
  : NotifyConsumer(l)
  , ws(websocket_client)
  , cont(lv_obj_create(lv_scr_act()))
  , back_btn(cont, &back, "Back", &PowerPanel::_handle_callback, this)
{
  SettingsUi::on_shown(cont, [this]() { shown(); });
  lv_obj_move_background(cont);
  lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

  // Power-loss recovery sits at the top, above the power-device toggles.
  build_recovery_section();

  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_FLOATING);
  lv_obj_align(back_btn.get_container(), LV_ALIGN_BOTTOM_RIGHT, 0, 0);

  ws.register_notify_update(this);
}

void PowerPanel::build_recovery_section() {
  recovery_cont = lv_obj_create(cont);
  lv_obj_set_size(recovery_cont, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(recovery_cont, 4, 0);
  lv_obj_set_flex_flow(recovery_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(recovery_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_clear_flag(recovery_cont, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *heading = lv_label_create(recovery_cont);
  lv_label_set_text(heading, LV_SYMBOL_CHARGE " Power Loss Recovery");

  recovery_status = lv_label_create(recovery_cont);
  lv_obj_set_width(recovery_status, LV_PCT(100));
  lv_label_set_long_mode(recovery_status, LV_LABEL_LONG_WRAP);
  lv_label_set_text(recovery_status, "");

  lv_obj_t *btn_row = lv_obj_create(recovery_cont);
  lv_obj_set_size(btn_row, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(btn_row, 0, 0);
  lv_obj_set_style_pad_column(btn_row, 8, 0);
  lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);

  recovery_resume_btn = lv_btn_create(btn_row);
  lv_obj_t *rl = lv_label_create(recovery_resume_btn);
  lv_label_set_text(rl, LV_SYMBOL_PLAY " Resume Print");
  lv_obj_center(rl);
  lv_obj_add_event_cb(recovery_resume_btn, &PowerPanel::_handle_callback, LV_EVENT_CLICKED, this);

  recovery_dismiss_btn = lv_btn_create(btn_row);
  lv_obj_t *dl = lv_label_create(recovery_dismiss_btn);
  lv_label_set_text(dl, LV_SYMBOL_TRASH " Discard");
  lv_obj_center(dl);
  lv_obj_add_event_cb(recovery_dismiss_btn, &PowerPanel::_handle_callback, LV_EVENT_CLICKED, this);

  // Hidden until refresh_recovery() finds a recoverable print.
  lv_obj_add_flag(recovery_resume_btn, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(recovery_dismiss_btn, LV_OBJ_FLAG_HIDDEN);
}

void PowerPanel::refresh_recovery() {
  if (recovery_cont == nullptr) {
    return;
  }
  auto plr = parse_plr_status(State::get_instance()
    ->get_data("/printer_state/nebulaos_power_loss_recovery"_json_pointer));
  auto view = plr_view(plr, KUtils::is_printing());

  lv_label_set_text(recovery_status, view.text.c_str());
  if (view.show_resume) {
    lv_obj_clear_flag(recovery_resume_btn, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(recovery_resume_btn, LV_OBJ_FLAG_HIDDEN);
  }
  if (view.show_discard) {
    lv_obj_clear_flag(recovery_dismiss_btn, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(recovery_dismiss_btn, LV_OBJ_FLAG_HIDDEN);
  }
}

void PowerPanel::consume(json &j) {
  // State has already merged this notification into printer_state. Read-only lookups:
  // operator[] would insert nulls into the shared notification, and a null merged as a
  // patch deletes the key.
  const json &n = j;
  if (!n.contains("/params/0/nebulaos_power_loss_recovery"_json_pointer)
      && !n.contains("/params/0/print_stats/state"_json_pointer)) {
    return;
  }
  std::lock_guard<std::mutex> lock(lv_lock);
  refresh_recovery();
}

// Called from LVGL event handlers; the response arrives on the websocket thread.
void PowerPanel::send_plr_command(const char *cmd, const char *failure) {
  std::string what(failure);
  ws.gcode_script(cmd, [this, what](json &resp) {
    if (!resp.contains("error")) {
      return;
    }
    std::string msg = resp["/error/message"_json_pointer].is_string()
      ? resp["/error/message"_json_pointer].template get<std::string>()
      : resp["error"].dump();
    spdlog::warn("power panel: {}: {}", what, msg);
    std::lock_guard<std::mutex> lock(lv_lock);
    KUtils::notify_toast(what + ": " + msg, 6000);
    refresh_recovery();
  });
}

// Never a one-tap resume: the backend's position_safe is always false (Z is assumed to
// have held while unpowered), so the operator confirms after reading what will happen.
void PowerPanel::confirm_resume() {
  auto plr = parse_plr_status(State::get_instance()
    ->get_data("/printer_state/nebulaos_power_loss_recovery"_json_pointer));
  if (!plr_view(plr, KUtils::is_printing()).show_resume) {
    refresh_recovery();
    return;
  }

  static const char *btns[] = {"Cancel", "Resume", ""};
  lv_obj_t *mbox = lv_msgbox_create(NULL, NULL, plr_resume_confirm_text(plr).c_str(), btns, false);
  lv_obj_add_event_cb(mbox, [](lv_event_t *e) {
    lv_obj_t *obj = lv_obj_get_parent(lv_event_get_target(e));
    auto *panel = (PowerPanel *)lv_event_get_user_data(e);
    bool resume = lv_msgbox_get_active_btn(obj) == 1;
    lv_msgbox_close(obj);
    if (resume) {
      panel->send_plr_command(PLR_RESUME_COMMAND, "Resume failed");
      KUtils::notify_toast("Resuming print after power loss...", 2500);
      lv_label_set_text(panel->recovery_status, "Resuming...");
      lv_obj_add_flag(panel->recovery_resume_btn, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(panel->recovery_dismiss_btn, LV_OBJ_FLAG_HIDDEN);
    }
  }, LV_EVENT_VALUE_CHANGED, this);
  KUtils::style_lock_mbox(mbox, 90);
}

PowerPanel::~PowerPanel() {
  ws.unregister_notify_update(this);
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }

  devices.clear();
}

void PowerPanel::create_device(json &j) {
  std::string name = j["device"].template get<std::string>();

  lv_obj_t *power_device_toggle;
  auto entry = devices.find(name);
  if (entry == devices.end()) {
    lv_obj_t *power_device_toggle_cont = lv_obj_create(cont);
    lv_obj_set_size(power_device_toggle_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(power_device_toggle_cont, 0, 0);

    lv_obj_t *l = lv_label_create(power_device_toggle_cont);
    lv_label_set_text(l, name.c_str());
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);

    power_device_toggle = lv_switch_create(power_device_toggle_cont); 
    lv_obj_align(power_device_toggle, LV_ALIGN_RIGHT_MID, 0, 0);

    lv_obj_add_event_cb(power_device_toggle, &PowerPanel::_handle_callback,
	        LV_EVENT_VALUE_CHANGED, this);

    devices.insert({name, power_device_toggle});
  } else {
    power_device_toggle = entry->second;
  }
  
  std::string status = j["status"].template get<std::string>();
  spdlog::debug("Fetched initial status for power device {}: {}", name, status);
  
  if (status == "on") {
    lv_obj_add_state(power_device_toggle, LV_STATE_CHECKED);
  } else {
    lv_obj_clear_state(power_device_toggle, LV_STATE_CHECKED);
  }
}

void PowerPanel::create_devices(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);

  if (j.contains("result")) {
    json result = j["result"];
    if (result.contains("devices")) {
      json devices = result["devices"];
      for (auto &device : devices.items()) {
        create_device(device.value());
      }
    }
  }
}

void PowerPanel::handle_device_callback(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);

  if (j.contains("result")) {
    json result = j["result"];
    for (auto &device : result.items()) {
      auto entry = devices.find(device.key());
      if (entry != devices.end()) {
        std::string new_status = device.value();

        spdlog::debug("Fetched new status for power device {}: {}", entry->first, new_status);

        if (new_status == "on") {
          lv_obj_add_state(entry->second, LV_STATE_CHECKED);
        } else {
          lv_obj_clear_state(entry->second, LV_STATE_CHECKED);
        }
      }
    }
  }
}

void PowerPanel::foreground() {
  lv_obj_move_foreground(cont);
}

// Every time the page comes on top (SettingsUi::on_shown).
void PowerPanel::shown() {
  // Re-check for a recoverable print each time the screen is opened.
  refresh_recovery();

  if (devices.empty()) {
    return;  // no [power] devices (NebulaOS default): nothing to poll
  }
  json params;
  for (auto &device : devices) {
    params[device.first] = 0; // value is ignored by moonraker
  }
  ws.send_jsonrpc("machine.device_power.status", params, [this](json& j) {
    this->handle_device_callback(j);
  });
}

void PowerPanel::handle_callback(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    lv_obj_t *btn = lv_event_get_current_target(e);
    if (btn == back_btn.get_container()) {
      lv_obj_move_background(cont);
    } else if (btn == recovery_resume_btn) {
      confirm_resume();
    } else if (btn == recovery_dismiss_btn) {
      send_plr_command(PLR_DISCARD_COMMAND, "Discard failed");
      lv_obj_add_flag(recovery_resume_btn, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(recovery_dismiss_btn, LV_OBJ_FLAG_HIDDEN);
    }
  } else if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
    lv_obj_t *obj = lv_event_get_target(e);
    for (auto &device : devices) {
      if (obj == device.second) {
        bool turnOn = lv_obj_has_state(device.second, LV_STATE_CHECKED);
        std::string status = turnOn ? "on" : "off";

        spdlog::debug("Turning power device {} {}", device.first, status);

        json params;
        params["device"] = device.first;
        params["action"] = status;
        ws.send_jsonrpc("machine.device_power.post_device", params, [this](json& j) {
          this->handle_device_callback(j);
        });
	      break;
      }
    }
  }
}
