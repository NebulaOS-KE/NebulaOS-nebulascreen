#ifndef __STATIC_IP_PANEL_H__
#define __STATIC_IP_PANEL_H__

#include "button_container.h"
#include "lvgl/lvgl.h"
#include "websocket_client.h"
#include <mutex>
#include <string>

// Static IP configuration sub-screen, reached from NetworkDetailPanel's "Configure
// Static IP" button. Backed by NebulaOS (machine.nebulaos.network.status / .ipv4, see
// nebulaos_network.h): persistent across reboots, verified on apply with automatic
// revert, and global for wlan0 - not per Wi-Fi network. This panel never touches wlan0
// itself.
class StaticIpPanel {
public:
  StaticIpPanel(KWebSocketClient &ws, std::mutex &l);
  ~StaticIpPanel();

  // Shows the panel (ssid is only the network it was opened from) and kicks off a
  // status refresh.
  void foreground(const std::string &ssid);

  void handle_back_btn(lv_event_t *event);
  // DHCP/Manual is a pure view switch now - it only ever changes which
  // fields are shown/editable, never touches the network. Reverting a live
  // static config to DHCP is its own explicitly-labelled action, see
  // handle_revert_btn.
  void handle_mode_toggle(lv_event_t *event);
  void handle_revert_btn(lv_event_t *event);
  void handle_save_btn(lv_event_t *event);
  void handle_kb_input(lv_event_t *event);

  static void _handle_back_btn(lv_event_t *e) { ((StaticIpPanel *)e->user_data)->handle_back_btn(e); }
  static void _handle_mode_toggle(lv_event_t *e) { ((StaticIpPanel *)e->user_data)->handle_mode_toggle(e); }
  static void _handle_revert_btn(lv_event_t *e) { ((StaticIpPanel *)e->user_data)->handle_revert_btn(e); }
  static void _handle_save_btn(lv_event_t *e) { ((StaticIpPanel *)e->user_data)->handle_save_btn(e); }
  static void _handle_kb_input(lv_event_t *e) { ((StaticIpPanel *)e->user_data)->handle_kb_input(e); }

private:
  void refresh_status();
  void set_manual_mode(bool manual);
  void update_revert_visibility();
  void set_busy(bool busy, const std::string &msg);
  // Sends machine.nebulaos.network.ipv4 (verification + automatic revert can
  // take a minute or more); the reply arrives on the websocket thread.
  void apply_ipv4(const json &params, bool to_static, const std::string &busy_msg);

  KWebSocketClient &ws;
  std::mutex &lv_lock;
  std::string ssid;

  lv_obj_t *cont;
  lv_obj_t *title_label;
  lv_obj_t *mode_cont;
  lv_obj_t *dhcp_mode_btn;
  lv_obj_t *manual_mode_btn;
  lv_obj_t *revert_btn;
  // Each address is 4 small octet boxes rather than one free-text field with
  // dots - fewer characters to type on the shared numeric pad, and a bad
  // octet can be flagged red right where it was typed instead of only via a
  // status line at the top.
  lv_obj_t *ip_octets[4];
  lv_obj_t *netmask_octets[4];
  lv_obj_t *gateway_octets[4];
  lv_obj_t *dns_octets[4];
  lv_obj_t *status_label;
  // Scrolls independently of the pinned title/mode/status header above it -
  // holds only the field rows now, so Save (a direct sibling below it, not
  // inside it) can never scroll out of view.
  lv_obj_t *body_cont;
  lv_obj_t *save_btn;
  lv_obj_t *spinner;
  lv_obj_t *kb;
  ButtonContainer back_btn;

  bool manual_mode = false;
  bool has_static_config = false;
  bool busy = false;
};

#endif // __STATIC_IP_PANEL_H__
