#include "boot_logo_panel.h"
#include "nebulaos_display.h"
#include "settings_ui.h"
#include "utils.h"
#include "spdlog/spdlog.h"

LV_IMG_DECLARE(back);
LV_IMG_DECLARE(display_img);
LV_IMG_DECLARE(sd_img);
LV_IMG_DECLARE(delete_img);
LV_IMG_DECLARE(reset_img);

BootLogoPanel::BootLogoPanel(KWebSocketClient &c, std::mutex &lock)
  : ws(c)
  , lv_lock(lock)
  , cont(SettingsUi::page("Boot logo", &body))
  , back_btn(cont, &back, "Back", &BootLogoPanel::_handle_callback, this)
{
  SettingsUi::on_shown(cont, [this]() { shown(); });
  lv_obj_t *left = SettingsUi::column(body, 60);
  lv_obj_t *right = SettingsUi::column(body, 37);

  lv_obj_t *row = SettingsUi::row(left, "Logo");
  logo_dd = lv_dropdown_create(row);
  lv_dropdown_set_options(logo_dd, "NebulaOS");
  lv_obj_set_width(logo_dd, LV_PCT(70));
  lv_obj_align(logo_dd, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_add_event_cb(logo_dd, &BootLogoPanel::_handle_callback, LV_EVENT_VALUE_CHANGED, this);

  // What the boot screen will look like (rendered by NebulaOS, 240x136).
  preview = lv_img_create(left);
  lv_obj_set_size(preview, 240, 136);
  lv_obj_set_style_border_width(preview, 1, 0);
  lv_obj_set_style_border_color(preview, lv_palette_main(LV_PALETTE_GREY), 0);
  preview_note = SettingsUi::hint(left, "");
  power_label = SettingsUi::hint(left, "Power-on logo: ...");
  SettingsUi::hint(left, "Or upload a PNG, BMP or JPEG (max 4 MB) in Mainsail: Machine > config > "
                         "uploads > boot-logo. Best: 480 x 272, landscape.");

  apply_btn = SettingsUi::icon_button(right, &display_img, "Apply",
                                      &BootLogoPanel::_handle_callback, this);
  usb_btn = SettingsUi::icon_button(right, &sd_img, "Add from USB",
                                    &BootLogoPanel::_handle_callback, this);
  delete_btn = SettingsUi::icon_button(right, &delete_img, "Delete",
                                       &BootLogoPanel::_handle_callback, this);
  restore_btn = SettingsUi::icon_button(right, &reset_img, "Restore Creality logo",
                                        &BootLogoPanel::_handle_callback, this);
  lv_obj_add_state(restore_btn, LV_STATE_DISABLED);

  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_FLOATING);
  lv_obj_align(back_btn.get_container(), LV_ALIGN_BOTTOM_RIGHT, 0, 0);
}

