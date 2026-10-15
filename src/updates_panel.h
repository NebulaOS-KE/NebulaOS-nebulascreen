#ifndef __UPDATES_PANEL_H__
#define __UPDATES_PANEL_H__

#include "button_container.h"
#include "image_panel.h"
#include "nebulaos_settings.h"
#include "websocket_client.h"
#include "lvgl/lvgl.h"

#include <mutex>
#include <string>
#include <vector>

// Settings > Updates (S1, 2026-10-11): app updates through Moonraker's update
// manager - Klipper, Moonraker, Mainsail, NebulaOS extensions - one at a time
// or all, plus Check now. Moonraker refuses updates while printing; NebulaOS's
// update supervisor rolls a broken app update back. The NebulaOS image itself
// (kernel, OS, GuppyScreen) is installed under System image (ImagePanel).
class UpdatesPanel {
 public:
  UpdatesPanel(KWebSocketClient &ws, std::mutex &lock);
  ~UpdatesPanel();

  void foreground();
#ifdef SIMULATOR
  void sim_fill();
  ImagePanel &sim_image() { return image_panel; }
#endif

  static void _handle_callback(lv_event_t *e) {
    ((UpdatesPanel *)e->user_data)->handle_callback(e);
  }

 private:
  void shown();
  void handle_callback(lv_event_t *e);
  void refresh(bool check_remote);
  void show(const json &status_reply);
  void upgrade(const std::string &name, const std::string &label);
  void set_busy(const std::string &text);

  KWebSocketClient &ws;
  std::mutex &lv_lock;

  lv_obj_t *body;   // set by SettingsUi::page() while cont is built
  lv_obj_t *cont;
  lv_obj_t *list;
  lv_obj_t *status_label;
  lv_obj_t *image_label;
  lv_obj_t *check_btn;
  lv_obj_t *all_btn;
  lv_obj_t *image_btn;
  ButtonContainer back_btn;
  ImagePanel image_panel;

  std::vector<UpdateRow> rows;
  std::vector<lv_obj_t *> row_btns;
  bool busy = false;
  bool waiting_for_reply = false;  // an upgrade request is still open
};

#endif // __UPDATES_PANEL_H__
