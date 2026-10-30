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
  lv_obj_add_event_cb(ta, &SpoolmanServerPanel::_handle_callback, LV_EVENT_CLICKED, user);
  lv_obj_add_event_cb(ta, &SpoolmanServerPanel::_handle_callback, LV_EVENT_READY, user);
  lv_obj_add_event_cb(ta, &SpoolmanServerPanel::_handle_callback, LV_EVENT_CANCEL, user);
  return ta;
}

SpoolmanServerPanel::SpoolmanServerPanel(KWebSocketClient &c, std::mutex &lock)
  : ws(c)
  , lv_lock(lock)
  , cont(SettingsUi::page("Spoolman server", &body))
{
  // Laid out like Printers: a new server on the left, the one in use on the right.
  lv_obj_t *left = SettingsUi::column(body, 52);
  lv_obj_set_style_pad_row(left, 8, 0);
  SettingsUi::hint(left, "Set or change the server. Moonraker restarts.");
  // Above the fields: the keyboard covers the lower half while typing.
  message = lv_label_create(left);
  lv_obj_set_width(message, LV_PCT(100));
  lv_label_set_long_mode(message, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(message, &lv_font_montserrat_10, 0);
  lv_label_set_text(message, "");

  host_input = text_input(left, "Server IP address or name", 253, this);
  lv_obj_set_width(host_input, LV_PCT(100));
  lv_textarea_set_accepted_chars(host_input, "abcdefghijklmnopqrstuvwxyz"
                                             "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-");

  lv_obj_t *row = plain_row(left);
  port_input = text_input(row, "Port", 5, this);
  lv_textarea_set_accepted_chars(port_input, "0123456789");
  lv_obj_set_width(port_input, 90);
  save_btn = small_button(row, LV_SYMBOL_PLUS " Use", &SpoolmanServerPanel::_handle_callback, this);

  lv_obj_t *right = SettingsUi::column(body, 46);
  lv_obj_set_height(right, LV_SIZE_CONTENT);   // clear of the Back tile
  lv_obj_set_style_border_width(right, 2, 0);
  lv_obj_set_style_border_opa(right, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(right, 6, 0);
  lv_obj_t *t = lv_label_create(right);
  lv_label_set_text(t, "In use");
  lv_obj_set_style_text_font(t, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(t, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);

  none_label = lv_label_create(right);
  lv_obj_set_width(none_label, LV_PCT(100));
  lv_label_set_long_mode(none_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(none_label, &lv_font_montserrat_12, 0);

  card = lv_obj_create(right);
  lv_obj_set_size(card, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(card, 4, 0);
  lv_obj_set_style_pad_left(card, 8, 0);
  lv_obj_set_style_border_width(card, 2, 0);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  card_label = lv_label_create(card);
  lv_obj_set_flex_grow(card_label, 1);
  lv_label_set_long_mode(card_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(card_label, &lv_font_montserrat_12, 0);
  remove_btn = lv_btn_create(card);
  lv_obj_set_size(remove_btn, 28, 28);
  lv_obj_set_style_bg_color(remove_btn, lv_palette_darken(LV_PALETTE_RED, 2), 0);
  lv_obj_t *x = lv_label_create(remove_btn);
  lv_label_set_text(x, LV_SYMBOL_CLOSE);
  lv_obj_center(x);
  lv_obj_add_event_cb(remove_btn, &SpoolmanServerPanel::_handle_callback, LV_EVENT_CLICKED, this);

  back_btn = SettingsUi::back_tile(cont, &SpoolmanServerPanel::_handle_callback, this);

  kb = lv_keyboard_create(cont);
  lv_obj_add_flag(kb, LV_OBJ_FLAG_FLOATING);
  lv_obj_set_size(kb, LV_PCT(100), 136);
  lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
  hide_keyboard();
  show_current("", true);
}

SpoolmanServerPanel::~SpoolmanServerPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

// LVGL lock held by the caller (Connections button).
void SpoolmanServerPanel::foreground(const std::string &known) {
  lv_textarea_set_text(host_input, "");
  lv_textarea_set_text(port_input, std::to_string(SPOOLMAN_DEFAULT_PORT).c_str());
  hide_keyboard();
  configured = !known.empty();
  show_current(known, known.empty());
  lv_obj_move_foreground(cont);
  validate();
  ws.send_jsonrpc(SPOOLMAN_STATUS_METHOD, json::object(), [this](json &reply) {
    auto v = spoolman_view(reply);
    std::lock_guard<std::mutex> lock(lv_lock);
    configured = v.configured;
    show_current(v.configured ? v.server : "", false);
    validate();
  });
}

// LVGL lock held by the caller. loading: nothing known yet, the status is on its way.
void SpoolmanServerPanel::show_current(const std::string &server, bool loading) {
  if (!server.empty()) {
    lv_label_set_text(card_label, spoolman_card_text(server).c_str());
    lv_obj_clear_flag(card, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(none_label, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_label_set_text(none_label, loading ? "Reading..." : "No server set");
  lv_obj_clear_flag(none_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(card, LV_OBJ_FLAG_HIDDEN);
}

void SpoolmanServerPanel::show_keyboard(lv_obj_t *ta) {
  lv_keyboard_set_textarea(kb, ta);
  lv_keyboard_set_mode(kb, ta == port_input ? LV_KEYBOARD_MODE_NUMBER
                                            : LV_KEYBOARD_MODE_TEXT_LOWER);
  lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(kb);
}

void SpoolmanServerPanel::hide_keyboard() {
  lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_state(host_input, LV_STATE_FOCUSED);
  lv_obj_clear_state(port_input, LV_STATE_FOCUSED);
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
  set_message(empty || c.ok ? "" : c.message, true);   // only what is wrong
  if (c.ok) lv_obj_clear_state(save_btn, LV_STATE_DISABLED);
  else lv_obj_add_state(save_btn, LV_STATE_DISABLED);
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
    // CLICKED too: a second tap on a field that kept its focus brings the keyboard back.
    if (code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED) {
      show_keyboard(obj);
    } else if (code == LV_EVENT_VALUE_CHANGED) {
      validate();
    } else if (code == LV_EVENT_READY) {
      hide_keyboard();
      save();
    } else if (code == LV_EVENT_CANCEL) {
      hide_keyboard();
    }
    return;
  }
  if (code != LV_EVENT_CLICKED) return;
  if (obj == save_btn) {
    hide_keyboard();
    save();
  } else if (obj == remove_btn) {
    remove();
  } else if (obj == back_btn) {
    hide_keyboard();
    lv_obj_move_background(cont);
  }
}

#ifdef SIMULATOR
void SpoolmanServerPanel::sim_fill(const char *host, const char *port) {
  lv_textarea_set_text(host_input, host);
  lv_textarea_set_text(port_input, port);
  show_keyboard(host_input);
  validate();
}

void SpoolmanServerPanel::sim_none() {
  configured = false;
  show_current("", false);
}
#endif
