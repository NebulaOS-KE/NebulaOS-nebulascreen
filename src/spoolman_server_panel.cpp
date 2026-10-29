#include "spoolman_server_panel.h"
#include "nebulaos_settings.h"
#include "settings_ui.h"
#include "utils.h"
#include "spdlog/spdlog.h"

static lv_obj_t *small_button(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *user) {
  lv_obj_t *b = lv_btn_create(parent);
  lv_obj_set_size(b, 80, 36);
  lv_obj_t *l = lv_label_create(b);
  lv_label_set_text(l, text);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
  lv_obj_center(l);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
  return b;
}

static lv_obj_t *plain_row(lv_obj_t *parent) {
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_style_pad_column(row, 6, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  return row;
}

static lv_obj_t *text_input(lv_obj_t *parent, const char *placeholder, uint32_t max_len,
                            void *user) {
  lv_obj_t *ta = lv_textarea_create(parent);
  lv_textarea_set_one_line(ta, true);
  lv_textarea_set_max_length(ta, max_len);
  lv_textarea_set_placeholder_text(ta, placeholder);
  lv_obj_add_event_cb(ta, &SpoolmanServerPanel::_handle_callback, LV_EVENT_VALUE_CHANGED, user);
  lv_obj_add_event_cb(ta, &SpoolmanServerPanel::_handle_callback, LV_EVENT_FOCUSED, user);
  lv_obj_add_event_cb(ta, &SpoolmanServerPanel::_handle_callback, LV_EVENT_READY, user);
  lv_obj_add_event_cb(ta, &SpoolmanServerPanel::_handle_callback, LV_EVENT_CANCEL, user);
  return ta;
}

SpoolmanServerPanel::SpoolmanServerPanel(KWebSocketClient &c, std::mutex &lock)
  : ws(c)
  , lv_lock(lock)
  , cont(SettingsUi::page("Spoolman server", &body))
{
  lv_obj_t *col = SettingsUi::column(body, 100);
  lv_obj_set_style_pad_row(col, 4, 0);
  SettingsUi::hint(col, "The Spoolman server the printer uses for spool tracking. "
                        "Moonraker restarts after a change.");
  current = SettingsUi::info_line(col, "Now");

  lv_obj_t *row = plain_row(col);
  host_input = text_input(row, "IP address or name", 253, this);
  lv_textarea_set_accepted_chars(host_input, "abcdefghijklmnopqrstuvwxyz"
                                             "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-");
  lv_obj_set_flex_grow(host_input, 1);
  port_input = text_input(row, "Port", 5, this);
  lv_textarea_set_accepted_chars(port_input, "0123456789");
  lv_obj_set_width(port_input, 90);

  row = plain_row(col);
  save_btn = small_button(row, "Save", &SpoolmanServerPanel::_handle_callback, this);
  remove_btn = small_button(row, "Remove", &SpoolmanServerPanel::_handle_callback, this);
  lv_obj_set_style_bg_color(remove_btn, lv_palette_darken(LV_PALETTE_RED, 2), 0);
  cancel_btn = small_button(row, "Cancel", &SpoolmanServerPanel::_handle_callback, this);
  lv_obj_set_style_bg_color(cancel_btn, lv_palette_darken(LV_PALETTE_GREY, 3), 0);

  message = lv_label_create(col);
  lv_obj_set_width(message, LV_PCT(100));
  lv_label_set_long_mode(message, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(message, &lv_font_montserrat_12, 0);
  lv_label_set_text(message, "");

  kb = lv_keyboard_create(cont);
  lv_obj_add_flag(kb, LV_OBJ_FLAG_FLOATING);
  lv_obj_set_size(kb, LV_PCT(100), 136);
  lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
  lv_keyboard_set_textarea(kb, host_input);
}

SpoolmanServerPanel::~SpoolmanServerPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

// LVGL lock held by the caller (Connections button).
void SpoolmanServerPanel::foreground() {
  lv_label_set_text(current, "...");
  lv_textarea_set_text(host_input, "");
  lv_textarea_set_text(port_input, std::to_string(SPOOLMAN_DEFAULT_PORT).c_str());
  lv_obj_add_state(host_input, LV_STATE_FOCUSED);
  lv_keyboard_set_textarea(kb, host_input);
  lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);   // names too; "1#" has the digits
  lv_obj_move_foreground(cont);
  validate();
  ws.send_jsonrpc(SPOOLMAN_STATUS_METHOD, json::object(), [this](json &reply) {
    auto v = spoolman_view(reply);
    std::lock_guard<std::mutex> lock(lv_lock);
    configured = v.configured;
    lv_label_set_text(current, v.configured ? v.server.c_str() : "not configured");
    if (v.configured && !v.host.empty()) {
      lv_textarea_set_text(host_input, v.host.c_str());
      if (v.port > 0) lv_textarea_set_text(port_input, std::to_string(v.port).c_str());
    }
    validate();
  });
}

void SpoolmanServerPanel::set_message(const std::string &text, bool error) {
  lv_label_set_text(message, text.c_str());
  if (error) {
    lv_obj_set_style_text_color(message, lv_palette_main(LV_PALETTE_ORANGE), 0);
  } else {
    lv_obj_remove_local_style_prop(message, LV_STYLE_TEXT_COLOR, 0);
  }
}

// LVGL lock held by the caller.
void SpoolmanServerPanel::validate() {
  auto c = spoolman_check(lv_textarea_get_text(host_input), lv_textarea_get_text(port_input));
  bool empty = std::string(lv_textarea_get_text(host_input)).empty();
  set_message(c.message, !c.ok && !empty);
  if (c.ok) lv_obj_clear_state(save_btn, LV_STATE_DISABLED);
  else lv_obj_add_state(save_btn, LV_STATE_DISABLED);
  if (configured) lv_obj_clear_state(remove_btn, LV_STATE_DISABLED);
  else lv_obj_add_state(remove_btn, LV_STATE_DISABLED);
}

void SpoolmanServerPanel::save() {
  auto c = spoolman_check(lv_textarea_get_text(host_input), lv_textarea_get_text(port_input));
  if (!c.ok) {
    validate();
    return;
  }
  std::string url = "http://" + c.host + ":" + std::to_string(c.port);
  std::string text = "Use the Spoolman server " + url + "?\n\nMoonraker restarts; the screen "
                     "reconnects in a few seconds.";
  SettingsUi::confirm("Spoolman server", text.c_str(), "Save", SettingsUi::Tone::Normal,
                      [this, c]() {
    set_message("Saving...", false);
    lv_obj_add_state(save_btn, LV_STATE_DISABLED);
    ws.send_jsonrpc(SPOOLMAN_SERVER_METHOD, spoolman_params(c.host, c.port),
                    [this](json &reply) { done(reply, "Spoolman server saved"); });
  });
}

void SpoolmanServerPanel::remove() {
  SettingsUi::confirm("Spoolman server", "Stop using Spoolman on this printer?\n\nMoonraker "
                      "restarts; the screen reconnects in a few seconds.", "Remove",
                      SettingsUi::Tone::Danger, [this]() {
    set_message("Removing...", false);
    ws.send_jsonrpc(SPOOLMAN_REMOVE_METHOD, json::object(),
                    [this](json &reply) { done(reply, "Spoolman removed"); });
  });
}

// Websocket thread.
void SpoolmanServerPanel::done(const json &reply, const std::string &what) {
  std::string err = spoolman_error_text(reply);
  std::lock_guard<std::mutex> lock(lv_lock);
  if (!err.empty()) {
    set_message(err, true);
    validate();
    return;
  }
  lv_obj_move_background(cont);
  if (!spoolman_restart_needed(reply)) {
    KUtils::notify_toast("Nothing changed", 3000);
    return;
  }
  KUtils::notify_toast(what + " - restarting Moonraker...", 4000);
  // The reply may never arrive: Moonraker restarts itself (as after a rename).
  ws.send_jsonrpc(SERVICE_RESTART_METHOD, moonraker_restart_params(), [](json &r) {
    auto e = hostname_error_text(r);
    if (!e.empty()) spdlog::warn("moonraker restart after the Spoolman change: {}", e);
  });
}

void SpoolmanServerPanel::handle_callback(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *obj = lv_event_get_current_target(e);
  if (obj == host_input || obj == port_input) {
    if (code == LV_EVENT_FOCUSED) {
      lv_keyboard_set_textarea(kb, obj);
      lv_keyboard_set_mode(kb, obj == port_input ? LV_KEYBOARD_MODE_NUMBER
                                                 : LV_KEYBOARD_MODE_TEXT_LOWER);
    } else if (code == LV_EVENT_VALUE_CHANGED) {
      validate();
    } else if (code == LV_EVENT_READY) {
      save();
    } else if (code == LV_EVENT_CANCEL) {
      lv_obj_move_background(cont);
    }
    return;
  }
  if (code != LV_EVENT_CLICKED) return;
  if (obj == save_btn) {
    save();
  } else if (obj == remove_btn) {
    remove();
  } else if (obj == cancel_btn) {
    lv_obj_move_background(cont);
  }
}

#ifdef SIMULATOR
void SpoolmanServerPanel::sim_fill(const char *host, const char *port) {
  lv_textarea_set_text(host_input, host);
  lv_textarea_set_text(port_input, port);
  validate();
}
#endif
