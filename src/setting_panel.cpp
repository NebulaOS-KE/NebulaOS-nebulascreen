#include "setting_panel.h"
#include "config.h"
#include "utils.h"
#include "spdlog/spdlog.h"
#include "subprocess.hpp"

#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;
namespace sp = subprocess;

LV_IMG_DECLARE(network_img);
LV_IMG_DECLARE(refresh_img);
LV_IMG_DECLARE(spoolman_img);
LV_IMG_DECLARE(update_img);

#ifdef ZBOLT
LV_IMG_DECLARE(info_img);
#else
LV_IMG_DECLARE(sysinfo_img);
#endif

LV_IMG_DECLARE(print);

SettingPanel::SettingPanel(KWebSocketClient &c, std::mutex &l, lv_obj_t *parent, SpoolmanPanel &sm)
  : ws(c)
  , lv_lock(l)
  , cont(lv_obj_create(parent))
#ifndef OS_ANDROID
  , wifi_panel(c, l)
#endif
  , sysinfo_panel(c, l)
  , spoolman_panel(sm)
  , wifi_btn(cont, &network_img, "WIFI", &SettingPanel::_handle_callback, this)
  , restart_klipper_btn(cont, &refresh_img, "Restart\nKlipper", &SettingPanel::_handle_callback, this)
  , restart_firmware_btn(cont, &refresh_img, "Restart\nFirmware", &SettingPanel::_handle_callback, this)
#ifdef ZBOLT
  , sysinfo_btn(cont, &info_img, "System", &SettingPanel::_handle_callback, this)
#else
  , sysinfo_btn(cont, &sysinfo_img, "System", &SettingPanel::_handle_callback, this)
#endif
  , spoolman_btn(cont, &spoolman_img, "Spoolman", &SettingPanel::_handle_callback, this)
  , guppy_restart_btn(cont, &refresh_img, "Restart\nGuppy", &SettingPanel::_handle_callback, this)
  , guppy_update_btn(cont, &update_img, "Update\nGuppy", &SettingPanel::_handle_callback, this)
  , printer_select_btn(cont, &print, "Printers", &SettingPanel::_handle_callback, this)
{
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));

  spoolman_btn.disable();
#ifdef OS_ANDROID
  wifi_btn.disable();
#endif

  // NebulaOS Phase 0 cleanup: update.sh is confirmed never deployed to a real
  // NebulaOS device (it was OpenKE's own self-update mechanism, deleted from
  // this repo as dead weight - NebulaOS ships whole-image updates instead).
  // Capability-gate the same way Spoolman is gated above, rather than
  // leaving a button that always fails silently when clicked.
  {
    auto update_script = fs::canonical("/proc/self/exe").parent_path() / "update.sh";
    if (!fs::exists(update_script)) {
      guppy_update_btn.disable();
    }
  }

  static lv_coord_t grid_main_row_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(5), LV_GRID_FR(5), LV_GRID_FR(1),
    LV_GRID_TEMPLATE_LAST};
  static lv_coord_t grid_main_col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
    LV_GRID_TEMPLATE_LAST};

  lv_obj_set_grid_dsc_array(cont, grid_main_col_dsc, grid_main_row_dsc);

  // row 1
  lv_obj_set_grid_cell(restart_klipper_btn.get_container(), LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
  lv_obj_set_grid_cell(restart_firmware_btn.get_container(), LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
  lv_obj_set_grid_cell(guppy_restart_btn.get_container(), LV_GRID_ALIGN_STRETCH, 2, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
  lv_obj_set_grid_cell(guppy_update_btn.get_container(), LV_GRID_ALIGN_STRETCH, 3, 1, LV_GRID_ALIGN_STRETCH, 1, 1);

  // row 2
  lv_obj_set_grid_cell(wifi_btn.get_container(), LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 2, 1);
  lv_obj_set_grid_cell(printer_select_btn.get_container(), LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 2, 1);
  lv_obj_set_grid_cell(spoolman_btn.get_container(), LV_GRID_ALIGN_STRETCH, 2, 1, LV_GRID_ALIGN_STRETCH, 2, 1);
  lv_obj_set_grid_cell(sysinfo_btn.get_container(), LV_GRID_ALIGN_STRETCH, 3, 1, LV_GRID_ALIGN_STRETCH, 2, 1);

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

void SettingPanel::handle_callback(lv_event_t *event) {
  if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
    lv_obj_t *btn = lv_event_get_current_target(event);

    if (btn == wifi_btn.get_container()) {
      spdlog::trace("wifi pressed");
#ifndef OS_ANDROID
      wifi_panel.foreground();
#endif
    } else if (btn == sysinfo_btn.get_container()) {
      spdlog::trace("setting system info pressed");
      sysinfo_panel.foreground();
    } else if (btn == restart_klipper_btn.get_container()) {
      spdlog::trace("setting restart klipper pressed");
      ws.send_jsonrpc("printer.restart");
    } else if (btn == restart_firmware_btn.get_container()) {
      spdlog::trace("setting restart klipper pressed");
      ws.send_jsonrpc("printer.firmware_restart");
    } else if (btn == spoolman_btn.get_container()) {
      spdlog::trace("setting spoolman pressed");
      spoolman_panel.foreground();
    } else if (btn == guppy_restart_btn.get_container()) {
      spdlog::trace("restart guppy pressed");
      // Through Moonraker (GUI_BACKEND=MOONRAKER_ONLY): its supervisorctl shim
      // runs /etc/init.d/S58guppyscreen restart; NebulaOS allows the service in
      // moonraker.asvc. On success this process is stopped and restarted.
      json params = {{"service", "guppyscreen"}};
      ws.send_jsonrpc("machine.services.restart", params, [this](json &reply) {
        if (!reply.contains("error")) {
          return;
        }
        auto msg = reply["/error/message"_json_pointer].is_string()
          ? reply["/error/message"_json_pointer].template get<std::string>()
          : reply["error"].dump();
        spdlog::warn("Restart Guppy Screen failed: {}", msg);
        std::lock_guard<std::mutex> lock(lv_lock);
        KUtils::notify_toast("Restart failed: " + msg, 5000);
      });
    } else if (btn == guppy_update_btn.get_container()) {
      spdlog::trace("update guppy pressed");
      // TODO: throw this inside the global threadpool to make it async
      auto update_script = fs::canonical("/proc/self/exe").parent_path() / "update.sh";
      const fs::path script(update_script);
      if (fs::exists(script)) {
        sp::call(script);
      } else {
        spdlog::warn("Failed to update Guppy Screen. Did not find update script.");
      }
    } else if (btn == printer_select_btn.get_container()) {
      spdlog::trace("setting printers pressed");
      printer_select_panel.foreground();
    }
  }
}

void SettingPanel::enable_spoolman() {
  spoolman_btn.enable();
}
