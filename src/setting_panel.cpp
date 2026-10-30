#include "setting_panel.h"
#include "nebulaos_image.h"
#include "spdlog/spdlog.h"

LV_IMG_DECLARE(network_img);
LV_IMG_DECLARE(display_img);
LV_IMG_DECLARE(print);
LV_IMG_DECLARE(sysinfo_img);
LV_IMG_DECLARE(update_img);
LV_IMG_DECLARE(reset_img);

SettingPanel::SettingPanel(KWebSocketClient &c, std::mutex &l, lv_obj_t *parent, SpoolmanPanel &sm)
  : ws(c)
  , lv_lock(l)
  , cont(lv_obj_create(parent))
  , connections_panel(c, l, sm)
  , display_panel(c, l)
  , printer_panel(c, l)
  , system_panel(c, l)
  , updates_panel(c, l)
  , reset_panel(c, l)
  , connections_btn(cont, &network_img, "Connections", &SettingPanel::_handle_callback, this)
  , display_btn(cont, &display_img, "Display", &SettingPanel::_handle_callback, this)
  , printer_btn(cont, &print, "Printer", &SettingPanel::_handle_callback, this)
  , system_btn(cont, &sysinfo_img, "System", &SettingPanel::_handle_callback, this)
  , updates_btn(cont, &update_img, "Updates", &SettingPanel::_handle_callback, this)
  , reset_btn(cont, &reset_img, "Reset", &SettingPanel::_handle_callback, this)
{
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));

  static lv_coord_t rows[] = {LV_GRID_FR(1), LV_GRID_FR(5), LV_GRID_FR(5), LV_GRID_FR(1),
    LV_GRID_TEMPLATE_LAST};
  static lv_coord_t cols[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
    LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(cont, cols, rows);
  lv_obj_set_style_pad_column(cont, 14, 0);

  ButtonContainer *tiles[] = {&connections_btn, &display_btn, &printer_btn,
                              &system_btn, &updates_btn, &reset_btn};
  for (int i = 0; i < 6; ++i) {
    lv_obj_set_grid_cell(tiles[i]->get_container(), LV_GRID_ALIGN_STRETCH, i % 3, 1,
                         LV_GRID_ALIGN_STRETCH, 1 + i / 3, 1);
  }
}

SettingPanel::~SettingPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

lv_obj_t *SettingPanel::get_container() {
  return cont;
}

void SettingPanel::enable_spoolman() {
  connections_panel.enable_spoolman();
}

void SettingPanel::handle_callback(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
    return;
  }
  lv_obj_t *btn = lv_event_get_current_target(event);
  if (btn == connections_btn.get_container()) {
    connections_panel.foreground();
  } else if (btn == display_btn.get_container()) {
    display_panel.foreground();
  } else if (btn == printer_btn.get_container()) {
    printer_panel.foreground();
  } else if (btn == system_btn.get_container()) {
    system_panel.foreground();
  } else if (btn == updates_btn.get_container()) {
    updates_panel.foreground();
  } else if (btn == reset_btn.get_container()) {
    reset_panel.foreground();
  }
}

#ifdef SIMULATOR
#include "settings_ui.h"
#include "nebulaos_refusal.h"
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

void sim_screenshot(const char *path);  // main.cpp

struct SimTour {
  std::string dir;
  SimSteps steps;
  std::function<void()> done;
  size_t next = 0;
  bool shoot = false;
};

// One step every 600 ms: run it, then save the frame as <dir>/<name>.ppm on the
// next tick. `done` runs after the last screenshot.
void sim_run_tour(const std::string &dir, SimSteps steps, std::function<void()> done) {
  auto *t = new SimTour();
  t->dir = dir;
  t->steps = std::move(steps);
  t->done = std::move(done);
  lv_timer_create([](lv_timer_t *timer) {
    auto *t = (SimTour *)timer->user_data;
    if (t->shoot) {
      std::string path = t->dir + "/" + t->steps[t->next].first + ".ppm";
      sim_screenshot(path.c_str());
      t->shoot = false;
      if (++t->next >= t->steps.size()) {
        lv_timer_del(timer);
        auto done = t->done;
        delete t;
        if (done) done();
      }
      return;
    }
    t->steps[t->next].second();
    t->shoot = true;
  }, 600, t);
}

