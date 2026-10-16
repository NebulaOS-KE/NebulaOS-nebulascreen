#ifndef __BOOT_LOGO_PANEL_H__
#define __BOOT_LOGO_PANEL_H__

#include "button_container.h"
#include "nebulaos_system.h"
#include "websocket_client.h"
#include "lvgl/lvgl.h"

#include <mutex>
#include <string>

// Settings > Display > Boot logo (G61, 2026-10-16). One dropdown: NebulaOS
// plus every image in the boot logo folder, the active one selected. Picking
// one shows a preview; Apply makes it the boot screen and the power-on logo
// of the running slot. "Add from USB" copies an image from the stick into the
// folder and selects it. Delete removes an upload. Restore Creality logo puts
// the original power-on logo back.
class BootLogoPanel {
 public:
  BootLogoPanel(KWebSocketClient &ws, std::mutex &lock);
  ~BootLogoPanel();

  void foreground();

  static void _handle_callback(lv_event_t *e) {
    ((BootLogoPanel *)e->user_data)->handle_callback(e);
  }

 private:
  void shown();
  void handle_callback(lv_event_t *e);
  // Reload the list; select `want` (a file name) if given, else the active one.
  void refresh(const std::string &want);
  void show_preview();
  void apply();
  void add_from_usb();
  void open_usb_picker();
  void close_usb_picker();
  void import_usb(const UsbImage &img);
  void delete_selected();
  void run(const char *method, const json &params, const char *what, const std::string &select);
  std::string selected_name();

  KWebSocketClient &ws;
  std::mutex &lv_lock;

  lv_obj_t *body;
  lv_obj_t *cont;
  lv_obj_t *logo_dd;
  lv_obj_t *preview;
  lv_obj_t *preview_note;
  lv_obj_t *power_label;
  lv_obj_t *apply_btn;
  lv_obj_t *usb_btn;
  lv_obj_t *delete_btn;
  lv_obj_t *restore_btn;
  lv_obj_t *usb_picker = NULL;
  ButtonContainer back_btn;

  BootLogoList list;
  std::vector<UsbImage> picker_items;   // what the open USB picker shows
  bool updating = false;
};

#endif // __BOOT_LOGO_PANEL_H__
