#ifndef __STATISTICS_PANEL_H__
#define __STATISTICS_PANEL_H__

#include "button_container.h"
#include "lvgl/lvgl.h"
#include "websocket_client.h"

#include <mutex>

// Settings > Printer > Statistics (Run 23, Tim 2026-11-02): total print time,
// prints and filament since NebulaOS (Moonraker's history) and, imported once,
// what the printer printed before NebulaOS (machine.nebulaos.stats; text from
// stats_text in nebulaos_system.h). Read-only.
class StatisticsPanel {
 public:
  StatisticsPanel(KWebSocketClient &ws, std::mutex &lock);
  ~StatisticsPanel();

  void foreground();
#ifdef SIMULATOR
  void sim_fill(const char *reply);
#endif

  static void _handle_callback(lv_event_t *e) {
    ((StatisticsPanel *)e->user_data)->handle_callback(e);
  }

 private:
  void shown();
  void handle_callback(lv_event_t *e);

  KWebSocketClient &ws;
  std::mutex &lv_lock;

  lv_obj_t *body;
  lv_obj_t *cont;
  lv_obj_t *summary_label;
  lv_obj_t *recent_label;
  ButtonContainer back_btn;
};

#endif // __STATISTICS_PANEL_H__
