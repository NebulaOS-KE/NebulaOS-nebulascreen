#ifndef __HOSTNAME_PANEL_H__
#define __HOSTNAME_PANEL_H__

#include "websocket_client.h"
#include "lvgl/lvgl.h"

#include <mutex>
#include <string>

// Settings > Connections > Name (run 4, 2026-10-11): rename the printer
// (<name>.local). On-screen keyboard, the NebulaOS hostname rules checked as
// you type (nebulaos_settings hostname_check), a confirmation that says how
// the printer is reached afterwards, then machine.nebulaos.network.hostname
// and - so mDNS announces the new name - machine.services.restart moonraker
// (not while printing: then the name follows at the next Moonraker start).
class HostnamePanel {
 public:
  HostnamePanel(KWebSocketClient &ws, std::mutex &lock);
  ~HostnamePanel();

  // `current`: the hostname as Connections shows it ("" when unknown).
  void foreground(const std::string &current, const std::string &address);
#ifdef SIMULATOR
  void sim_type(const char *text);
  void sim_save();
#endif

  static void _handle_callback(lv_event_t *e) {
    ((HostnamePanel *)e->user_data)->handle_callback(e);
  }

 private:
  void handle_callback(lv_event_t *e);
  void validate();
  void save();
  void apply(const std::string &name);

  KWebSocketClient &ws;
  std::mutex &lv_lock;
  std::string current;
  std::string address;

  lv_obj_t *body;   // set by SettingsUi::page() while cont is built
  lv_obj_t *cont;
  lv_obj_t *input;
  lv_obj_t *save_btn;
  lv_obj_t *cancel_btn;
  lv_obj_t *message;
  lv_obj_t *kb;
};

#endif // __HOSTNAME_PANEL_H__
