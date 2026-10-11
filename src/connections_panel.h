#ifndef __CONNECTIONS_PANEL_H__
#define __CONNECTIONS_PANEL_H__

#include "button_container.h"
#include "printer_select_panel.h"
#include "spoolman_panel.h"
#include "websocket_client.h"
#include "wifi_panel.h"
#include "lvgl/lvgl.h"

#include <mutex>

// Settings > Connections (S1, 2026-10-11): how to reach the printer - Wi-Fi
// network, address (DHCP/static), its .local name, and the Mainsail, Moonraker
// and Spoolman addresses - with the Wi-Fi networks, Printers and Spoolman
// screens one tap away. Data: machine.nebulaos.network.status, server.config.
class ConnectionsPanel {
 public:
  ConnectionsPanel(KWebSocketClient &ws, std::mutex &lock, SpoolmanPanel &spoolman);
  ~ConnectionsPanel();

  void foreground();
  void enable_spoolman();
#ifdef SIMULATOR
  void sim_fill();
  void sim_wifi();
#endif

  static void _handle_callback(lv_event_t *e) {
    ((ConnectionsPanel *)e->user_data)->handle_callback(e);
  }

 private:
  void handle_callback(lv_event_t *e);
  void refresh();
  void show(const json &net_status_reply, const json &server_config_reply);

  KWebSocketClient &ws;
  std::mutex &lv_lock;
  SpoolmanPanel &spoolman_panel;
  WifiPanel wifi_panel;
  PrinterSelectPanel printer_select_panel;

  lv_obj_t *body;   // set by SettingsUi::page() while cont is built
  lv_obj_t *cont;
  lv_obj_t *wifi_value;
  lv_obj_t *address_value;
  lv_obj_t *name_value;
  lv_obj_t *mainsail_value;
  lv_obj_t *moonraker_value;
  lv_obj_t *spoolman_value;
  lv_obj_t *wifi_btn;
  lv_obj_t *printers_btn;
  lv_obj_t *spoolman_btn;
  ButtonContainer back_btn;
};

#endif // __CONNECTIONS_PANEL_H__
