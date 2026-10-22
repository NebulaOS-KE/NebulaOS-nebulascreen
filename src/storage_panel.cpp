#include "storage_panel.h"
#include "nebulaos_display.h"
#include "nebulaos_system.h"
#include "settings_ui.h"
#include "utils.h"
#include "spdlog/spdlog.h"

LV_IMG_DECLARE(back);
LV_IMG_DECLARE(delete_img);

StoragePanel::StoragePanel(KWebSocketClient &c, std::mutex &lock)
  : ws(c)
  , lv_lock(lock)
  , cont(SettingsUi::page("Storage", &body))
  , back_btn(cont, &back, "Back", &StoragePanel::_handle_callback, this)
{
  SettingsUi::on_shown(cont, [this]() { shown(); });
  lv_obj_t *left = SettingsUi::column(body, 60);
  lv_obj_t *right = SettingsUi::column(body, 37);

  usage_label = lv_label_create(left);
  lv_obj_set_width(usage_label, LV_PCT(100));
  lv_label_set_long_mode(usage_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(usage_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_line_space(usage_label, 4, 0);
  lv_label_set_text(usage_label, "Adding up...");
  SettingsUi::hint(left, "Logs, old backups and installed system images are cleaned up "
                         "automatically.");

  cleanup_btn = SettingsUi::icon_button(right, &delete_img, "Clean up Creality files",
                                        &StoragePanel::_handle_callback, this);
  SettingsUi::set_tone(cleanup_btn, SettingsUi::Tone::Danger);
  SettingsUi::hint(right, "Removes the Creality system's data from the data drive. "
                          "Shows what it frees first.");

  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_FLOATING);
  lv_obj_align(back_btn.get_container(), LV_ALIGN_BOTTOM_RIGHT, 0, 0);
}

StoragePanel::~StoragePanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void StoragePanel::foreground() {
  lv_obj_move_foreground(cont);
}

// Every time the page comes on top. Walking the drive takes a few seconds.
void StoragePanel::shown() {
  ws.send_jsonrpc(STORAGE_METHOD, json::object(), [this](json &reply) {
    std::string text = storage_text(reply);
    std::lock_guard<std::mutex> lock(lv_lock);
    lv_label_set_text(usage_label, text.c_str());
  });
}

// LVGL thread: first what it would free, then the dialog.
void StoragePanel::start_cleanup() {
  if (busy) return;
  if (SettingsUi::refuse_if_printing()) {
    return;
  }
  busy = true;
  lv_obj_t *wait = SettingsUi::busy("Clean up Creality files", "Checking what can be removed...");
  ws.send_jsonrpc(CLEANUP_PLAN_METHOD, json::object(), [this, wait](json &reply) {
    CleanupPlan plan = cleanup_plan(reply);
    std::lock_guard<std::mutex> lock(lv_lock);
    SettingsUi::close_busy(wait);
    busy = false;
    if (!plan.ok || plan.bytes + plan.videos_bytes <= 0) {
      SettingsUi::notice("Clean up Creality files", cleanup_text(plan, true).c_str());
      return;
    }
    std::string option;
    if (plan.videos_bytes > 0) {
      option = "Also delete Creality's timelapse videos (" + human_bytes(plan.videos_bytes) + ")";
    }
    SettingsUi::confirm_option("Clean up Creality files",
      [plan](bool videos) { return cleanup_text(plan, videos); },
      option.empty() ? NULL : option.c_str(), "Delete Creality files",
      SettingsUi::Tone::Danger, [this](bool videos) { run_cleanup(videos); });
  });
}

// LVGL thread.
void StoragePanel::run_cleanup(bool videos) {
  if (SettingsUi::refuse_if_printing()) {   // a print may have started meanwhile
    return;
  }
  busy = true;
  lv_obj_t *wait = SettingsUi::busy("Clean up Creality files", "Deleting... this can take a minute.");
  ws.send_jsonrpc(CLEANUP_CREALITY_METHOD, json{{"videos", videos}}, [this, wait](json &reply) {
    std::string msg = cleanup_result_text(reply);
    spdlog::info("storage: Creality cleanup: {}", msg);
    std::lock_guard<std::mutex> lock(lv_lock);
    SettingsUi::close_busy(wait);
    busy = false;
    if (!SettingsUi::show_if_refused(reply)) {
      SettingsUi::notice("Clean up Creality files", msg.c_str());
    }
    shown();
  });
}

void StoragePanel::handle_callback(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  lv_obj_t *obj = lv_event_get_current_target(e);
  if (obj == back_btn.get_container()) {
    lv_obj_move_background(cont);
  } else if (obj == cleanup_btn) {
    start_cleanup();
  }
}