void SettingPanel::sim_tour(const char *dir, lv_obj_t *home) {
  auto close_dialogs = []() { SettingsUi::dismiss_confirms(); };
  SimSteps steps = {
    {"01-settings", [home]() { lv_obj_move_foreground(home); }},
    {"02-connections", [this]() { connections_panel.foreground(); connections_panel.sim_fill(); }},
    {"03-wifi", [this]() { connections_panel.sim_wifi(); }},
    {"04-display", [this]() { display_panel.foreground(); display_panel.sim_fill(); }},
    {"05-printer", [this]() { printer_panel.foreground(); }},
    {"06-system", [this]() { system_panel.foreground(); system_panel.sim_fill(); }},
    {"07-updates", [this]() { updates_panel.foreground(); updates_panel.sim_fill(); }},
    {"08-reset", [this]() { reset_panel.foreground(); }},
    {"09-clean-reset-confirm", [this]() { reset_panel.sim_tap(2); }},
    {"10-swap-confirm", [this, close_dialogs]() { close_dialogs(); reset_panel.sim_tap(3); }},
    {"11-reboot-confirm", [this, close_dialogs]() {
      close_dialogs(); system_panel.foreground(); system_panel.sim_fill(); system_panel.sim_tap_reboot(); }},
    // Run 4: printer name and MCU firmware.
    {"12-hostname", [this, close_dialogs]() {
      close_dialogs(); connections_panel.foreground(); connections_panel.sim_fill();
      connections_panel.sim_rename(); connections_panel.sim_hostname().sim_type("ke-workshop"); }},
    {"13-hostname-invalid", [this]() { connections_panel.sim_hostname().sim_type("ke_workshop"); }},
    {"14-hostname-confirm", [this]() {
      connections_panel.sim_hostname().sim_type("ke-workshop"); connections_panel.sim_hostname().sim_save(); }},
    {"15-printer", [this, close_dialogs]() { close_dialogs(); printer_panel.foreground(); }},
    {"16-mcu", [this]() { printer_panel.sim_mcu().foreground(); printer_panel.sim_mcu().sim_fill(); }},
    {"17-mcu-confirm", [this]() { printer_panel.sim_mcu().sim_install(0); }},
    {"18-mcu-busy", [this, close_dialogs]() { close_dialogs(); printer_panel.sim_mcu().sim_busy(); }},
    {"19-mcu-done", [this]() {
      printer_panel.sim_mcu().sim_result(R"({"result":{"result":"FLASHED","native":false}})"); }},
    {"20-mcu-failed", [this, close_dialogs]() {
      close_dialogs(); printer_panel.sim_mcu().sim_result(
        R"({"error":{"code":500,"message":"FLASH_FAILED: Could not enter bootloader: no handshake"}})"); }},
    // System image (on-printer image updates, 2026-10-14).
    {"21-updates", [this, close_dialogs]() {
      close_dialogs(); updates_panel.foreground(); updates_panel.sim_fill(); }},
    {"22-image", [this]() {
      updates_panel.sim_image().foreground(); updates_panel.sim_image().sim_fill(); }},
    {"23-image-confirm-nebulaos", [this]() { updates_panel.sim_image().sim_check(R"({"result":{
      "result":"OK","file":"upload/NebulaOS-KE-2026-10-15.img","sha256":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "size":98412544,"version":"2026.10.15","system":"nebulaos","target":"A",
      "slots":{"running":"B","A":{"kernel":"Linux-4.4.94","system":"creality"},
               "B":{"kernel":"Linux-6.6.157-rt79","system":"nebulaos"}}}})"); }},
    {"24-image-confirm-creality", [this, close_dialogs]() {
      close_dialogs(); updates_panel.sim_image().sim_check(R"({"result":{
      "result":"OK","file":"usb/sda1/Ender-3_V3_KE_F005_ota_img_V1.1.0.12.img","sha256":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
      "size":112283648,"version":"1.1.0.12","system":"creality","target":"A",
      "slots":{"running":"B","A":{"kernel":"Linux-6.6.157-rt79","system":"nebulaos"},
               "B":{"kernel":"Linux-6.6.157-rt79","system":"nebulaos"}}}})"); }},
    {"25-image-refused", [this, close_dialogs]() {
      close_dialogs(); updates_panel.sim_image().sim_check(
        R"J({"error":{"code":400,"message":"INVALID_IMAGE: not a KE update package (no ota_update.in)"}})J"); }},
    {"26-image-writing", [this, close_dialogs]() {
      close_dialogs(); updates_panel.sim_image().sim_progress(
        R"({"result":{"job":{"state":"running","phase":"writing","percent":57}}})"); }},
    {"27-image-restarting", [this]() { updates_panel.sim_image().sim_progress(
        R"({"result":{"job":{"state":"restarting","phase":"restarting","percent":100,"target":"A"}}})"); }},
    {"28-image-failed", [this]() { updates_panel.sim_image().sim_progress(
        R"({"result":{"job":{"state":"failed","phase":"failed","error":"INVALID_IMAGE",
           "message":"rootfs.squashfs chunk 41 is damaged","target_cleared":true}}})"); }},
    {"29-image-rolled-back", [close_dialogs]() {
      close_dialogs();
      auto l = image_last(json::parse(R"({"result":{"last":{"result":"ROLLED_BACK","version":"2026.10.15",
        "system":"nebulaos","from":"B","to":"A","note":"Klipper or the screen did not come up within 300 s"}}})"));
      SettingsUi::notice(l.title.c_str(), l.text.c_str()); }},
    {"30-swap-confirm-nebulaos", [this, close_dialogs]() {
      close_dialogs(); reset_panel.foreground();
      reset_panel.sim_slots(R"({"result":{"slots":{"running":"B","A":{"kernel":"Linux-6.6.157-rt79","system":"nebulaos"},
        "B":{"kernel":"Linux-6.6.157-rt79","system":"nebulaos"}}}})");
      reset_panel.sim_tap(3); }},
    // G91 v3.4: the refusal popup and the image install's stop outcomes
    {"31-refusal-active", [close_dialogs]() {
      close_dialogs(); SettingsUi::show_if_refused(json::parse(
        R"({"error":{"code":409,"message":"ACTIVE: Not possible while the printer is moving or heating."}})")); }},
    {"32-refusal-updating", [close_dialogs]() {
      close_dialogs(); SettingsUi::show_if_refused(json::parse(
        R"({"error":{"code":409,"message":"UPDATING: The new system is still being checked - try again after the check."}})")); }},
    {"33-image-stopped-print", [this, close_dialogs]() {
      close_dialogs(); updates_panel.sim_image().foreground(); updates_panel.sim_image().sim_progress(
        R"({"result":{"job":{"state":"failed","phase":"stopped_print","error":"PRINTING","stopped":true,"target_cleared":true}}})"); }},
    {"34-image-next-power-on", [this, close_dialogs]() {
      close_dialogs(); updates_panel.sim_image().sim_progress(
        R"({"result":{"job":{"state":"finished","phase":"next_power_on","percent":100,"target":"A"}}})"); }},
    // G98 (Run 18): Settings > Printer > Restart Klipper while heating
    {"35-restart-klipper-refused", [this, close_dialogs]() {
      close_dialogs(); printer_panel.foreground();
      RestartDecision d = restart_decision(RestartKind::KLIPPER, json::parse(
        R"({"result":{"busy":true,"reason":"ACTIVE","klippy":"ready","profile":"restart",
            "message":"Not possible while the printer is moving or heating."}})"));
      SettingsUi::refusal(d.refusal); }},
    // Spoolman server (Printers-style layout, 2026-10-30).
    {"36-spoolman", [this, close_dialogs]() {
      close_dialogs(); connections_panel.foreground(); connections_panel.sim_fill();
      connections_panel.sim_spoolman().foreground("http://192.168.0.235:7912"); }},
    {"37-spoolman-typing", [this]() {
      connections_panel.sim_spoolman().sim_fill("192.168.0.20", "7912"); }},
    {"38-spoolman-none", [this]() {
      connections_panel.sim_spoolman().foreground(""); connections_panel.sim_spoolman().sim_none(); }},
  };
  sim_run_tour(dir, std::move(steps), []() { exit(0); });
}
#endif
