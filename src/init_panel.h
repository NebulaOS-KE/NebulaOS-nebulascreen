#ifndef __INIT_PANEL_H__
#define __INIT_PANEL_H__

#include "lvgl/lvgl.h"
#include "websocket_client.h"
#include "nebulaos_system.h"
#include "main_panel.h"
#include "bedmesh_panel.h"
#include "print_status_panel.h"
#include "filament_runout_panel.h"

#include <mutex>

class InitPanel {
 public:
  InitPanel(MainPanel &mp, BedMeshPanel &bmp, FilamentRunoutPanel &frp, std::mutex &l);
  ~InitPanel();

  void connected(KWebSocketClient &ws);
  void disconnected(KWebSocketClient &ws);
  void set_message(const char *message);
#ifdef SIMULATOR
  /* Hide the "Waiting for printer..." overlay in simulator mode. */
  void sim_hide();
#endif

 private:
  void image_result(int tries);
  void klipper_state_for_notices(bool ready);  // LVGL lock held

  lv_obj_t *cont;
  lv_obj_t *label;
  MainPanel &main_panel;
  BedMeshPanel &bedmesh_panel;
  FilamentRunoutPanel &filament_runout_panel;
  std::mutex &lv_lock;
  KWebSocketClient *ws = NULL;   // for image_result's retries
  lv_timer_t *image_timer = NULL;
  int image_tries = 0;
  // Issue #8: the held-back "mcu-unreachable" notice (notice_waits_for_klipper).
  lv_timer_t *mcu_notice_timer = NULL;
  SystemNotice mcu_notice;
};

#endif // __INIT_PANEL_H__
