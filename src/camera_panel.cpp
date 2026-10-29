#include "camera_panel.h"
#include "nebulaos_camera.h"
#include "settings_ui.h"
#include "utils.h"
#include "spdlog/spdlog.h"

static lv_obj_t *right_dropdown(lv_obj_t *row, const char *options) {
  lv_obj_t *dd = lv_dropdown_create(row);
  lv_dropdown_set_options(dd, options);
  lv_obj_set_width(dd, LV_PCT(55));
  lv_obj_align(dd, LV_ALIGN_RIGHT_MID, 0, 0);
  return dd;
}

static lv_obj_t *small_button(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *user) {
  lv_obj_t *b = lv_btn_create(parent);
  lv_obj_set_size(b, LV_PCT(48), 36);
  lv_obj_t *l = lv_label_create(b);
  lv_label_set_text(l, text);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
  lv_obj_center(l);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
  return b;
}

CameraPanel::CameraPanel(KWebSocketClient &c, std::mutex &lock)
  : ws(c)
  , lv_lock(lock)
  , cont(SettingsUi::page("Camera", &body))
{
  SettingsUi::on_shown(cont, [this]() { shown(); });
  lv_obj_t *left = SettingsUi::column(body, 48);
  lv_obj_t *right = SettingsUi::column(body, 50);
  lv_obj_add_flag(right, LV_OBJ_FLAG_SCROLLABLE);

  ir_dd = right_dropdown(SettingsUi::row(left, "Night mode"), CAMERA_IR_OPTIONS);
  flicker_dd = right_dropdown(SettingsUi::row(left, "Anti-flicker"), CAMERA_FLICKER_OPTIONS);
  wb_dd = right_dropdown(SettingsUi::row(left, "White balance"), "Auto\nFixed");
  exposure_dd = right_dropdown(SettingsUi::row(left, "Exposure"), "Auto\nManual");
  for (lv_obj_t *dd : {ir_dd, flicker_dd, wb_dd, exposure_dd}) {
    lv_obj_add_event_cb(dd, &CameraPanel::_handle_callback, LV_EVENT_VALUE_CHANGED, this);
  }

  lv_obj_t *row = lv_obj_create(left);
  lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  defaults_btn = small_button(row, "Defaults", &CameraPanel::_handle_callback, this);
  reset_btn = small_button(row, "Restart camera", &CameraPanel::_handle_callback, this);
  SettingsUi::set_tone(reset_btn, SettingsUi::Tone::Warning);
  info = SettingsUi::hint(left, "...");

  for (const auto &s : camera_sliders()) {
    lv_obj_t *r = lv_obj_create(right);
    lv_obj_set_size(r, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(r, 2, 0);
    lv_obj_set_style_border_width(r, 0, 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_t *name = lv_label_create(r);
    lv_label_set_text(name, s.label);
    lv_obj_set_width(name, LV_PCT(70));
    lv_obj_t *val = lv_label_create(r);
    lv_label_set_text(val, "-");
    lv_obj_t *sl = lv_slider_create(r);
    lv_obj_set_width(sl, LV_PCT(92));
    lv_slider_set_range(sl, s.min, s.max);
    lv_obj_add_event_cb(sl, &CameraPanel::_handle_callback, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(sl, &CameraPanel::_handle_callback, LV_EVENT_RELEASED, this);
    sliders[s.key] = sl;
    slider_rows[s.key] = r;
    slider_values[s.key] = val;
  }

  back_tile = SettingsUi::back_tile(cont, &CameraPanel::_handle_callback, this);
}

CameraPanel::~CameraPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void CameraPanel::foreground() {
  lv_obj_move_foreground(cont);
}

// LVGL thread, lock held (SettingsUi::on_shown).
void CameraPanel::shown() {
  lv_label_set_text(info, "Reading the camera settings...");
  ws.send_jsonrpc(CAMERA_STATUS_METHOD, json::object(), [this](json &reply) {
    std::lock_guard<std::mutex> lock(lv_lock);
    show(reply);
  });
}

void CameraPanel::set_slider(const std::string &key, int value) {
  auto it = sliders.find(key);
  if (it == sliders.end()) return;
  lv_slider_set_value(it->second, value, LV_ANIM_OFF);
  lv_label_set_text(slider_values[key], std::to_string(value).c_str());
}

// LVGL lock held.
void CameraPanel::update_visibility() {
  bool fixed_wb = !camera_bool(config, "wb_auto", true);
  bool manual = camera_bool(config, "exposure_manual", false);
  if (fixed_wb) lv_obj_clear_flag(slider_rows["wb_temperature"], LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(slider_rows["wb_temperature"], LV_OBJ_FLAG_HIDDEN);
  if (manual) {
    lv_obj_clear_flag(slider_rows["exposure_time"], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_state(exposure_dd, LV_STATE_DISABLED);    // one-way: Restart camera
  } else {
    lv_obj_add_flag(slider_rows["exposure_time"], LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_state(exposure_dd, LV_STATE_DISABLED);
  }
}

// LVGL lock held.
void CameraPanel::show(const json &reply) {
  auto v = camera_view(reply);
  if (!v.ok) {
    lv_label_set_text(info, "The camera settings could not be read.");
    return;
  }
  loading = true;
  config = v.config;
  lv_dropdown_set_selected(ir_dd, camera_index_of(camera_ir_values(),
                                                  camera_str(config, "ir_mode", "auto")));
  lv_dropdown_set_selected(flicker_dd, camera_index_of(camera_flicker_values(),
                                                       camera_str(config, "power_line", "off")));
  lv_dropdown_set_selected(wb_dd, camera_bool(config, "wb_auto", true) ? 0 : 1);
  lv_dropdown_set_selected(exposure_dd, camera_bool(config, "exposure_manual", false) ? 1 : 0);
  for (const auto &s : camera_sliders()) {
    set_slider(s.key, camera_int(config, s.key, camera_int(v.defaults, s.key, s.min)));
  }
  update_visibility();
  loading = false;
  std::string text = v.present ? "Camera connected." : "No Nebula Camera connected - changes are saved "
                                                       "and applied when it is.";
  if (!v.firmware.empty()) text += "\nFirmware: " + v.firmware;
  text += "\nFlip the image in Mainsail (webcam settings).";
  lv_label_set_text(info, text.c_str());
  if (v.present && v.firmware.empty()) {
    ws.send_jsonrpc(CAMERA_VERSION_METHOD, json::object(), [this](json &r) {
      std::string fw = camera_version_of(r);
      if (fw.empty()) return;
      std::lock_guard<std::mutex> lock(lv_lock);
      std::string t = std::string(lv_label_get_text(info)) + "\nFirmware: " + fw;
      lv_label_set_text(info, t.c_str());
    });
  }
}

void CameraPanel::send(const json &changes) {
  for (auto &kv : changes.items()) config[kv.key()] = kv.value();
  update_visibility();
  ws.send_jsonrpc(CAMERA_SETTINGS_METHOD, camera_settings_params(changes),
                  [this](json &reply) { after(reply); });
}

// Websocket thread.
void CameraPanel::after(const json &reply) {
  std::string text = camera_result_text(reply);
  std::lock_guard<std::mutex> lock(lv_lock);
  if (!text.empty()) KUtils::notify_toast(text, 4000);
  if (reply.is_object() && reply.contains("error")) shown();   // show what is really saved
}

void CameraPanel::handle_callback(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *obj = lv_event_get_current_target(e);
  if (code == LV_EVENT_CLICKED && obj == back_tile) {
    lv_obj_move_background(cont);
    return;
  }
  if (loading) return;
  for (auto &kv : sliders) {
    if (obj != kv.second) continue;
    int value = lv_slider_get_value(obj);
    lv_label_set_text(slider_values[kv.first], std::to_string(value).c_str());
    if (code == LV_EVENT_RELEASED) send({{kv.first, value}});
    return;
  }
  if (code == LV_EVENT_VALUE_CHANGED) {
    if (obj == ir_dd) {
      send({{"ir_mode", camera_ir_values()[lv_dropdown_get_selected(ir_dd)]}});
    } else if (obj == flicker_dd) {
      send({{"power_line", camera_flicker_values()[lv_dropdown_get_selected(flicker_dd)]}});
    } else if (obj == wb_dd) {
      send({{"wb_auto", lv_dropdown_get_selected(wb_dd) == 0}});
    } else if (obj == exposure_dd && lv_dropdown_get_selected(exposure_dd) == 1) {
      lv_dropdown_set_selected(exposure_dd, 0);          // until confirmed
      SettingsUi::confirm("Manual exposure",
        "Set the exposure by hand?\n\nThe camera cannot switch back to automatic exposure "
        "by itself: only Restart camera brings it back.", "Manual", SettingsUi::Tone::Warning,
        [this]() {
          lv_dropdown_set_selected(exposure_dd, 1);
          send({{"exposure_manual", true},
                {"exposure_time", (int)lv_slider_get_value(sliders["exposure_time"])}});
        });
    }
    return;
  }
  if (code != LV_EVENT_CLICKED) return;
  if (obj == defaults_btn) {
    // Every setting, night mode and anti-flicker included (Tim 2026-10-29);
    // manual exposure only ends with a camera restart.
    bool manual = camera_bool(config, "exposure_manual", false);
    SettingsUi::confirm("Defaults",
      manual ? "Put every camera setting back to its default?\n\nThe camera restarts to get "
               "automatic exposure back; the picture is gone for a few seconds."
             : "Put every camera setting back to its default?",
      "Defaults", SettingsUi::Tone::Warning, [this, manual]() {
        if (manual) lv_label_set_text(info, "Restarting the camera...");
        ws.send_jsonrpc(CAMERA_DEFAULTS_METHOD, json::object(), [this](json &reply) {
          after(reply);
          std::lock_guard<std::mutex> lock(lv_lock);
          shown();
        });
      });
  } else if (obj == reset_btn) {
    SettingsUi::confirm("Restart camera",
      "Restart the camera?\n\nThe picture is gone for a few seconds; your settings are applied "
      "again afterwards and the exposure goes back to automatic.", "Restart",
      SettingsUi::Tone::Warning, [this]() {
        lv_label_set_text(info, "Restarting the camera...");
        ws.send_jsonrpc(CAMERA_RESET_METHOD, json::object(), [this](json &reply) {
          after(reply);
          std::lock_guard<std::mutex> lock(lv_lock);
          shown();
        });
      });
  }
}
