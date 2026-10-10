#include "system_panel.h"
#include "nebulaos_display.h"
#include "nebulaos_system.h"
#include "utils.h"
#include "spdlog/spdlog.h"

LV_IMG_DECLARE(back);

static lv_obj_t *labeled_row(lv_obj_t *parent, const char *label) {
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *l = lv_label_create(row);
  lv_label_set_text(l, label);
  lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
  return row;
}

static lv_obj_t *text_btn(lv_obj_t *parent, const char *label, lv_event_cb_t cb, void *user) {
  lv_obj_t *b = lv_btn_create(parent);
  lv_obj_set_size(b, LV_PCT(100), 44);
  lv_obj_t *l = lv_label_create(b);
  lv_label_set_text(l, label);
  lv_obj_center(l);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
  return b;
}

SystemPanel::SystemPanel(KWebSocketClient &c, std::mutex &lock)
  : ws(c)
  , lv_lock(lock)
  , cont(lv_obj_create(lv_scr_act()))
  , left_cont(lv_obj_create(cont))
  , right_cont(lv_obj_create(cont))
  , info_label(lv_label_create(left_cont))
  , mode_dd(nullptr)
  , image_dd(nullptr)
  , logs_btn(nullptr)
  , reboot_btn(nullptr)
  , shutdown_btn(nullptr)
  , back_btn(cont, &back, "Back", &SystemPanel::_handle_callback, this)
  , logs_panel(c, lock)
{
  lv_obj_move_background(cont);
  lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);

  for (lv_obj_t *col : {left_cont, right_cont}) {
    lv_obj_set_height(col, LV_PCT(100));
    lv_obj_set_style_pad_all(col, 4, 0);
    lv_obj_set_style_pad_row(col, 6, 0);
    lv_obj_set_style_border_width(col, 0, 0);
    lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  }
  lv_obj_set_width(left_cont, LV_PCT(62));
  lv_obj_set_width(right_cont, LV_PCT(38));

  lv_obj_set_width(info_label, LV_PCT(100));
  lv_label_set_long_mode(info_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(info_label, &lv_font_montserrat_12, 0);
  lv_label_set_text(info_label, "Loading...");

  lv_obj_t *mode_row = labeled_row(left_cont, "Boot screen");
  mode_dd = lv_dropdown_create(mode_row);
  lv_dropdown_set_options(mode_dd, BOOT_MODE_OPTIONS);
  lv_obj_set_width(mode_dd, LV_PCT(55));
  lv_obj_align(mode_dd, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_add_event_cb(mode_dd, &SystemPanel::_handle_callback, LV_EVENT_VALUE_CHANGED, this);

  lv_obj_t *image_row = labeled_row(left_cont, "Boot image");
  image_dd = lv_dropdown_create(image_row);
  lv_dropdown_set_options(image_dd, "Built-in");
  lv_obj_set_width(image_dd, LV_PCT(55));
  lv_obj_align(image_dd, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_add_event_cb(image_dd, &SystemPanel::_handle_callback, LV_EVENT_VALUE_CHANGED, this);

  lv_obj_t *hint = lv_label_create(left_cont);
  lv_obj_set_width(hint, LV_PCT(100));
  lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, 0);
  lv_obj_set_style_text_color(hint, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);
  lv_label_set_text(hint, "Upload PNG/BMP boot images in Mainsail (nebulaos_boot folder). "
                          "Applies on the next boot.");

  logs_btn = text_btn(right_cont, LV_SYMBOL_LIST " Logs", &SystemPanel::_handle_callback, this);
  reboot_btn = text_btn(right_cont, LV_SYMBOL_REFRESH " Reboot", &SystemPanel::_handle_callback, this);
  shutdown_btn = text_btn(right_cont, LV_SYMBOL_POWER " Shut down", &SystemPanel::_handle_callback, this);
  lv_obj_set_style_bg_color(shutdown_btn, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);

  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_FLOATING);
  lv_obj_align(back_btn.get_container(), LV_ALIGN_BOTTOM_RIGHT, 0, 0);
}

SystemPanel::~SystemPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void SystemPanel::foreground() {
  lv_obj_move_foreground(cont);
  refresh();
}

void SystemPanel::report_error(json &reply, const char *what) {
  auto err = jsonrpc_error_message(reply);
  if (err.empty()) {
    return;
  }
  spdlog::warn("system panel: {}: {}", what, err);
  std::lock_guard<std::mutex> lock(lv_lock);
  KUtils::notify_toast(std::string(what) + ": " + err, 5000);
}

