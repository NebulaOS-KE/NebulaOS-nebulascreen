#ifndef __SPOOLMAN_SERVER_PANEL_H__
#define __SPOOLMAN_SERVER_PANEL_H__

#include "websocket_client.h"
#include "lvgl/lvgl.h"

#include <mutex>
#include <string>

// Settings > Connections > Spoolman (G125, Run 22): the address and port of the
// Spoolman server Moonraker uses, like the Printers page takes a Moonraker
// address. NebulaOS writes [spoolman] server in moonraker.conf
// (machine.nebulaos.spoolman.server / .remove), then Moonraker restarts
// (machine.services.restart moonraker, as after a rename). Refused by NebulaOS
// while the printer is busy. The spool list stays on the extruder screen.
class SpoolmanServerPanel {
 public:
  SpoolmanServerPanel(KWebSocketClient &ws, std::mutex &lock);
  ~SpoolmanServerPanel();

  void foreground();
#ifdef SIMULATOR
  void sim_fill(const char *host, const char *port);
#endif

  static void _handle_callback(lv_event_t *e) {
    ((SpoolmanServerPanel *)e->user_data)->handle_callback(e);
  }

 private:
  void handle_callback(lv_event_t *e);
  void validate();
  void save();
  void remove();
  void done(const json &reply, const std::string &what);
  void set_message(const std::string &text, bool error);

  KWebSocketClient &ws;
  std::mutex &lv_lock;
  bool configured = false;

  lv_obj_t *body;   // set by SettingsUi::page() while cont is built
  lv_obj_t *cont;
  lv_obj_t *current;
  lv_obj_t *host_input;
  lv_obj_t *port_input;
  lv_obj_t *save_btn;
  lv_obj_t *remove_btn;
  lv_obj_t *cancel_btn;
  lv_obj_t *message;
  lv_obj_t *kb;
};

#endif // __SPOOLMAN_SERVER_PANEL_H__
