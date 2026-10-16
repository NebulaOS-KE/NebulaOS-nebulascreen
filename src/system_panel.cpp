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


SystemPanel::SystemPanel(KWebSocketClient &c, std::mutex &lock)
  : ws(c)
  , lv_lock(lock)
  , cont(SettingsUi::page("System", &body))
  , back_btn(cont, &back, "Back", &SystemPanel::_handle_callback, this)
  , logs_panel(c, lock)
{
  SettingsUi::on_shown(cont, [this]() { shown(); });
  lv_obj_t *left = SettingsUi::column(body, 60);
  lv_obj_t *right = SettingsUi::column(body, 37);

  info_label = lv_label_create(left);
  lv_obj_set_width(info_label, LV_PCT(100));
  lv_label_set_long_mode(info_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(info_label, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_line_space(info_label, 3, 0);
  lv_label_set_text(info_label, "Loading...");

  // G60 (2026-10-16): one general log level for NebulaOS, Moonraker and the
  // screen (Klipper has none); NebulaOS stores it, shown() reads it back.
  lv_obj_t *ll_row = SettingsUi::row(left, "Log level");
  loglevel_dd = lv_dropdown_create(ll_row);
  lv_dropdown_set_options(loglevel_dd, LOG_LEVEL_OPTIONS);
  lv_obj_set_width(loglevel_dd, 110);
  lv_obj_align(loglevel_dd, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_add_event_cb(loglevel_dd, &SystemPanel::_handle_callback, LV_EVENT_VALUE_CHANGED, this);

  logs_btn = SettingsUi::icon_button(right, &logs_img, "Logs", &SystemPanel::_handle_callback, this);
  restart_gui_btn = SettingsUi::icon_button(right, &refresh_img, "Restart GUI",
                                            &SystemPanel::_handle_callback, this);
  // Four full-width buttons, 34 px high, so the column ends above the Back
  // button and "Shut down" fits on one line (G9).
  reboot_btn = SettingsUi::icon_button(right, &refresh_img, "Reboot", &SystemPanel::_handle_callback, this);
  shutdown_btn = SettingsUi::icon_button(right, &power_img, "Shut down", &SystemPanel::_handle_callback, this);
  for (lv_obj_t *b : {logs_btn, restart_gui_btn, reboot_btn, shutdown_btn}) {
    lv_obj_set_height(b, 34);
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
}

// Every time the page comes on top (SettingsUi::on_shown).
void SystemPanel::shown() {
  refresh();
  ws.send_jsonrpc(LOGS_STATUS_METHOD, json::object(), [this](json &reply) {
    if (reply.contains("error")) return;
    std::lock_guard<std::mutex> lock(lv_lock);
    updating_level = true;
    lv_dropdown_set_selected(loglevel_dd, log_level_index(reply));
    updating_level = false;
  });
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
    if (updating_level) return;
    std::string level = log_level_at(lv_dropdown_get_selected(loglevel_dd));
    if (level.empty()) return;
    // The screen's own log follows at once; NebulaOS applies the rest.
    std::string screen = log_level_spdlog(level);
    auto ll = spdlog::level::from_str(screen);
    spdlog::set_level(ll);
    spdlog::flush_on(ll);
    Config *conf = Config::get_instance();
    conf->set<std::string>(conf->df() + "log_level", screen);
    conf->save();
    ws.send_jsonrpc(LOGS_LEVEL_METHOD, json{{"level", level}}, [this](json &reply) {
      report_error(reply, "Log level not changed");
    });
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
