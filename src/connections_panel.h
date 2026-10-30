#ifndef __CONNECTIONS_PANEL_H__
#define __CONNECTIONS_PANEL_H__

#include "button_container.h"
#include "hostname_panel.h"
#include "printer_select_panel.h"
#include "spoolman_panel.h"
#include "spoolman_server_panel.h"
#include "websocket_client.h"
#include "wifi_panel.h"
#include "lvgl/lvgl.h"

#include <mutex>

// Settings > Connections (S1, 2026-10-11): how to reach the printer - Wi-Fi
// network, address (DHCP/static), its .local name (renamed on its own page), and the Mainsail, Moonraker
// and Spoolman addresses - with the Wi-Fi networks, Printers and Spoolman
// server (G125: address + port, like Printers) screens one tap away. Data:
// machine.nebulaos.network.status, server.config.
class ConnectionsPanel {
 public:
  ConnectionsPanel(KWebSocketClient &ws, std::mutex &lock, SpoolmanPanel &spoolman);
  ~ConnectionsPanel();

  void foreground();
  void enable_spoolman();
#ifdef SIMULATOR
  void sim_fill();
  void sim_wifi();
  HostnamePanel &sim_hostname() { return hostname_panel; }
  SpoolmanServerPanel &sim_spoolman() { return spoolman_server_panel; }
  void sim_rename() { lv_event_send(rename_btn, LV_EVENT_CLICKED, NULL); }
#endif

  static void _handle_callback(lv_event_t *e) {
    ((ConnectionsPanel *)e->user_data)->handle_callback(e);
  }

 private:
  void shown();
  void handle_callback(lv_event_t *e);
  void refresh();
  void show(const json &net_status_reply, const json &server_config_reply);

  KWebSocketClient &ws;
  std::mutex &lv_lock;
  SpoolmanPanel &spoolman_panel;
  WifiPanel wifi_panel;
  PrinterSelectPanel printer_select_panel;
  HostnamePanel hostname_panel;
  SpoolmanServerPanel spoolman_server_panel;   // G125: the server; the spools stay on Extrude
  std::string hostname;   // from the last network status, "" when unknown
  std::string address;

  lv_obj_t *body;   // set by SettingsUi::page() while cont is built
  lv_obj_t *cont;
  lv_obj_t *wifi_value;
  lv_obj_t *address_value;
  lv_obj_t *name_value;
  lv_obj_t *rename_btn;
  lv_obj_t *mainsail_value;
  lv_obj_t *moonraker_value;
  lv_obj_t *spoolman_value;
  std::string spoolman;    // as last read, so the Spoolman page shows it at once
  lv_obj_t *wifi_btn;
  lv_obj_t *printers_btn;
  lv_obj_t *spoolman_btn;
  ButtonContainer back_btn;
};

#endif // __CONNECTIONS_PANEL_H__
