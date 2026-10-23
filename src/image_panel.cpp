#include "image_panel.h"
#include "settings_ui.h"
#include "utils.h"
#include "spdlog/spdlog.h"

LV_IMG_DECLARE(back);
LV_IMG_DECLARE(refresh_img);

// Unpacking and writing ~100 MB takes minutes; past this the screen stops
// waiting and says so (the install itself is not affected).
static const uint32_t INSTALL_GIVE_UP_MS = 30 * 60 * 1000;
static const uint32_t POLL_MS = 2000;
static const char *const DONT_SWITCH_OFF =
  "Do not switch the printer off. The printer keeps running this version until "
  "it restarts into the new one.";

ImagePanel::ImagePanel(KWebSocketClient &c, std::mutex &lock)
  : ws(c)
  , lv_lock(lock)
  , cont(SettingsUi::page("System image", &body))
  , back_btn(cont, &back, "Back", &ImagePanel::_handle_callback, this)
{
  SettingsUi::on_shown(cont, [this]() { shown(); });
  lv_obj_t *left = SettingsUi::column(body, 68);
  lv_obj_set_style_pad_row(left, 3, 0);
  lv_obj_t *right = SettingsUi::column(body, 29);

  slot_a = SettingsUi::info_line(left, "Slot A");
  slot_b = SettingsUi::info_line(left, "Slot B");

  list = lv_obj_create(left);
  lv_obj_set_width(list, LV_PCT(100));
  lv_obj_set_flex_grow(list, 1);
  lv_obj_set_style_pad_all(list, 4, 0);
  lv_obj_set_style_pad_row(list, 4, 0);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);

  SettingsUi::hint(left, "Put a KE .img on a USB stick, or upload it "
                         "in Mainsail (Machine > config > uploads > system-image), then tap Refresh.");

  refresh_btn = SettingsUi::icon_button(right, &refresh_img, "Refresh",
                                        &ImagePanel::_handle_callback, this);

  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_FLOATING);
  SettingsUi::place_back(back_btn.get_container());  // G94: one Back position
}

