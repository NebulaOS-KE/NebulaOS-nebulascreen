#include "logs_panel.h"
#include "settings_ui.h"
#include "nebulaos_system.h"
#include "utils.h"
#include "spdlog/spdlog.h"

#include <fstream>

LV_IMG_DECLARE(back);

static const size_t LOG_TAIL_BYTES = 12 * 1024;
static const char *WIFI_ENTRY = "Wi-Fi";
static const char *SAVE_ENTRY = "Save logs to USB";

LogsPanel::LogsPanel(KWebSocketClient &c, std::mutex &lock)
  : ws(c)
  , lv_lock(lock)
  , cont(lv_obj_create(lv_scr_act()))
  , list(lv_list_create(cont))
  , text_cont(lv_obj_create(cont))
  , text(lv_label_create(text_cont))
  , back_btn(cont, &back, "Back", &LogsPanel::_handle_callback, this)
{
  SettingsUi::on_shown(cont, [this]() { shown(); });
  lv_obj_move_background(cont);
  lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_pad_all(cont, 4, 0);
  lv_obj_set_style_pad_column(cont, 4, 0);
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);

  lv_obj_set_size(list, LV_PCT(32), LV_PCT(100));

  lv_obj_set_height(text_cont, LV_PCT(100));
  lv_obj_set_flex_grow(text_cont, 1);
  lv_obj_set_style_pad_all(text_cont, 4, 0);
  lv_obj_set_scroll_dir(text_cont, LV_DIR_VER);

  lv_obj_set_width(text, LV_PCT(100));
  lv_label_set_long_mode(text, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(text, &lv_font_montserrat_10, 0);
  lv_label_set_text(text, "Select a log file.");

  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_FLOATING);
  lv_obj_align(back_btn.get_container(), LV_ALIGN_BOTTOM_RIGHT, 0, 0);
}

LogsPanel::~LogsPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void LogsPanel::foreground() {
  lv_obj_move_foreground(cont);
}

// Every time the page comes on top (SettingsUi::on_shown).
void LogsPanel::shown() {
  json params = {{"root", "logs"}};
  ws.send_jsonrpc("server.files.list", params, [this](json &reply) {
    auto names = log_files(reply);
    std::lock_guard<std::mutex> lock(lv_lock);
    files = names;
    lv_obj_clean(list);
    lv_obj_t *s = lv_list_add_btn(list, LV_SYMBOL_USB, SAVE_ENTRY);
    lv_obj_add_event_cb(s, &LogsPanel::_handle_callback, LV_EVENT_CLICKED, this);
    lv_obj_t *w = lv_list_add_btn(list, LV_SYMBOL_WIFI, WIFI_ENTRY);
    lv_obj_add_event_cb(w, &LogsPanel::_handle_callback, LV_EVENT_CLICKED, this);
    if (files.empty()) {
      lv_label_set_text(text, "No log files found.");
      return;
    }
    for (size_t i = 0; i < files.size(); ++i) {
      lv_obj_t *b = lv_list_add_btn(list, LV_SYMBOL_FILE, files[i].c_str());
      lv_obj_add_event_cb(b, &LogsPanel::_handle_callback, LV_EVENT_CLICKED, this);
    }
  });
}

// The newest LOG_TAIL_BYTES of a log, read from the local logs root.
void LogsPanel::show_file(const std::string &name) {
  if (!KUtils::is_running_local()) {
    lv_label_set_text(text, "Logs can be viewed on the printer's own screen, or in Mainsail.");
    return;
  }
  std::string root = KUtils::get_root_path("logs");
  std::ifstream f(root + "/" + name, std::ios::binary);
  if (root.empty() || !f) {
    lv_label_set_text(text, ("Cannot open " + name).c_str());
    return;
  }
  f.seekg(0, std::ios::end);
  std::streamoff size = f.tellg();
  std::streamoff start = size > (std::streamoff)LOG_TAIL_BYTES ? size - (std::streamoff)LOG_TAIL_BYTES : 0;
  f.seekg(start);
  std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  std::string shown = tail_text(content, LOG_TAIL_BYTES);
  if (start > 0) {
    shown = "... (last " + std::to_string(LOG_TAIL_BYTES / 1024) + " KB of " + name + ")\n" + shown;
  }
  lv_label_set_text(text, shown.c_str());
  lv_obj_update_layout(text_cont);
  lv_obj_scroll_to_y(text_cont, LV_COORD_MAX, LV_ANIM_OFF);
}

// G59: the Wi-Fi and DHCP lines of system.log.0 and system.log, newest last.
void LogsPanel::show_wifi() {
  if (!KUtils::is_running_local()) {
    lv_label_set_text(text, "Logs can be viewed on the printer's own screen, or in Mainsail.");
    return;
  }
  std::string root = KUtils::get_root_path("logs");
  std::string all;
  for (const char *name : {"system.log.0", "system.log"}) {
    std::ifstream f(root + "/" + name, std::ios::binary);
    if (!root.empty() && f) {
      all.append((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    }
  }
  std::string lines = wifi_log_lines(all);
  if (lines.empty()) {
    lv_label_set_text(text, "No Wi-Fi events in system.log yet.");
    return;
  }
  std::string shown = tail_text(lines, LOG_TAIL_BYTES);
  if (shown.size() < lines.size()) {
    shown = "... (last " + std::to_string(LOG_TAIL_BYTES / 1024) + " KB of the Wi-Fi lines)\n" + shown;
  }
  lv_label_set_text(text, shown.c_str());
  lv_obj_update_layout(text_cont);
  lv_obj_scroll_to_y(text_cont, LV_COORD_MAX, LV_ANIM_OFF);
}

// G60 (2026-10-16): every log, the state file and the config in one zip on
// the USB stick, for a problem report.
void LogsPanel::save_to_usb() {
  SettingsUi::confirm("Save logs to USB",
    "All logs, the printer's state and its config files go into one zip file on the USB "
    "stick (passwords are left out). Takes up to a minute.",
    "Save", SettingsUi::Tone::Normal, [this]() {
      lv_label_set_text(text, "Saving the logs to the USB stick...");
      ws.send_jsonrpc(LOGS_SAVE_METHOD, json::object(), [this](json &reply) {
        bool ok = false;
        std::string msg = logs_save_text(reply, &ok);
        std::lock_guard<std::mutex> lock(lv_lock);
        lv_label_set_text(text, msg.c_str());
        SettingsUi::notice(ok ? "Logs saved" : "Logs not saved", msg.c_str());
      });
    });
}

void LogsPanel::handle_callback(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
    return;
  }
  lv_obj_t *btn = lv_event_get_current_target(e);
  if (btn == back_btn.get_container()) {
    lv_obj_move_background(cont);
    return;
  }
  const char *name = lv_list_get_btn_text(list, btn);
  if (name != nullptr && std::string(name) == SAVE_ENTRY) {
    save_to_usb();
  } else if (name != nullptr && std::string(name) == WIFI_ENTRY) {
    show_wifi();
  } else if (name != nullptr) {
    show_file(name);
  }
}
