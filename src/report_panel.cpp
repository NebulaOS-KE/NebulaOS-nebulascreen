#include "report_panel.h"
#include "config.h"
#include "nebulaos_display.h"
#include "nebulaos_system.h"
#include "settings_ui.h"
#include "utils.h"
#include "spdlog/spdlog.h"

#include <cstring>

LV_IMG_DECLARE(back);
LV_IMG_DECLARE(sd_img);

static lv_obj_t *step(lv_obj_t *parent, const char *text) {
  lv_obj_t *l = lv_label_create(parent);
  lv_obj_set_width(l, LV_PCT(100));
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
  lv_label_set_text(l, text);
  return l;
}

ReportPanel::ReportPanel(KWebSocketClient &c, std::mutex &lock)
  : ws(c)
  , lv_lock(lock)
  , cont(SettingsUi::page("Report a problem", &body))
  , back_btn(cont, &back, "Back", &ReportPanel::_handle_callback, this)
{
  SettingsUi::on_shown(cont, [this]() { shown(); });
  lv_obj_t *left = SettingsUi::column(body, 62);
  lv_obj_t *right = SettingsUi::column(body, 35);

  SettingsUi::hint(left, "Something not working? Help us fix it in three steps.");

  step(left, "1. Turn on detailed logging, then do what went wrong once more.");
  lv_obj_t *ll_row = SettingsUi::row(left, "Log level");
  loglevel_dd = lv_dropdown_create(ll_row);
  lv_dropdown_set_options(loglevel_dd, LOG_LEVEL_OPTIONS);
  lv_obj_set_width(loglevel_dd, 110);
  lv_obj_align(loglevel_dd, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_add_event_cb(loglevel_dd, &ReportPanel::_handle_callback, LV_EVENT_VALUE_CHANGED, this);
  debug_note_label = SettingsUi::hint(left, "Debug switches back to Info by itself after 24 hours.");

  step(left, "2. Save the logs to a USB stick (button on the right).");
  step(left, "3. Report it on GitHub - scan the code or open\n"
             "github.com/NebulaOS-KE/NebulaOS/issues\n"
             "Attach the zip and say what you did and what happened.");

#if LV_USE_QRCODE
  lv_obj_t *qr = lv_qrcode_create(right, 104, lv_color_black(), lv_color_white());
  lv_qrcode_update(qr, REPORT_ISSUES_URL, strlen(REPORT_ISSUES_URL));
  lv_obj_set_style_border_color(qr, lv_color_white(), 0);
  lv_obj_set_style_border_width(qr, 4, 0);
#endif
  save_btn = SettingsUi::icon_button(right, &sd_img, "Save logs to USB",
                                     &ReportPanel::_handle_callback, this);

  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_FLOATING);
  SettingsUi::place_back(back_btn.get_container());  // G94: one Back position
}

ReportPanel::~ReportPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void ReportPanel::foreground() {
  lv_obj_move_foreground(cont);
}

// Every time the page comes on top (SettingsUi::on_shown).
void ReportPanel::shown() {
  ws.send_jsonrpc(LOGS_STATUS_METHOD, json::object(), [this](json &reply) {
    if (reply.contains("error")) return;
    std::string note = debug_note(reply);
    std::lock_guard<std::mutex> lock(lv_lock);
    updating_level = true;
    lv_dropdown_set_selected(loglevel_dd, log_level_index(reply));
    updating_level = false;
    lv_label_set_text(debug_note_label, note.c_str());
  });
}

// LVGL thread.
void ReportPanel::set_level() {
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
    std::string err = jsonrpc_error_message(reply);
    std::lock_guard<std::mutex> lock(lv_lock);
    if (!err.empty()) {
      KUtils::notify_toast("Log level not changed: " + err, 5000);
      return;
    }
    shown();               // the "switches back at ..." line
  });
}

// LVGL thread. Every log, the state file and the config in one zip (G60).
void ReportPanel::save_logs() {
  if (saving) return;
  SettingsUi::confirm("Save logs to USB",
    "All logs, the printer's state and its config files go into one zip file on the USB "
    "stick (passwords are left out). Takes up to a minute.",
    "Save", SettingsUi::Tone::Normal, [this]() {
      saving = true;
      lv_obj_t *busy = SettingsUi::busy("Save logs to USB", "Saving the logs to the USB stick...");
      ws.send_jsonrpc(LOGS_SAVE_METHOD, json::object(), [this, busy](json &reply) {
        bool ok = false;
        std::string msg = logs_save_text(reply, &ok);
        std::lock_guard<std::mutex> lock(lv_lock);
        saving = false;
        SettingsUi::close_busy(busy);
        SettingsUi::notice(ok ? "Logs saved" : "Logs not saved", msg.c_str());
      });
    });
}

void ReportPanel::handle_callback(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *obj = lv_event_get_current_target(e);
  if (code == LV_EVENT_VALUE_CHANGED && obj == loglevel_dd) {
    if (!updating_level) set_level();
    return;
  }
  if (code != LV_EVENT_CLICKED) return;
  if (obj == back_btn.get_container()) {
    lv_obj_move_background(cont);
  } else if (obj == save_btn) {
    save_logs();
  }
}
