#include "mcu_panel.h"
#include "nebulaos_system.h"
#include "settings_ui.h"
#include "utils.h"
#include "spdlog/spdlog.h"

LV_IMG_DECLARE(back);
LV_IMG_DECLARE(refresh_img);
LV_IMG_DECLARE(chip_img);

// The helper bounds a flash at 600 s and Moonraker waits 660 s; past that the
// screen stops waiting (the reply, if it still comes, is shown anyway).
static const uint32_t FLASH_GIVE_UP_MS = 700 * 1000;

McuPanel::McuPanel(KWebSocketClient &c, std::mutex &lock)
  : ws(c)
  , lv_lock(lock)
  , cont(SettingsUi::page("MCU firmware", &body))
  , back_btn(cont, &back, "Back", &McuPanel::_handle_callback, this)
{
  SettingsUi::on_shown(cont, [this]() { shown(); });
  lv_obj_t *left = SettingsUi::column(body, 64);
  lv_obj_set_style_pad_row(left, 3, 0);
  lv_obj_t *right = SettingsUi::column(body, 33);

  running_value = SettingsUi::info_line(left, "Running");
  reported_value = SettingsUi::info_line(left, "Version");
  owner_value = SettingsUi::info_line(left, "Owner");

  list = lv_obj_create(left);
  lv_obj_set_width(list, LV_PCT(100));
  lv_obj_set_flex_grow(list, 1);
  lv_obj_set_style_pad_all(list, 4, 0);
  lv_obj_set_style_pad_row(list, 4, 0);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);

  SettingsUi::hint(left, "Add a .bin in Mainsail (Machine > config > uploads > mainboard-firmware) or "
                         "plug in a USB stick, then tap Refresh.");

  refresh_btn = SettingsUi::icon_button(right, &refresh_img, "Refresh",
                                        &McuPanel::_handle_callback, this);
  // G94 (Run 18): two lines, not three (the page title already says MCU)
  native_btn = SettingsUi::icon_button(right, &chip_img, "Reinstall\nbuilt-in firmware",
                                       &McuPanel::_handle_callback, this);
  lv_obj_set_height(native_btn, 44);
  lv_obj_add_state(native_btn, LV_STATE_DISABLED);

  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_FLOATING);
  SettingsUi::place_back(back_btn.get_container());  // G94: one Back position
}