// Called from the LVGL thread; replies arrive on the websocket thread.
void SystemPanel::refresh() {
  ws.send_jsonrpc(SYSTEM_INFO_METHOD, json::object(), [this](json &reply) {
    auto info = system_info(reply);
    std::lock_guard<std::mutex> lock(lv_lock);
    lv_label_set_text(info_label, info.text.c_str());
    if (info.storage_warning) {
      lv_obj_set_style_text_color(info_label, lv_palette_main(LV_PALETTE_ORANGE), 0);
    } else {
      lv_obj_remove_local_style_prop(info_label, LV_STYLE_TEXT_COLOR, 0);
    }
  });
  refresh_boot_display();
}

void SystemPanel::refresh_boot_display() {
  json params = {{"root", "nebulaos_boot"}};
  ws.send_jsonrpc("server.files.list", params, [this](json &files_reply) {
    auto imgs = boot_images(files_reply);
    ws.send_jsonrpc(BOOT_DISPLAY_STATUS_METHOD, json::object(), [this, imgs](json &status) {
      std::lock_guard<std::mutex> lock(lv_lock);
      images = imgs;
      updating = true;
      lv_dropdown_set_options(image_dd, boot_image_options(images).c_str());
      lv_dropdown_set_selected(image_dd, boot_image_index(status, images));
      lv_dropdown_set_selected(mode_dd, boot_mode_index(status));
      updating = false;
    });
  });
}

void SystemPanel::confirm_power(bool reboot) {
  if (KUtils::is_printing()) {
    KUtils::notify_locked();
    return;
  }
  static const char *reboot_btns[] = {"Cancel", "Reboot", ""};
  static const char *shutdown_btns[] = {"Cancel", "Shut down", ""};
  lv_obj_t *mbox = lv_msgbox_create(NULL, NULL,
    reboot ? "Reboot the printer now?" : "Shut the printer down now?\n\nSwitch it off at the power switch afterwards.",
    reboot ? reboot_btns : shutdown_btns, false);
  lv_obj_set_user_data(mbox, reboot ? (void *)mbox : nullptr);  // non-null = reboot
  lv_obj_add_event_cb(mbox, [](lv_event_t *e) {
    lv_obj_t *obj = lv_obj_get_parent(lv_event_get_target(e));
    auto *panel = (SystemPanel *)lv_event_get_user_data(e);
    bool go = lv_msgbox_get_active_btn(obj) == 1;
    bool is_reboot = lv_obj_get_user_data(obj) != nullptr;
    lv_msgbox_close(obj);
    if (!go) {
      return;
    }
    // Re-check: a print may have started while the dialog was open.
    if (KUtils::is_printing()) {
      KUtils::notify_locked();
      return;
    }
    const char *method = is_reboot ? "machine.reboot" : "machine.shutdown";
    panel->ws.send_jsonrpc(method, json::object(), [panel, is_reboot](json &reply) {
      panel->report_error(reply, is_reboot ? "Reboot failed" : "Shutdown failed");
    });
    KUtils::notify_toast(is_reboot ? "Rebooting..." : "Shutting down...", 3000);
  }, LV_EVENT_VALUE_CHANGED, this);
  KUtils::style_lock_mbox(mbox, 90);
}

void SystemPanel::handle_callback(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *obj = lv_event_get_current_target(e);

  if (code == LV_EVENT_VALUE_CHANGED && !updating) {
    if (obj == mode_dd) {
      json params = {{"mode", boot_mode_at(lv_dropdown_get_selected(mode_dd))}};
      ws.send_jsonrpc(BOOT_DISPLAY_MODE_METHOD, params, [this](json &reply) {
        report_error(reply, "Boot screen not changed");
      });
    } else if (obj == image_dd) {
      uint16_t idx = lv_dropdown_get_selected(image_dd);
      if (idx == 0) {
        ws.send_jsonrpc(BOOT_DISPLAY_RESET_METHOD, json::object(), [this](json &reply) {
          report_error(reply, "Boot image not changed");
        });
      } else if ((size_t)(idx - 1) < images.size()) {
        json params = {{"filename", images[idx - 1]}};
        ws.send_jsonrpc(BOOT_DISPLAY_IMAGE_METHOD, params, [this](json &reply) {
          report_error(reply, "Boot image not changed");
        });
      }
    }
    return;
  }

  if (code != LV_EVENT_CLICKED) {
    return;
  }
  if (obj == back_btn.get_container()) {
    lv_obj_move_background(cont);
  } else if (obj == logs_btn) {
    logs_panel.foreground();
  } else if (obj == reboot_btn) {
    confirm_power(true);
  } else if (obj == shutdown_btn) {
    confirm_power(false);
  }
}