BootLogoPanel::~BootLogoPanel() {
  close_usb_picker();
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void BootLogoPanel::foreground() {
  lv_obj_move_foreground(cont);
}

void BootLogoPanel::shown() {
  refresh("");
}

std::string BootLogoPanel::selected_name() {
  uint16_t idx = lv_dropdown_get_selected(logo_dd);
  return idx == 0 || idx > list.names.size() ? "" : list.names[idx - 1];
}

// LVGL thread; replies on the websocket thread.
void BootLogoPanel::refresh(const std::string &want) {
  ws.send_jsonrpc(BOOT_DISPLAY_IMAGES_METHOD, json::object(), [this, want](json &reply) {
    ws.send_jsonrpc(BOOT_DISPLAY_STATUS_METHOD, json::object(), [this, want, reply](json &status) {
      std::lock_guard<std::mutex> lock(lv_lock);
      list = boot_logo_list(reply);
      updating = true;
      lv_dropdown_set_options(logo_dd, list.options.c_str());
      int idx = want.empty() ? list.active : boot_logo_index(list, want);
      lv_dropdown_set_selected(logo_dd, idx);
      updating = false;
      auto p = power_on_view(status);
      lv_label_set_text(power_label, p.text.c_str());
      if (p.can_restore) lv_obj_clear_state(restore_btn, LV_STATE_DISABLED);
      else lv_obj_add_state(restore_btn, LV_STATE_DISABLED);
      show_preview();
    });
  });
}

// LVGL lock held by the caller.
void BootLogoPanel::show_preview() {
  std::string name = selected_name();
  uint16_t idx = lv_dropdown_get_selected(logo_dd);
  bool active = (int)idx == list.active;
  lv_label_set_text(preview_note, active ? "Active now." : "Not applied yet - tap Apply.");
  if (name.empty()) lv_obj_add_state(delete_btn, LV_STATE_DISABLED);
  else lv_obj_clear_state(delete_btn, LV_STATE_DISABLED);
  json params = {{"filename", name}};
  ws.send_jsonrpc(BOOT_DISPLAY_PREVIEW_METHOD, params, [this, name](json &reply) {
    std::string src = boot_logo_preview_src(reply);
    std::lock_guard<std::mutex> lock(lv_lock);
    if (selected_name() != name) return;        // the selection moved on meanwhile
    if (src.empty()) {
      lv_img_set_src(preview, NULL);
      std::string err = jsonrpc_error_message(reply);
      lv_label_set_text(preview_note, err.empty() ? "No preview." : boot_logo_error_text(err).c_str());
      return;
    }
    lv_img_cache_invalidate_src(src.c_str());
    lv_img_set_src(preview, src.c_str());
  });
}

// LVGL thread. Every change goes through here: run, report plainly, reload.
void BootLogoPanel::run(const char *method, const json &params, const char *what,
                        const std::string &select) {
  ws.send_jsonrpc(method, params, [this, what, select](json &reply) {
    std::string err = jsonrpc_error_message(reply);
    std::string msg = err.empty() ? power_on_result(reply) : boot_logo_error_text(err);
    std::string pick = select;
    if (err.empty() && reply.contains("result") && reply["result"].is_object()
        && reply["result"].contains("name") && reply["result"]["name"].is_string()) {
      pick = reply["result"]["name"].get<std::string>();       // Add from USB: select it
    }
    std::lock_guard<std::mutex> lock(lv_lock);
    if (!msg.empty()) {
      SettingsUi::notice(what, msg.c_str());
    }
    refresh(pick);
  });
}

void BootLogoPanel::apply() {
  std::string name = selected_name();
  if (name.empty()) {
    SettingsUi::confirm("Boot logo: NebulaOS",
      "The NebulaOS logo becomes the boot screen and the power-on logo of this boot slot. "
      "Shown from the next start.", "Apply", SettingsUi::Tone::Warning, [this]() {
        run(BOOT_DISPLAY_RESET_METHOD, json::object(), "Boot logo", "");
      });
    return;
  }
  SettingsUi::confirm(("Boot logo: " + name).c_str(),
    "This image becomes the boot screen and the power-on logo of this boot slot. "
    "Shown from the next start.", "Apply", SettingsUi::Tone::Warning, [this, name]() {
      run(BOOT_DISPLAY_IMAGE_METHOD, json{{"filename", name}}, "Boot logo", name);
    });
}

void BootLogoPanel::close_usb_picker() {
  if (usb_picker != NULL) {
    lv_obj_del(usb_picker);
    usb_picker = NULL;
  }
}

// G64 (2026-10-16): the stick is looked at again on every tap - the list
// from when the page opened missed a stick plugged in afterwards ("no
// picture found"). Then a small modal list of the images on it.
void BootLogoPanel::add_from_usb() {
  ws.send_jsonrpc(BOOT_DISPLAY_IMAGES_METHOD, json::object(), [this](json &reply) {
    BootLogoList fresh = boot_logo_list(reply);
    std::lock_guard<std::mutex> lock(lv_lock);
    list.usb = fresh.usb;
    list.usb_present = fresh.usb_present;
    open_usb_picker();
  });
}

// LVGL lock held by the caller.
void BootLogoPanel::open_usb_picker() {
  if (list.usb.empty()) {
    SettingsUi::notice("Add from USB", list.usb_present
      ? "No images on the USB stick. Use PNG, BMP or JPEG files (max 4 MB, 2048 x 2048)."
      : "No USB stick found. Plug one in with a PNG, BMP or JPEG image and try again.");
    return;
  }
  close_usb_picker();
  picker_items = list.usb;
  usb_picker = lv_obj_create(lv_layer_top());
  lv_obj_set_size(usb_picker, LV_PCT(70), LV_PCT(80));
  lv_obj_center(usb_picker);
  lv_obj_set_flex_flow(usb_picker, LV_FLEX_FLOW_COLUMN);
  lv_obj_t *title = lv_label_create(usb_picker);
  lv_label_set_text(title, "Add from USB: pick an image");
  lv_obj_t *l = lv_list_create(usb_picker);
  lv_obj_set_width(l, LV_PCT(100));
  lv_obj_set_flex_grow(l, 1);
  for (size_t i = 0; i < picker_items.size(); ++i) {
    lv_obj_t *b = lv_list_add_btn(l, LV_SYMBOL_IMAGE, picker_items[i].name.c_str());
    lv_obj_set_user_data(b, (void *)(uintptr_t)(i + 1));
    lv_obj_add_event_cb(b, &BootLogoPanel::_handle_callback, LV_EVENT_CLICKED, this);
  }
  lv_obj_t *cancel = lv_list_add_btn(l, LV_SYMBOL_CLOSE, "Cancel");
  lv_obj_set_user_data(cancel, (void *)(uintptr_t)0);
  lv_obj_add_event_cb(cancel, &BootLogoPanel::_handle_callback, LV_EVENT_CLICKED, this);
}

void BootLogoPanel::import_usb(const UsbImage &img) {
  close_usb_picker();
  run(BOOT_DISPLAY_IMPORT_METHOD, json{{"file", img.id}}, "Add from USB", img.name);
}

void BootLogoPanel::delete_selected() {
  std::string name = selected_name();
  if (name.empty()) return;
  SettingsUi::confirm("Delete image", ("Delete " + name + " from the boot logo folder?").c_str(),
    "Delete", SettingsUi::Tone::Danger, [this, name]() {
      run(BOOT_DISPLAY_DELETE_METHOD, json{{"filename", name}}, "Delete image", "");
    });
}

void BootLogoPanel::handle_callback(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *obj = lv_event_get_current_target(e);
  if (code == LV_EVENT_VALUE_CHANGED && obj == logo_dd) {
    if (!updating) show_preview();
    return;
  }
  if (code != LV_EVENT_CLICKED) return;
  if (usb_picker != NULL && lv_obj_get_parent(obj) != NULL
      && lv_obj_get_parent(lv_obj_get_parent(obj)) == usb_picker) {
    uintptr_t i = (uintptr_t)lv_obj_get_user_data(obj);
    if (i == 0 || i > picker_items.size()) {
      close_usb_picker();
    } else {
      UsbImage img = picker_items[i - 1];
      import_usb(img);
    }
    return;
  }
  if (obj == back_btn.get_container()) {
    close_usb_picker();
    lv_obj_move_background(cont);
  } else if (obj == apply_btn) {
    apply();
  } else if (obj == usb_btn) {
    add_from_usb();
  } else if (obj == delete_btn) {
    delete_selected();
  } else if (obj == restore_btn) {
    SettingsUi::confirm("Restore Creality logo",
      "The original power-on logo goes back into this boot slot's RTOS. The boot screen "
      "after it stays as it is. Shown from the next start.",
      "Restore", SettingsUi::Tone::Warning, [this]() {
        run(BOOT_DISPLAY_RESTORE_METHOD, json::object(), "Power-on logo", "");
      });
  }
}
