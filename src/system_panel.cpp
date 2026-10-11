#include "system_panel.h"
#include "config.h"
#include "nebulaos_display.h"
#include "nebulaos_system.h"
#include "settings_ui.h"
#include "utils.h"
#include "spdlog/spdlog.h"

#include <vector>

LV_IMG_DECLARE(back);
LV_IMG_DECLARE(logs_img);
LV_IMG_DECLARE(refresh_img);
LV_IMG_DECLARE(power_img);

#ifdef GUPPYSCREEN_VERSION
#define GS_VERSION GUPPYSCREEN_VERSION
#else
#define GS_VERSION "dev-snapshot"
#endif

static const std::vector<std::string> log_levels = {"trace", "debug", "info", "warn"};

SystemPanel::SystemPanel(KWebSocketClient &c, std::mutex &lock)
  : ws(c)
  , lv_lock(lock)
  , cont(SettingsUi::page("System", &body))
  , back_btn(cont, &back, "Back", &SystemPanel::_handle_callback, this)
  , logs_panel(c, lock)
{
  lv_obj_t *left = SettingsUi::column(body, 60);
  lv_obj_t *right = SettingsUi::column(body, 37);

  info_label = lv_label_create(left);
  lv_obj_set_width(info_label, LV_PCT(100));
  lv_label_set_long_mode(info_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(info_label, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_line_space(info_label, 3, 0);
  lv_label_set_text(info_label, "Loading...");

  lv_obj_t *ll_row = SettingsUi::row(left, "Screen log level");
  loglevel_dd = lv_dropdown_create(ll_row);
  lv_dropdown_set_options(loglevel_dd, "Trace\nDebug\nInfo\nWarn");
  lv_obj_set_width(loglevel_dd, 90);
  lv_obj_align(loglevel_dd, LV_ALIGN_RIGHT_MID, 0, 0);
  Config *conf = Config::get_instance();
  uint32_t level = 1;
  auto df = conf->get_json("/default_printer");
  if (!df.empty()) {
    auto v = conf->get_json(conf->df() + "log_level");
    if (v.is_string()) {
      auto it = std::find(log_levels.begin(), log_levels.end(), v.template get<std::string>());
      if (it != log_levels.end()) {
        level = std::distance(log_levels.begin(), it);
      }
    }
  }
  lv_dropdown_set_selected(loglevel_dd, level);
  lv_obj_add_event_cb(loglevel_dd, &SystemPanel::_handle_callback, LV_EVENT_VALUE_CHANGED, this);

  logs_btn = SettingsUi::icon_button(right, &logs_img, "Logs", &SystemPanel::_handle_callback, this);
  restart_gui_btn = SettingsUi::icon_button(right, &refresh_img, "Restart GUI",
                                            &SystemPanel::_handle_callback, this);
  // Reboot and Shut down side by side, clear of the Back button below.
  lv_obj_t *power_row = lv_obj_create(right);
  lv_obj_set_size(power_row, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(power_row, 0, 0);
  lv_obj_set_style_pad_column(power_row, 6, 0);
  lv_obj_set_style_border_width(power_row, 0, 0);
  lv_obj_set_style_bg_opa(power_row, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(power_row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(power_row, LV_FLEX_FLOW_ROW);
  reboot_btn = SettingsUi::icon_button(power_row, &refresh_img, "Reboot", &SystemPanel::_handle_callback, this);
  shutdown_btn = SettingsUi::icon_button(power_row, &power_img, "Shut down", &SystemPanel::_handle_callback, this);
  for (lv_obj_t *b : {reboot_btn, shutdown_btn}) {
    lv_obj_set_width(b, 0);
    lv_obj_set_flex_grow(b, 1);
    lv_obj_set_height(b, 48);
    lv_obj_set_style_text_font(lv_obj_get_child(b, 1), &lv_font_montserrat_10, 0);
  }
  SettingsUi::set_tone(shutdown_btn, SettingsUi::Tone::Danger);

  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_FLOATING);
  lv_obj_align(back_btn.get_container(), LV_ALIGN_BOTTOM_RIGHT, 0, 0);
}

SystemPanel::~SystemPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void SystemPanel::foreground() {
  lv_obj_move_foreground(cont);
  refresh();
}

void SystemPanel::report_error(json &reply, const char *what) {
  auto err = jsonrpc_error_message(reply);
  if (err.empty()) {
    return;
  }
  spdlog::warn("system panel: {}: {}", what, err);
  std::lock_guard<std::mutex> lock(lv_lock);
  KUtils::notify_toast(std::string(what) + ": " + err, 5000);
}

// Called on the LVGL thread; replies arrive on the websocket thread.
void SystemPanel::refresh() {
  ws.send_jsonrpc(SYSTEM_INFO_METHOD, json::object(), [this](json &reply) {
    std::lock_guard<std::mutex> lock(lv_lock);
    show_info(reply);
  });
}

// LVGL lock held by the caller.
void SystemPanel::show_info(const json &reply) {
  auto info = system_info(reply);
  std::string text = info.text;
  auto nl = text.find('\n');
  text.insert(nl == std::string::npos ? text.size() : nl, "\nGuppyScreen " GS_VERSION);
  lv_label_set_text(info_label, text.c_str());
  if (info.storage_warning) {
    lv_obj_set_style_text_color(info_label, lv_palette_main(LV_PALETTE_ORANGE), 0);
  } else {
    lv_obj_remove_local_style_prop(info_label, LV_STYLE_TEXT_COLOR, 0);
  }
}

#ifdef SIMULATOR
void SystemPanel::sim_fill() {
  show_info(json::parse(R"({"result":{"build":{"available":true,"build_date":"2026-10-11T02:43:29Z",
    "firmware_head":"df415dc0000000000000000000000000000000000"},"slot":"B","hostname":"nebulaos",
    "kernel":{"release":"6.6.157-rt79"},"storage":{"free_bytes":1766465536,"total_bytes":6314295296,
    "read_only":false,"boot_check":{"result":"CLEAN"}}}})"));
}
#endif

void SystemPanel::restart_gui() {
  // Through Moonraker (GUI_BACKEND=MOONRAKER_ONLY): its supervisorctl shim runs
  // /etc/init.d/S58guppyscreen restart; NebulaOS allows the service in
  // moonraker.asvc. On success this process is stopped and restarted.
  json params = {{"service", "guppyscreen"}};
  ws.send_jsonrpc("machine.services.restart", params, [this](json &reply) {
    report_error(reply, "Restart failed");
  });
}

void SystemPanel::confirm_power(bool reboot) {
  if (KUtils::is_printing()) {
    KUtils::notify_locked();
    return;
  }
  SettingsUi::confirm(
    reboot ? "Reboot" : "Shut down",
    reboot ? "Reboot the printer now?"
           : "Shut the printer down now?\n\nSwitch it off at the power switch afterwards.",
    reboot ? "Reboot" : "Shut down",
    reboot ? SettingsUi::Tone::Normal : SettingsUi::Tone::Danger,
    [this, reboot]() {
      // Re-check: a print may have started while the dialog was open.
      if (KUtils::is_printing()) {
        KUtils::notify_locked();
        return;
      }
      ws.send_jsonrpc(reboot ? "machine.reboot" : "machine.shutdown", json::object(),
        [this, reboot](json &reply) {
          report_error(reply, reboot ? "Reboot failed" : "Shutdown failed");
        });
      KUtils::notify_toast(reboot ? "Rebooting..." : "Shutting down...", 3000);
    });
}

void SystemPanel::handle_callback(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *obj = lv_event_get_current_target(e);

  if (code == LV_EVENT_VALUE_CHANGED && obj == loglevel_dd) {
    auto idx = lv_dropdown_get_selected(loglevel_dd);
    if (idx < log_levels.size()) {
      auto ll = spdlog::level::from_str(log_levels[idx]);
      spdlog::set_level(ll);
      spdlog::flush_on(ll);
      Config *conf = Config::get_instance();
      conf->set<std::string>(conf->df() + "log_level", log_levels[idx]);
      conf->save();
    }
    return;
  }
  if (code != LV_EVENT_CLICKED) {
    return;
  }
  if (obj == back_btn.get_container()) {
    lv_obj_move_background(cont);
  } else if (obj == logs_btn) {
    logs_panel.foreground();
  } else if (obj == restart_gui_btn) {
    restart_gui();
  } else if (obj == reboot_btn) {
    confirm_power(true);
  } else if (obj == shutdown_btn) {
    confirm_power(false);
  }
}
