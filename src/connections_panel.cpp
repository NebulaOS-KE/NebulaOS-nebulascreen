#include "connections_panel.h"
#include "nebulaos_network.h"
#include "nebulaos_settings.h"
#include "settings_ui.h"
#include "spdlog/spdlog.h"

LV_IMG_DECLARE(back);
LV_IMG_DECLARE(network_img);
LV_IMG_DECLARE(print);
LV_IMG_DECLARE(spoolman_img);

ConnectionsPanel::ConnectionsPanel(KWebSocketClient &c, std::mutex &lock, SpoolmanPanel &sm)
  : ws(c)
  , lv_lock(lock)
  , spoolman_panel(sm)
  , wifi_panel(c, lock)
  , hostname_panel(c, lock)
  , spoolman_server_panel(c, lock)
  , cont(SettingsUi::page("Connections", &body))
  , back_btn(cont, &back, "Back", &ConnectionsPanel::_handle_callback, this)
{
  SettingsUi::on_shown(cont, [this]() { shown(); });
  lv_obj_t *left = SettingsUi::column(body, 60);
  wifi_value = SettingsUi::info_line(left, "Wi-Fi");
  address_value = SettingsUi::info_line(left, "Address");
  name_value = SettingsUi::info_line(left, "Name");
  // Rename: a small button at the end of the Name line.
  rename_btn = lv_btn_create(lv_obj_get_parent(name_value));
  lv_obj_set_size(rename_btn, 64, 24);
  lv_obj_t *rl = lv_label_create(rename_btn);
  lv_label_set_text(rl, "Change");
  lv_obj_set_style_text_font(rl, &lv_font_montserrat_12, 0);
  lv_obj_center(rl);
  lv_obj_add_event_cb(rename_btn, &ConnectionsPanel::_handle_callback, LV_EVENT_CLICKED, this);
  lv_obj_set_flex_align(lv_obj_get_parent(name_value), LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  SettingsUi::divider(left);
  mainsail_value = SettingsUi::info_line(left, "Mainsail");
  moonraker_value = SettingsUi::info_line(left, "Moonraker");
  spoolman_value = SettingsUi::info_line(left, "Spoolman");

  lv_obj_t *right = SettingsUi::column(body, 37);
  wifi_btn = SettingsUi::icon_button(right, &network_img, "Wi-Fi networks",
                                     &ConnectionsPanel::_handle_callback, this);
  printers_btn = SettingsUi::icon_button(right, &print, "Printers",
                                         &ConnectionsPanel::_handle_callback, this);
  spoolman_btn = SettingsUi::icon_button(right, &spoolman_img, "Spoolman",
                                         &ConnectionsPanel::_handle_callback, this);
  // G125: always on - it is where the server is set, not the spool list

  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_FLOATING);
  SettingsUi::place_back(back_btn.get_container());  // G94: one Back position
}

ConnectionsPanel::~ConnectionsPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void ConnectionsPanel::enable_spoolman() {
  // G125: the Spoolman button sets the server, so it is never disabled.
}

void ConnectionsPanel::foreground() {
  lv_obj_move_foreground(cont);
}

// Every time the page comes on top (SettingsUi::on_shown).
void ConnectionsPanel::shown() {
  refresh();
}

// Called on the LVGL thread; replies arrive on the websocket thread.
void ConnectionsPanel::refresh() {
  ws.send_jsonrpc(NETWORK_STATUS_METHOD, json::object(), [this](json &reply) {
    ws.send_jsonrpc(SERVER_CONFIG_METHOD, json::object(), [this, reply](json &cfg) {
      std::lock_guard<std::mutex> lock(lv_lock);
      show(reply, cfg);
    });
  });
}

// LVGL lock held by the caller.
void ConnectionsPanel::show(const json &net, const json &cfg) {
  auto v = connections_view(net, cfg);
  lv_label_set_text(wifi_value, v.wifi.c_str());
  lv_label_set_text(address_value, v.address.c_str());
  lv_label_set_text(name_value, v.name.c_str());
  hostname = v.hostname;
  address = v.address == "-" ? "" : v.address.substr(0, v.address.find(' '));
  lv_label_set_text(mainsail_value, v.mainsail.empty() ? "-" : v.mainsail.c_str());
  lv_label_set_text(moonraker_value, v.moonraker.empty() ? "-" : v.moonraker.c_str());
  lv_label_set_text(spoolman_value, v.spoolman.empty() ? "not configured" : v.spoolman.c_str());
}

#ifdef SIMULATOR
void ConnectionsPanel::sim_fill() {
  show(json::parse(R"({"result":{"config":{"hostname":"nebulaos","ipv4":{"mode":"dhcp"}},
    "live":{"address":"192.168.0.98","ssid":"Office_2.4Ghz"}}})"),
       json::parse(R"({"result":{"config":{"server":{"port":7125},
    "spoolman":{"server":"http://192.168.0.20:7912"}}}})"));
  enable_spoolman();
}

void ConnectionsPanel::sim_wifi() {
  wifi_panel.sim_fill();
}
#endif

void ConnectionsPanel::handle_callback(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
    return;
  }
  lv_obj_t *obj = lv_event_get_current_target(e);
  if (obj == back_btn.get_container()) {
    lv_obj_move_background(cont);
  } else if (obj == wifi_btn) {
    wifi_panel.foreground();
  } else if (obj == rename_btn) {
    hostname_panel.foreground(hostname, address);
  } else if (obj == printers_btn) {
    printer_select_panel.foreground();
  } else if (obj == spoolman_btn) {
    spoolman_server_panel.foreground();
  }
}
