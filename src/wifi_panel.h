#ifndef __WIFI_PANEL_H__
#define __WIFI_PANEL_H__

#include "button_container.h"
#include "nebulaos_settings.h"
#include "websocket_client.h"
#include "static_ip_panel.h"
#include "network_detail_panel.h"
#include "wifi_row_item.h"
#include "lvgl/lvgl.h"
#include <mutex>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Wi-Fi networks (Q3, 2026-10-11): every Wi-Fi action goes through NebulaOS -
// machine.nebulaos.wifi.scan / connect / forget - instead of wpa_supplicant's
// socket. NebulaOS saves a new network only once it connected, and puts the
// previous one back after a wrong password or a timeout.
class WifiPanel {
public:
  WifiPanel(KWebSocketClient &ws, std::mutex &l);

  ~WifiPanel();

  void foreground();
  // G55: called (LVGL lock held) when the panel closes and after a connect
  // or forget finishes, so the page underneath can show the new network.
  std::function<void()> on_changed;
  void handle_back_btn(lv_event_t *event);
#ifdef SIMULATOR
  void sim_fill();
#endif
  void confirm_remove_network(const std::string &ssid, const std::string &nid);
  void handle_kb_input(lv_event_t *e);
  void handle_eye_btn(lv_event_t *e);
  void connect(const std::string &ssid, const std::string &password);

  // WifiRowItem calls this when its row is tapped: connects/reconnects for
  // an ordinary row, or opens NetworkDetailPanel if the row is the network
  // we're already connected to (where "connect" wouldn't mean anything).
  void handle_row_activated(const std::string &ssid);

  // NetworkDetailPanel's two buttons call back into here - it owns the
  // StaticIpPanel instance those actions need.
  void open_static_ip_for_current();
  void forget_current_network();

  static void _handle_back_btn(lv_event_t *event) {
    WifiPanel *panel = (WifiPanel *)event->user_data;
    panel->handle_back_btn(event);
  };


  static void _handle_kb_input(lv_event_t *e) {
    WifiPanel *panel = (WifiPanel *)e->user_data;
    panel->handle_kb_input(e);
  };

  static void _handle_eye_btn(lv_event_t *e) {
    WifiPanel *panel = (WifiPanel *)e->user_data;
    panel->handle_eye_btn(e);
  };

private:
  void rebuild_wifi_rows();
  void scan();
  void show_scan(const json &reply);   // LVGL lock held
  void show_prompt(const std::string &text);

  KWebSocketClient &ws;
  std::mutex &lv_lock;
  lv_obj_t *cont;
  lv_obj_t *spinner;
  lv_obj_t *top_cont;
  lv_obj_t *wifi_list_cont;
  lv_obj_t *wifi_right;
  lv_obj_t *prompt_cont;
  lv_obj_t *wifi_label;
  lv_obj_t *pw_row;
  lv_obj_t *password_input;
  lv_obj_t *eye_btn;
  ButtonContainer back_btn;
  lv_obj_t *kb;
  std::string selected_network;
  std::string cur_network;
  std::string cur_address;
  std::string pending_remove_ssid;
  std::vector<WifiNetwork> networks;
  std::vector<std::unique_ptr<WifiRowItem>> wifi_rows;

  bool entering_password = false;
  bool pw_visible = false;
  bool busy = false;          // a connect or forget is running
  // A connect/forget result stays on screen until the next action; the scan
  // that follows updates only the list (P9: the error flashed by, 2026-10-15).
  bool holding_result = false;
  int rescans_left = 0;
  StaticIpPanel static_ip_panel;
  NetworkDetailPanel network_detail_panel;

};

#endif // __WIFI_PANEL_H__
