#ifndef __CAMERA_PANEL_H__
#define __CAMERA_PANEL_H__

#include "websocket_client.h"
#include "lvgl/lvgl.h"

#include <map>
#include <mutex>
#include <string>

// Settings > Display > Camera (G121, Run 22): the Creality Nebula Camera's
// controls - night mode, anti-flicker, white balance, exposure (one-way to
// manual), brightness/contrast/saturation/sharpness/zoom, Defaults, Reset
// camera, firmware version. NebulaOS saves them and applies them again each
// time the camera appears (nebulaos_camera.h). Flip stays in Mainsail.
class CameraPanel {
 public:
  CameraPanel(KWebSocketClient &ws, std::mutex &lock);
  ~CameraPanel();

  void foreground();

  static void _handle_callback(lv_event_t *e) {
    ((CameraPanel *)e->user_data)->handle_callback(e);
  }

 private:
  void shown();
  void show(const json &status_reply);
  void handle_callback(lv_event_t *e);
  void send(const json &changes);
  void after(const json &reply);
  void set_slider(const std::string &key, int value);
  void update_visibility();

  KWebSocketClient &ws;
  std::mutex &lv_lock;
  json config;
  bool loading = false;

  lv_obj_t *body;   // set by SettingsUi::page() while cont is built
  lv_obj_t *cont;
  lv_obj_t *ir_dd;
  lv_obj_t *flicker_dd;
  lv_obj_t *wb_dd;
  lv_obj_t *exposure_dd;
  lv_obj_t *defaults_btn;
  lv_obj_t *reset_btn;
  lv_obj_t *info;
  std::map<std::string, lv_obj_t *> sliders;
  std::map<std::string, lv_obj_t *> slider_rows;
  std::map<std::string, lv_obj_t *> slider_values;
  lv_obj_t *back_tile;
};

#endif // __CAMERA_PANEL_H__
