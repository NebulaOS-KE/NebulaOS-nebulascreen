#include "hostname_panel.h"
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

HostnamePanel::HostnamePanel(KWebSocketClient &c, std::mutex &lock)
  : ws(c)
  , lv_lock(lock)
  , cont(SettingsUi::page("Printer name", &body))
{
  lv_obj_t *col = SettingsUi::column(body, 100);
  lv_obj_set_style_pad_row(col, 4, 0);
  SettingsUi::hint(col, "Other devices find the printer as <name>.local (Mainsail, "
                        "slicers). Its IP address keeps working.");

  lv_obj_t *row = lv_obj_create(col);
  lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_style_pad_column(row, 6, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  input = lv_textarea_create(row);
  lv_textarea_set_one_line(input, true);
  lv_textarea_set_max_length(input, 63);
  lv_textarea_set_placeholder_text(input, "e.g. ke-workshop");
  lv_obj_set_flex_grow(input, 1);
  lv_obj_add_state(input, LV_STATE_FOCUSED);   // cursor visible
  lv_obj_add_event_cb(input, &HostnamePanel::_handle_callback, LV_EVENT_VALUE_CHANGED, this);
  lv_obj_add_event_cb(input, &HostnamePanel::_handle_callback, LV_EVENT_READY, this);
  lv_obj_add_event_cb(input, &HostnamePanel::_handle_callback, LV_EVENT_CANCEL, this);
  lv_obj_add_event_cb(input, &HostnamePanel::_handle_callback, LV_EVENT_CLICKED, this);

  save_btn = small_button(row, "Save", &HostnamePanel::_handle_callback, this);
  cancel_btn = small_button(row, "Cancel", &HostnamePanel::_handle_callback, this);
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
  lv_keyboard_set_textarea(kb, input);
}

HostnamePanel::~HostnamePanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void HostnamePanel::foreground(const std::string &cur, const std::string &addr) {
  current = cur;
  address = addr;
  lv_textarea_set_text(input, cur.c_str());
  lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
  lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(cont);
  validate();
}

// LVGL lock held by the caller.
void HostnamePanel::validate() {
  auto c = hostname_check(lv_textarea_get_text(input), current);
  lv_label_set_text(message, c.message.c_str());
  bool error = !c.ok && c.message != "This is the current name.";
  if (error) {
    lv_obj_set_style_text_color(message, lv_palette_main(LV_PALETTE_ORANGE), 0);
  } else {
    lv_obj_remove_local_style_prop(message, LV_STYLE_TEXT_COLOR, 0);
  }
  if (c.ok) lv_obj_clear_state(save_btn, LV_STATE_DISABLED);
  else lv_obj_add_state(save_btn, LV_STATE_DISABLED);
}

void HostnamePanel::save() {
  auto c = hostname_check(lv_textarea_get_text(input), current);
  if (!c.ok) {
    validate();
    return;
  }
  std::string name = c.name;
  bool printing = KUtils::is_printing();
  std::string text = "Rename the printer to \"" + name + "\"?\n\nAfterwards it is reachable as " +
    name + ".local.\n" + (address.empty() ? "" : "Its IP address " + address + " keeps working.\n");
  text += printing
    ? "A print is running, so the new name is announced on the network after the next "
      "Moonraker restart or reboot."
    : "Moonraker restarts so the network learns the new name; the screen reconnects in a "
      "few seconds.";
  SettingsUi::confirm("Rename printer", text.c_str(), "Rename", SettingsUi::Tone::Normal,
                      [this, name]() { apply(name); });
}

void HostnamePanel::apply(const std::string &name) {
  lv_label_set_text(message, "Saving...");
  lv_obj_add_state(save_btn, LV_STATE_DISABLED);
  ws.send_jsonrpc(HOSTNAME_METHOD, hostname_params(name), [this, name](json &reply) {
    std::string err = hostname_error_text(reply);
    std::lock_guard<std::mutex> lock(lv_lock);
    if (!err.empty()) {
      lv_label_set_text(message, err.c_str());
      lv_obj_set_style_text_color(message, lv_palette_main(LV_PALETTE_ORANGE), 0);
      lv_obj_clear_state(save_btn, LV_STATE_DISABLED);
      return;
    }
    current = name;
    lv_obj_move_background(cont);
    if (hostname_restart_needed(reply) && !KUtils::is_printing()) {
      KUtils::notify_toast("Renamed to " + name + ".local - restarting Moonraker...", 4000);
      // The reply may never arrive: Moonraker restarts itself (no
      // moonraker.asvc entry needed for its own restart).
      ws.send_jsonrpc(SERVICE_RESTART_METHOD, moonraker_restart_params(), [](json &r) {
        auto e = hostname_error_text(r);
        if (!e.empty()) spdlog::warn("moonraker restart after rename: {}", e);
      });
    } else {
      KUtils::notify_toast("Renamed to " + name + ".local (announced after the next restart)", 4000);
    }
  });
}

void HostnamePanel::handle_callback(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *obj = lv_event_get_current_target(e);
  if (obj == input) {
    if (code == LV_EVENT_VALUE_CHANGED) {
      validate();
    } else if (code == LV_EVENT_READY) {
      save();
    } else if (code == LV_EVENT_CANCEL) {
      // The keyboard key hides the keyboard (the page stays); tap the field
      // to bring it back. Cancel leaves the page.
      lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    } else if (code == LV_EVENT_CLICKED) {
      lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }
    return;
  }
  if (code != LV_EVENT_CLICKED) return;
  if (obj == save_btn) {
    save();
  } else if (obj == cancel_btn) {
    lv_obj_move_background(cont);
  }
}

#ifdef SIMULATOR
void HostnamePanel::sim_type(const char *text) {
  lv_textarea_set_text(input, text);
  validate();
}

void HostnamePanel::sim_save() {
  save();
}
#endif
