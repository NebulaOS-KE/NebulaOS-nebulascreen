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

  // known: the server the Connections page already read ("" = none or not
  // read yet), shown at once; the status request then confirms it.
  void foreground(const std::string &known);
#ifdef SIMULATOR
  void sim_fill(const char *host, const char *port);
  void sim_none();
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
  void show_current(const std::string &server, bool loading);
  void show_keyboard(lv_obj_t *ta);
  void hide_keyboard();

  KWebSocketClient &ws;
  std::mutex &lv_lock;
  bool configured = false;

  lv_obj_t *body;   // set by SettingsUi::page() while cont is built
  lv_obj_t *cont;
  lv_obj_t *card;          // the server in use, like a saved printer on Printers
  lv_obj_t *card_label;
  lv_obj_t *none_label;    // "No server set" / "Reading..."
  lv_obj_t *host_input;
  lv_obj_t *port_input;
  lv_obj_t *save_btn;
  lv_obj_t *remove_btn;
  lv_obj_t *back_btn;
  lv_obj_t *message;
  lv_obj_t *kb;
};

#endif // __SPOOLMAN_SERVER_PANEL_H__
