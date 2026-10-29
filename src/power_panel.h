#ifndef __POWER_PANEL_H__
#define __POWER_PANEL_H__

#include "button_container.h"
#include "lvgl/lvgl.h"
#include "websocket_client.h"

#include <string>
#include <mutex>

#include "notify_consumer.h"

class PowerPanel : public NotifyConsumer {
  public:
    PowerPanel(KWebSocketClient &ws, std::mutex &l);
    ~PowerPanel();

    void create_devices(json &j);
    void consume(json &j);
    
    void foreground();
    // G80: once after the printer state arrived (InitPanel) - the popup
    // after a power loss does not wait for the next status change.
    void check_recovery();
    void handle_callback(lv_event_t *event);

    static void _handle_callback(lv_event_t *event) {
      PowerPanel *panel = (PowerPanel*)event->user_data;
      panel->handle_callback(event);
    };

  private:
  void shown();
    void create_device(json &j);
    void handle_device_callback(json &j);

    // Power-loss recovery section (rendered at the top of the panel). State comes from
    // the nebulaos_power_loss_recovery Klipper object; see plr_status.h.
    void build_recovery_section();
    void refresh_recovery();
    void confirm_resume();
    // G80 (Run 22): after a power loss the screen asks by itself, once per
    // record (plr_offer_key), whichever page is open.
    void offer_resume();
    void start_resume();
    std::string offered_key;
    void send_plr_command(const char *cmd, const char *failure);

    KWebSocketClient &ws;

    lv_obj_t *cont;
    ButtonContainer back_btn;

    std::map<std::string, lv_obj_t*> devices;

    lv_obj_t *recovery_cont = nullptr;
    lv_obj_t *recovery_status = nullptr;
    lv_obj_t *recovery_resume_btn = nullptr;
    lv_obj_t *recovery_dismiss_btn = nullptr;

};

#endif //__POWER_PANEL_H__