ImagePanel::~ImagePanel() {
  if (poll_timer != NULL) {
    lv_timer_del(poll_timer);
  }
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void ImagePanel::foreground() {
  lv_obj_move_foreground(cont);
}

// Every time the page comes on top (SettingsUi::on_shown).
void ImagePanel::shown() {
  refresh();
  // An install started before (the screen restarted meanwhile): follow it.
  if (!working) {
    ws.send_jsonrpc(IMAGE_STATUS_METHOD, json::object(), [this](json &st) {
      std::lock_guard<std::mutex> lock(lv_lock);
      if (!working && image_job(st).active) {
        start_busy("Installing system image", image_job(st).text);
      }
    });
  }
}

// Called on the LVGL thread; replies arrive on the websocket thread.
void ImagePanel::refresh() {
  lv_obj_clean(list);
  SettingsUi::hint(list, "Looking for system images...");
  ws.send_jsonrpc(IMAGE_LIST_METHOD, json::object(), [this](json &reply) {
    std::lock_guard<std::mutex> lock(lv_lock);
    show_files(reply);
  });
}

// LVGL lock held by the caller.
void ImagePanel::show_files(const json &list_reply) {
  files = image_files(list_reply);
  lv_obj_clean(list);
  if (!files.ok) {
    SettingsUi::hint(list, files.error.c_str());
    return;
  }
  lv_label_set_text(slot_a, image_slot_line(files.slots, "A").substr(8).c_str());
  lv_label_set_text(slot_b, image_slot_line(files.slots, "B").substr(8).c_str());
  if (files.files.empty()) {
    SettingsUi::hint(list, files.usb_present ? "No .img files in the upload folder or on the USB stick."
                                             : "No .img files found.");
  }
  for (size_t i = 0; i < files.files.size(); ++i) {
    const auto &f = files.files[i];
    lv_obj_t *row = lv_obj_create(list);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(row, 30, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *text = lv_obj_create(row);
    lv_obj_set_height(text, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(text, 1);
    lv_obj_set_style_pad_all(text, 0, 0);
    lv_obj_set_style_pad_row(text, 1, 0);
    lv_obj_set_style_border_width(text, 0, 0);
    lv_obj_set_style_bg_opa(text, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(text, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(text, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *name = lv_label_create(text);
    lv_obj_set_width(name, LV_PCT(100));
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    lv_label_set_text(name, f.name.c_str());
    lv_obj_set_style_text_font(name, &lv_font_montserrat_12, 0);

    lv_obj_t *detail = lv_label_create(text);
    lv_obj_set_width(detail, LV_PCT(100));
    lv_label_set_text(detail, f.detail.c_str());
    lv_obj_set_style_text_font(detail, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(detail, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);

    lv_obj_t *b = lv_btn_create(row);
    lv_obj_set_size(b, 66, 28);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, "Install");
    lv_obj_center(l);
    lv_obj_set_user_data(b, (void *)(uintptr_t)(i + 1));
    lv_obj_add_event_cb(b, &ImagePanel::_handle_callback, LV_EVENT_CLICKED, this);
    if (working) lv_obj_add_state(b, LV_STATE_DISABLED);
  }
}

// LVGL thread. Checking reads the whole file (checksum) and the package
// listing: some seconds, so a box says so.
void ImagePanel::check(const ImageFile &f) {
  if (working) return;
  if (SettingsUi::refuse_if_printing()) {
    return;
  }
  working = true;
  busy_label = SettingsUi::busy("Checking system image", (f.name + "\n\nThis takes a moment.").c_str());
  ws.send_jsonrpc(IMAGE_CHECK_METHOD, image_check_params(f.id), [this](json &reply) {
    std::lock_guard<std::mutex> lock(lv_lock);
    checked(reply);
  });
}

// LVGL lock held.
void ImagePanel::checked(const json &check_reply) {
  if (busy_label != NULL) {
    SettingsUi::close_busy(busy_label);
    busy_label = NULL;
  }
  working = false;
  if (SettingsUi::show_if_refused(check_reply)) {
    return;
  }
  ImageCheck c = image_check(check_reply);
  if (!c.ok) {
    SettingsUi::notice(c.title.c_str(), c.text.c_str());
    return;
  }
  SettingsUi::Tone tone = c.system == "nebulaos" ? SettingsUi::Tone::Warning
                                                 : SettingsUi::Tone::Danger;
  SettingsUi::confirm(c.title.c_str(), c.text.c_str(), "Install & restart", tone, [this, c]() {
    if (SettingsUi::refuse_if_printing()) {   // a print may have started meanwhile
      return;
    }
    start_busy("Installing system image", "Starting...");
    ws.send_jsonrpc(IMAGE_INSTALL_METHOD, image_install_params(c), [this](json &reply) {
      std::string err = image_install_error(reply);
      if (err.empty()) {
        return;     // the poll follows the job from here
      }
      std::lock_guard<std::mutex> lock(lv_lock);
      stop_busy();
      if (SettingsUi::show_if_refused(reply)) {
        return;
      }
      SettingsUi::notice("Not installed", err.c_str());
    });
  });
}

// LVGL thread.
void ImagePanel::start_busy(const char *title, const std::string &text) {
  working = true;
  lv_obj_add_state(refresh_btn, LV_STATE_DISABLED);
  busy_since = lv_tick_get();
  busy_label = SettingsUi::busy(title, (text + "\n\n" + DONT_SWITCH_OFF).c_str());
  poll_timer = lv_timer_create([](lv_timer_t *t) { ((ImagePanel *)t->user_data)->poll(); },
                               POLL_MS, this);
}

// LVGL thread.
void ImagePanel::poll() {
  if (busy_label == NULL) return;
  if (lv_tick_elaps(busy_since) > INSTALL_GIVE_UP_MS) {
    stop_busy();
    SettingsUi::notice("No answer", "The install did not report back. Open System image "
                                    "again in a minute to see what each slot holds.");
    return;
  }
  ws.send_jsonrpc(IMAGE_STATUS_METHOD, json::object(), [this](json &st) {
    std::lock_guard<std::mutex> lock(lv_lock);
    show_job(st);
  });
}

// LVGL lock held. While the printer restarts Moonraker stops answering; the
// last text ("Restarting into slot X") stays until the screen goes away.
void ImagePanel::show_job(const json &status_reply) {
  if (busy_label == NULL) return;
  ImageJob j = image_job(status_reply);
  if (j.failed) {
    stop_busy();
    // G91: a print started during the install - stopped at a point of no return
    SettingsUi::notice(j.stopped ? "Install stopped" : "Install failed", j.text.c_str());
    refresh();
    return;
  }
  if (j.next_power_on) {
    stop_busy();
    SettingsUi::notice("Install finished", j.text.c_str());
    refresh();
    return;
  }
  if (!j.active) return;   // no answer (restarting) or not started yet
  std::string text = j.text + "\n\n" + (j.restarting ? "The screen comes back after the restart."
                                                     : DONT_SWITCH_OFF);
  lv_label_set_text(busy_label, text.c_str());
}

// LVGL lock held.
void ImagePanel::stop_busy() {
  if (poll_timer != NULL) {
    lv_timer_del(poll_timer);
    poll_timer = NULL;
  }
  if (busy_label != NULL) {
    SettingsUi::close_busy(busy_label);
    busy_label = NULL;
  }
  working = false;
  lv_obj_clear_state(refresh_btn, LV_STATE_DISABLED);
}

void ImagePanel::handle_callback(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
    return;
  }
  lv_obj_t *obj = lv_event_get_current_target(e);
  if (obj == back_btn.get_container()) {
    lv_obj_move_background(cont);
  } else if (obj == refresh_btn) {
    refresh();
  } else {
    size_t i = (size_t)(uintptr_t)lv_obj_get_user_data(obj);
    if (i >= 1 && i <= files.files.size()) {
      check(files.files[i - 1]);
    }
  }
}

#ifdef SIMULATOR
void ImagePanel::sim_fill() {
  show_files(json::parse(R"({"result":{"usb":["sda1"],"upload_dir":"/opt/printer_data/config/uploads/system-image",
    "slots":{"running":"B","A":{"kernel":"Linux-4.4.94","system":"creality"},
             "B":{"kernel":"Linux-6.6.157-rt79","system":"nebulaos"}},
    "files":[
      {"id":"upload/NebulaOS-KE-2026-10-15.img","source":"upload","name":"NebulaOS-KE-2026-10-15.img","size":98412544},
      {"id":"usb/sda1/Ender-3_V3_KE_F005_ota_img_V1.1.0.12.img","source":"usb",
       "name":"Ender-3_V3_KE_F005_ota_img_V1.1.0.12.img","size":112283648}]}})"));
}

void ImagePanel::sim_check(const char *reply_json) {
  checked(json::parse(reply_json));
}

void ImagePanel::sim_progress(const char *status_json) {
  if (busy_label == NULL) start_busy("Installing system image", "Starting...");
  show_job(json::parse(status_json));
}
#endif
