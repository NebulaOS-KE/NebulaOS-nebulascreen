#include "wifi_panel.h"
#include "nebulaos_display.h"
#include "utils.h"
#include "config.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <vector>
#include <utility>

LV_IMG_DECLARE(back);

WifiPanel::WifiPanel(KWebSocketClient &c, std::mutex &l)
  : ws(c)
  , lv_lock(l)
  , cont(lv_obj_create(lv_scr_act()))
  , spinner(lv_spinner_create(cont, 1000, 60))
  , top_cont(lv_obj_create(cont))
  , wifi_list_cont(lv_obj_create(top_cont))
  , wifi_right(lv_obj_create(top_cont))
  , prompt_cont(wifi_right)
  , wifi_label(lv_label_create(prompt_cont))
  , pw_row(lv_obj_create(prompt_cont))
  , password_input(lv_textarea_create(pw_row))
  , eye_btn(lv_btn_create(pw_row))
  , back_btn(cont, &back, "Back", &WifiPanel::_handle_back_btn, this)
  , kb(lv_keyboard_create(cont))
  , static_ip_panel(c, l)
  , network_detail_panel(l,
      [this]() { open_static_ip_for_current(); },
      [this]() { forget_current_network(); })
{
  lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_pad_all(cont, 0, 0);
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_add_flag(cont, LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_CLICKABLE);

  lv_obj_add_flag(spinner, LV_OBJ_FLAG_FLOATING);
  lv_obj_set_size(spinner, 80, 80);
  lv_obj_align(spinner, LV_ALIGN_CENTER, 0, 0);

  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_FLOATING);
  lv_obj_align(back_btn.get_container(), LV_ALIGN_BOTTOM_RIGHT, 0, -20);

  lv_obj_set_flex_grow(top_cont, 1);
  lv_obj_set_flex_flow(top_cont, LV_FLEX_FLOW_ROW);
  lv_obj_clear_flag(top_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_border_width(top_cont, 0, 0);
  lv_obj_set_width(top_cont, LV_PCT(100));

  // The list gets more than half the row.
  lv_obj_set_flex_grow(wifi_list_cont, 3);
  lv_obj_set_height(wifi_list_cont, LV_PCT(90));
  lv_obj_set_flex_flow(wifi_list_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(wifi_list_cont, 0, 0);
  lv_obj_set_style_border_width(wifi_list_cont, 0, 0);
  lv_obj_set_scroll_dir(wifi_list_cont, LV_DIR_VER);
  lv_obj_add_flag(wifi_list_cont, LV_OBJ_FLAG_HIDDEN);

  lv_obj_set_style_border_width(wifi_right, 0, 0);
  lv_obj_set_flex_grow(wifi_right, 2);
  lv_obj_add_flag(wifi_right, LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_CLICKABLE);

  lv_obj_add_flag(prompt_cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_size(prompt_cont, LV_PCT(100), LV_PCT(100));
  lv_obj_clear_flag(prompt_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_border_width(prompt_cont, 0, 0);

  lv_obj_set_width(wifi_label, LV_PCT(92));
  lv_label_set_long_mode(wifi_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(wifi_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(wifi_label, LV_ALIGN_TOP_MID, 0, 10);

  // pw_row: flex row holding the password textarea and the eye toggle button
  lv_obj_set_flex_flow(pw_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(pw_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_width(pw_row, LV_PCT(92));
  lv_obj_set_height(pw_row, LV_SIZE_CONTENT);
  lv_obj_set_style_border_width(pw_row, 0, 0);
  lv_obj_set_style_bg_opa(pw_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(pw_row, 0, 0);
  lv_obj_set_style_pad_column(pw_row, 4, 0);
  lv_obj_align(pw_row, LV_ALIGN_TOP_MID, 0, 40);
  lv_obj_add_flag(pw_row, LV_OBJ_FLAG_HIDDEN);

  lv_obj_set_flex_grow(password_input, 1);
  lv_obj_set_height(password_input, LV_SIZE_CONTENT);
  lv_textarea_set_password_mode(password_input, true);
  lv_textarea_set_one_line(password_input, true);

  lv_obj_set_size(eye_btn, 36, 36);
  lv_obj_set_style_pad_all(eye_btn, 0, 0);
  {
    lv_obj_t *lbl = lv_label_create(eye_btn);
    lv_label_set_text(lbl, LV_SYMBOL_EYE_CLOSE);
    lv_obj_center(lbl);
  }
  lv_obj_add_event_cb(eye_btn, &WifiPanel::_handle_eye_btn, LV_EVENT_CLICKED, this);

  lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(password_input, &WifiPanel::_handle_kb_input, LV_EVENT_FOCUSED, this);
  lv_obj_add_event_cb(password_input, &WifiPanel::_handle_kb_input, LV_EVENT_DEFOCUSED, this);
  lv_obj_add_event_cb(password_input, &WifiPanel::_handle_kb_input, LV_EVENT_READY, this);
  lv_obj_add_event_cb(password_input, &WifiPanel::_handle_kb_input, LV_EVENT_CANCEL, this);

  // allow clicks on non-clickables to hide the keyboard
  lv_obj_add_event_cb(prompt_cont, &WifiPanel::_handle_kb_input, LV_EVENT_CLICKED, this);
  lv_obj_add_event_cb(wifi_label, &WifiPanel::_handle_kb_input, LV_EVENT_CLICKED, this);

  lv_obj_move_background(cont);
  lv_obj_move_foreground(spinner);
}

WifiPanel::~WifiPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void WifiPanel::foreground() {
  spdlog::trace("wifi panel fg");
  lv_obj_move_foreground(cont);
  lv_obj_clear_flag(spinner, LV_OBJ_FLAG_HIDDEN);
  show_prompt("Scanning for networks...");
  // A second scan picks up access points that answered late.
  rescans_left = 1;
  scan();
}

void WifiPanel::show_prompt(const std::string &text) {
  lv_label_set_text(wifi_label, text.c_str());
  lv_obj_add_flag(pw_row, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(prompt_cont, LV_OBJ_FLAG_HIDDEN);
}

// Called on the LVGL thread; the reply arrives on the websocket thread.
void WifiPanel::scan() {
  ws.send_jsonrpc(WIFI_SCAN_METHOD, json::object(), [this](json &reply) {
    std::lock_guard<std::mutex> lock(lv_lock);
    show_scan(reply);
    if (rescans_left > 0 && jsonrpc_error_message(reply).empty()) {
      rescans_left--;
      scan();
    }
  });
}

void WifiPanel::show_scan(const json &reply) {
  lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);
  auto err = wifi_error_text(reply);
  if (!err.empty()) {
    if (!busy && !entering_password) {
      show_prompt("Wi-Fi scan failed.\n" + err);
    }
    return;
  }
  networks = wifi_networks(reply);
  auto current = wifi_current(reply);
  cur_network = current.ssid;
  cur_address = current.address;
  if (!busy && !entering_password) {
    show_prompt(cur_network.empty()
      ? std::string("Please select your Wi-Fi network")
      : "Connected to\n" + cur_network + "\nIP: " + (cur_address.empty() ? "-" : cur_address));
  }
  if (!entering_password) {
    rebuild_wifi_rows();
    lv_obj_scroll_to_y(wifi_list_cont, 0, LV_ANIM_OFF);
    lv_obj_clear_flag(wifi_list_cont, LV_OBJ_FLAG_HIDDEN);
  }
}

void WifiPanel::handle_back_btn(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED) {
    spdlog::trace("wifi panel bg");
    lv_obj_add_flag(wifi_list_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(prompt_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_background(cont);
  }
}

void WifiPanel::rebuild_wifi_rows() {
  wifi_rows.clear();
  lv_obj_clean(wifi_list_cont);

  auto make_group_label = [&](const char *text) {
    lv_obj_t *l = lv_label_create(wifi_list_cont);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(l, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);
    lv_obj_set_style_pad_top(l, 8, 0);
    lv_obj_set_style_pad_bottom(l, 2, 0);
    lv_obj_set_style_pad_left(l, 8, 0);
  };
  auto add_rows = [&](bool saved) {
    for (const auto &n : networks) {
      if (n.saved != saved || !wifi_supported(n)) continue;
      // The forget button removes by name (the "nid" slot carries the SSID).
      wifi_rows.push_back(std::make_unique<WifiRowItem>(
        wifi_list_cont, n.ssid, n.signal, n.current, n.saved, n.ssid,
        [this](const std::string &ssid) { handle_row_activated(ssid); },
        [this](const std::string &ssid, const std::string &nid) { confirm_remove_network(ssid, nid); }));
    }
  };
  bool any_saved = std::any_of(networks.begin(), networks.end(),
    [](const WifiNetwork &n) { return n.saved && wifi_supported(n); });
  bool any_other = std::any_of(networks.begin(), networks.end(),
    [](const WifiNetwork &n) { return !n.saved && wifi_supported(n); });
  if (any_saved) {
    make_group_label("KNOWN NETWORKS");
    add_rows(true);
  }
  if (any_other) {
    make_group_label("OTHER NETWORKS IN RANGE");
    add_rows(false);
  }
}

void WifiPanel::handle_row_activated(const std::string &ssid) {
  if (busy) {
    return;
  }
  auto it = std::find_if(networks.begin(), networks.end(),
    [&ssid](const WifiNetwork &n) { return n.ssid == ssid; });
  if (it == networks.end()) {
    return;
  }
  if (!cur_network.empty() && ssid == cur_network) {
    std::string security_text = it->security == "open" ? "Open (unsecured)" : "Secured (WPA/WPA2)";
    network_detail_panel.foreground(ssid, wifi_signal_text(it->signal), security_text,
                                    cur_address.empty() ? "-" : cur_address);
    return;
  }

  selected_network = ssid;
  if (!wifi_needs_password(*it)) {
    connect(ssid, "");
  } else {
    entering_password = true;
    lv_label_set_text(wifi_label, fmt::format("Enter password for {}", ssid).c_str());
    lv_obj_clear_flag(pw_row, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(prompt_cont, LV_OBJ_FLAG_HIDDEN);
    lv_event_send(password_input, LV_EVENT_FOCUSED, NULL);
  }
}

void WifiPanel::open_static_ip_for_current() {
  if (cur_network.empty()) return;
  static_ip_panel.foreground(cur_network);
}

void WifiPanel::forget_current_network() {
  if (cur_network.empty()) return;
  confirm_remove_network(cur_network, cur_network);
}

void WifiPanel::confirm_remove_network(const std::string &ssid, const std::string &nid) {
  static const char *btns[] = {"Remove", "Cancel", ""};
  std::string msg = fmt::format("Forget network \"{}\"?\nThis deletes its saved password.", ssid);
  lv_obj_t *mbox = lv_msgbox_create(NULL, NULL, msg.c_str(), btns, false);
  lv_obj_add_event_cb(mbox, [](lv_event_t *ev) {
    lv_obj_t *obj = lv_obj_get_parent(lv_event_get_target(ev));
    auto *self = static_cast<WifiPanel *>(lv_event_get_user_data(ev));
    bool confirmed = lv_msgbox_get_active_btn(obj) == 0;
    std::string ssid = self->pending_remove_ssid;
    self->pending_remove_ssid.clear();
    lv_msgbox_close(obj);
    if (!confirmed) return;

    self->busy = true;
    self->network_detail_panel.hide();
    self->show_prompt("Forgetting " + ssid + "...");
    lv_obj_clear_flag(self->spinner, LV_OBJ_FLAG_HIDDEN);
    self->ws.send_jsonrpc(WIFI_FORGET_METHOD, json{{"ssid", ssid}}, [self](json &reply) {
      std::lock_guard<std::mutex> lock(self->lv_lock);
      self->busy = false;
      auto err = wifi_error_text(reply);
      if (!err.empty()) {
        self->show_prompt(err);
      }
      self->scan();
    });
  }, LV_EVENT_VALUE_CHANGED, this);
  pending_remove_ssid = nid.empty() ? ssid : nid;
  KUtils::style_lock_mbox(mbox, 90);
}

void WifiPanel::handle_kb_input(lv_event_t *e)
{
  const lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_FOCUSED) {
    lv_keyboard_set_textarea(kb, password_input);
    lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
    // The keyboard covers most of the screen's width - hiding the (not
    // interactable while typing anyway) network list avoids the hard,
    // no-scroll clip through a row that a full-width keyboard otherwise
    // produces (confirmed live 2026-07-12), and hiding Back mirrors the
    // same pattern used for the octet keyboard in StaticIpPanel.
    lv_obj_add_flag(wifi_list_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_HIDDEN);
  } else if (code == LV_EVENT_DEFOCUSED || code == LV_EVENT_CANCEL) {
    entering_password = false;
    lv_keyboard_set_textarea(kb, NULL);
    lv_label_set_text(wifi_label, "Please select your wifi network");
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(pw_row, LV_OBJ_FLAG_HIDDEN);
    pw_visible = false;
    lv_textarea_set_password_mode(password_input, true);
    lv_label_set_text(lv_obj_get_child(eye_btn, 0), LV_SYMBOL_EYE_CLOSE);
    lv_obj_clear_flag(wifi_list_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(back_btn.get_container(), LV_OBJ_FLAG_HIDDEN);
  } else if (code == LV_EVENT_READY) {
    const char *password = lv_textarea_get_text(password_input);
    if (password == NULL || password[0] == 0) {
      return;
    }

    entering_password = false;
    std::string pw(password);
    lv_textarea_set_text(password_input, "");
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_state(password_input, LV_STATE_FOCUSED);
    connect(selected_network, pw);
    lv_obj_add_flag(pw_row, LV_OBJ_FLAG_HIDDEN);
    pw_visible = false;
    lv_textarea_set_password_mode(password_input, true);
    lv_label_set_text(lv_obj_get_child(eye_btn, 0), LV_SYMBOL_EYE_CLOSE);
    lv_obj_clear_flag(wifi_list_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(back_btn.get_container(), LV_OBJ_FLAG_HIDDEN);
  } else if (code == LV_EVENT_CLICKED) {
    lv_obj_t *target = lv_event_get_target(e);
    if (target != kb && target != password_input && target != eye_btn && target != pw_row) {
      lv_event_send(password_input, LV_EVENT_DEFOCUSED, NULL);
    }
  }
}

void WifiPanel::handle_eye_btn(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  lv_event_stop_bubbling(e);
  pw_visible = !pw_visible;
  lv_textarea_set_password_mode(password_input, !pw_visible);
  lv_label_set_text(lv_obj_get_child(eye_btn, 0),
                    pw_visible ? LV_SYMBOL_EYE_OPEN : LV_SYMBOL_EYE_CLOSE);
}

// NebulaOS connects, saves the network only when it worked, and restores the
// previous one otherwise; the reply carries the outcome and the new address.
void WifiPanel::connect(const std::string &ssid, const std::string &password) {
  busy = true;
  show_prompt(fmt::format("Connecting to {} ...", ssid));
  lv_obj_clear_flag(spinner, LV_OBJ_FLAG_HIDDEN);
  ws.send_jsonrpc(WIFI_CONNECT_METHOD, wifi_connect_params(ssid, password), [this, ssid](json &reply) {
    std::lock_guard<std::mutex> lock(lv_lock);
    busy = false;
    lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);
    auto err = wifi_error_text(reply);
    if (err.empty()) {
      // G5: also says when a static address does not fit the new network.
      bool address_ok = true;
      show_prompt(wifi_connected_text(reply, ssid, &address_ok));
    } else {
      show_prompt(err);
    }
    scan();
  });
}

#ifdef SIMULATOR
void WifiPanel::sim_fill() {
  lv_obj_move_foreground(cont);
  show_scan(json::parse(R"({"result":{"complete":true,
    "status":{"state":"COMPLETED","ssid":"Office_2.4Ghz","address":"192.168.0.98"},
    "networks":[
      {"ssid":"Office_2.4Ghz","signal":-48,"security":"psk","saved":true,"current":true},
      {"ssid":"Workshop","signal":-63,"security":"psk","saved":true,"current":false},
      {"ssid":"Guest","signal":-58,"security":"open","saved":false,"current":false},
      {"ssid":"Neighbour-5G","signal":-79,"security":"psk","saved":false,"current":false},
      {"ssid":"Corp","signal":-70,"security":"eap","saved":false,"current":false}]}})"));
}
#endif