McuPanel::~McuPanel() {
  if (busy_timer != NULL) {
    lv_timer_del(busy_timer);
  }
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void McuPanel::foreground() {
  lv_obj_move_foreground(cont);
}

// Every time the page comes on top (SettingsUi::on_shown).
void McuPanel::shown() {
  refresh();
}

// Called on the LVGL thread; replies arrive on the websocket thread.
void McuPanel::refresh() {
  lv_obj_clean(list);
  SettingsUi::hint(list, "Looking for firmware files...");
  ws.send_jsonrpc(MCU_STATUS_METHOD, json::object(), [this](json &st) {
    std::lock_guard<std::mutex> lock(lv_lock);
    show_status(st);
  });
  ws.send_jsonrpc(MCU_FILES_METHOD, json::object(), [this](json &reply) {
    std::lock_guard<std::mutex> lock(lv_lock);
    show_files(reply);
  });
}

// LVGL lock held by the caller.
void McuPanel::show_status(const json &status_reply) {
  auto v = mcu_status_view(status_reply);
  lv_label_set_text(running_value, v.running.c_str());
  lv_label_set_text(reported_value, v.reported.c_str());
  std::string owner = v.ownership + (v.last.empty() ? "" : "\nLast install: " + v.last);
  lv_label_set_text(owner_value, owner.c_str());
}

// LVGL lock held by the caller.
void McuPanel::show_files(const json &files_reply) {
  files = mcu_files(files_reply);
  lv_obj_clean(list);
  if (files.files.empty()) {
    SettingsUi::hint(list, files.usb_present ? "No .bin files in the upload folder or on the USB stick."
                                             : "No .bin files found.");
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
    lv_label_set_long_mode(detail, LV_LABEL_LONG_WRAP);
    lv_label_set_text(detail, f.detail.c_str());
    lv_obj_set_style_text_font(detail, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(detail, f.warn ? lv_palette_main(LV_PALETTE_ORANGE)
                                               : lv_palette_lighten(LV_PALETTE_GREY, 1), 0);
    if (f.valid) {
      lv_obj_t *b = lv_btn_create(row);
      lv_obj_set_size(b, 66, 28);
      lv_obj_t *l = lv_label_create(b);
      lv_label_set_text(l, "Install");
      lv_obj_center(l);
      lv_obj_set_user_data(b, (void *)(uintptr_t)(i + 1));
      lv_obj_add_event_cb(b, &McuPanel::_handle_callback, LV_EVENT_CLICKED, this);
      if (flashing) lv_obj_add_state(b, LV_STATE_DISABLED);
    }
  }
  if (files.has_builtin && !flashing) {
    lv_obj_clear_state(native_btn, LV_STATE_DISABLED);
  } else {
    lv_obj_add_state(native_btn, LV_STATE_DISABLED);
  }
}

void McuPanel::install(const McuFile &f) {
  if (flashing) return;
  if (SettingsUi::refuse_if_printing()) {
    return;
  }
  McuFile chosen = f;
  SettingsUi::confirm(f.native ? "Reinstall built-in MCU firmware" : "Install MCU firmware",
    mcu_confirm_text(f).c_str(), "Install", SettingsUi::Tone::Danger, [this, chosen]() {
      if (SettingsUi::refuse_if_printing()) {   // a print may have started meanwhile
        return;
      }
      start_busy();
      ws.send_jsonrpc(MCU_FLASH_METHOD, mcu_flash_params(chosen), [this](json &reply) {
        {
          std::lock_guard<std::mutex> lock(lv_lock);
          finish(reply);
        }
      });
    });
}

// LVGL thread.
void McuPanel::start_busy() {
  flashing = true;
  lv_obj_add_state(native_btn, LV_STATE_DISABLED);
  lv_obj_add_state(refresh_btn, LV_STATE_DISABLED);
  busy_since = lv_tick_get();
  busy_label = SettingsUi::busy("Installing MCU firmware",
    "Do not switch the printer off. Klipper is stopped and comes back by itself.");
  busy_timer = lv_timer_create([](lv_timer_t *t) { ((McuPanel *)t->user_data)->tick_busy(); },
                               1000, this);
  tick_busy();
}

// LVGL thread: seconds so far, and give up after FLASH_GIVE_UP_MS.
void McuPanel::tick_busy() {
  if (busy_label == NULL) return;
  uint32_t ms = lv_tick_elaps(busy_since);
  if (ms > FLASH_GIVE_UP_MS) {
    finish(json{{"error", {{"message",
      "TIMEOUT: no answer from the printer. Open MCU firmware again in a minute to see "
      "what is installed"}}}});
    return;
  }
  std::string text = "Do not switch the printer off. Klipper is stopped and comes back "
                     "by itself.\n\n" + std::to_string(ms / 1000) + " s";
  lv_label_set_text(busy_label, text.c_str());
}

// LVGL lock held. Also reached a second time if the reply arrives after the
// screen gave up - then only the result is shown.
void McuPanel::finish(const json &reply) {
  if (busy_timer != NULL) {
    lv_timer_del(busy_timer);
    busy_timer = NULL;
  }
  if (busy_label != NULL) {
    SettingsUi::close_busy(busy_label);
    busy_label = NULL;
  }
  if (refusal_of(reply).refused) {
    // G91: refused before anything was stopped - Klipper keeps running
    flashing = false;
    lv_obj_clear_state(refresh_btn, LV_STATE_DISABLED);
    SettingsUi::show_if_refused(reply);
    refresh();
    return;
  }
  auto r = mcu_flash_result(reply);
  if (r.ok) {
    // G34 (2026-10-16): the result says what the board runs, once Klipper
    // has reached it again - whatever firmware it is.
    check_board(r, lv_tick_get());
    return;
  }
  flashing = false;
  lv_obj_clear_state(refresh_btn, LV_STATE_DISABLED);
  SettingsUi::notice(r.title.c_str(), r.text.c_str());
  refresh();
}

// LVGL lock held. Asks Klipper for the MCU's version every 3 s, up to
// BOARD_CHECK_MS; then the result popup.
void McuPanel::check_board(const McuFlashResult &r, uint32_t since) {
  if (busy_label == NULL) {
    busy_label = SettingsUi::busy("Installing MCU firmware", "Installed. Checking the board...");
  }
  json q = {{"objects", {{"mcu", {"mcu_version"}}}}};
  ws.send_jsonrpc("printer.objects.query", q, [this, r, since](json &reply) {
    std::string version = mcu_running_version(reply);
    std::lock_guard<std::mutex> lock(lv_lock);
    bool late = lv_tick_elaps(since) > BOARD_CHECK_MS;
    if (version.empty() && !late) {
      lv_timer_t *t = lv_timer_create([](lv_timer_t *t) {
        auto *ctx = (std::pair<McuPanel *, std::pair<McuFlashResult, uint32_t>> *)t->user_data;
        ctx->first->check_board(ctx->second.first, ctx->second.second);
        delete ctx;
        lv_timer_del(t);
      }, 3000, new std::pair<McuPanel *, std::pair<McuFlashResult, uint32_t>>(this, {r, since}));
      lv_timer_set_repeat_count(t, 1);
      return;
    }
    if (busy_label != NULL) {
      SettingsUi::close_busy(busy_label);
      busy_label = NULL;
    }
    flashing = false;
    lv_obj_clear_state(refresh_btn, LV_STATE_DISABLED);
    refresh();
    if (!version.empty()) {
      SettingsUi::notice(r.title.c_str(), ("Done - the board runs " + version + ".").c_str());
      return;
    }
    if (!files.has_builtin) {
      SettingsUi::notice("Klipper can't reach the board",
        "The firmware was written, but Klipper does not reach the mainboard.");
      return;
    }
    SettingsUi::confirm("Klipper can't reach the board",
      "The firmware was written, but Klipper does not reach the mainboard. Reinstall the "
      "built-in MCU firmware?", "Reinstall built-in", SettingsUi::Tone::Warning,
      [this]() { if (files.has_builtin) install(files.builtin); });
  });
}

void McuPanel::handle_callback(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
    return;
  }
  lv_obj_t *obj = lv_event_get_current_target(e);
  if (obj == back_btn.get_container()) {
    lv_obj_move_background(cont);
  } else if (obj == refresh_btn) {
    refresh();
  } else if (obj == native_btn) {
    if (files.has_builtin) install(files.builtin);
  } else {
    size_t i = (size_t)(uintptr_t)lv_obj_get_user_data(obj);
    if (i >= 1 && i <= files.files.size()) {
      install(files.files[i - 1]);
    }
  }
}

#ifdef SIMULATOR
void McuPanel::sim_fill() {
  std::string a(64, 'a'), c(64, 'c'), b(64, 'b');
  show_status(json::parse(R"({"result":{"managed":true,"guard":{"result":"PASS",
    "application_class":"NATIVE_CANDIDATE_001","application_identity":"v0.13.0-742-g01a9c2f92"},
    "last_flash":null}})"));
  show_files(json::parse(R"({"result":{"usb":["sda1"],
    "builtin":{"id":"builtin/candidate-001.bin","source":"builtin","name":"Built-in MCU firmware",
      "sha256":")" + c + R"(","native":true,"size":40048,"valid":true,"kind":"packed"},
    "files":[
      {"id":"upload/klipper.bin","source":"upload","name":"klipper.bin","size":40112,
       "valid":true,"kind":"raw","sha256":")" + a + R"(","native":false,
       "klipper_version":"v0.13.0-118-g1a2b3c4d"},
      {"id":"upload/printer.cfg.bin","source":"upload","name":"printer.cfg.bin","size":4096,
       "valid":false,"sha256":")" + b + R"(","native":false,
       "problem":"not firmware for this printer's mainboard: no vendor header and no NebulaOS placeholder at 0x200"},
      {"id":"usb/sda1/firmware/ke-klipper-v0.13.bin","source":"usb","name":"firmware/ke-klipper-v0.13.bin",
       "size":40210,"valid":true,"kind":"packed","sha256":")" + a + R"(","native":false,
       "klipper_version":"v0.12.0-301-gbeef0011",
       "version_note":"built from Klipper v0.12.0, NebulaOS uses v0.13.0: Klipper will probably not connect"}]}})"));
}

void McuPanel::sim_install(size_t index) {
  if (index < files.files.size()) install(files.files[index]);
}

void McuPanel::sim_busy() {
  start_busy();
  busy_since = lv_tick_get() - 23000;
  tick_busy();
}

void McuPanel::sim_result(const char *reply_json) {
  finish(json::parse(reply_json));
}
#endif
