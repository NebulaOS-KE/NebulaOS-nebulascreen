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
  , cont(SettingsUi::page("Connections", &body))
  , back_btn(cont, &back, "Back", &ConnectionsPanel::_handle_callback, this)
{
  lv_obj_t *left = SettingsUi::column(body, 60);
  wifi_value = SettingsUi::info_line(left, "Wi-Fi");
  address_value = SettingsUi::info_line(left, "Address");
  name_value = SettingsUi::info_line(left, "Name");
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
  lv_obj_add_state(spoolman_btn, LV_STATE_DISABLED);

  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_FLOATING);
  lv_obj_align(back_btn.get_container(), LV_ALIGN_BOTTOM_RIGHT, 0, 0);
}

ConnectionsPanel::~ConnectionsPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void ConnectionsPanel::enable_spoolman() {
  lv_obj_clear_state(spoolman_btn, LV_STATE_DISABLED);
}

void ConnectionsPanel::foreground() {
  lv_obj_move_foreground(cont);
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
  } else if (obj == printers_btn) {
    printer_select_panel.foreground();
  } else if (obj == spoolman_btn) {
    spoolman_panel.foreground();
  }
}
