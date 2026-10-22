#include "updates_panel.h"

#include <algorithm>
#include "nebulaos_display.h"
#include "nebulaos_system.h"
#include "settings_ui.h"
#include "utils.h"
#include "spdlog/spdlog.h"

LV_IMG_DECLARE(back);
LV_IMG_DECLARE(refresh_img);
LV_IMG_DECLARE(update_img);
LV_IMG_DECLARE(sd_img);

UpdatesPanel::UpdatesPanel(KWebSocketClient &c, std::mutex &lock)
  : ws(c)
  , lv_lock(lock)
  , cont(SettingsUi::page("Updates", &body))
  , back_btn(cont, &back, "Back", &UpdatesPanel::_handle_callback, this)
  , image_panel(c, lock)
{
  SettingsUi::on_shown(cont, [this]() { shown(); });
  lv_obj_t *left = SettingsUi::column(body, 68);
  lv_obj_t *right = SettingsUi::column(body, 29);

  list = lv_obj_create(left);
  lv_obj_set_width(list, LV_PCT(100));
  lv_obj_set_flex_grow(list, 1);
  lv_obj_set_style_pad_all(list, 4, 0);
  lv_obj_set_style_pad_row(list, 4, 0);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);

  status_label = SettingsUi::hint(left, "");
  image_label = SettingsUi::hint(left, "");

  check_btn = SettingsUi::icon_button(right, &refresh_img, "Check now", &UpdatesPanel::_handle_callback, this);
  all_btn = SettingsUi::icon_button(right, &update_img, "Update all", &UpdatesPanel::_handle_callback, this);
  lv_obj_add_state(all_btn, LV_STATE_DISABLED);
  image_btn = SettingsUi::icon_button(right, &sd_img, "System image", &UpdatesPanel::_handle_callback, this);

  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_FLOATING);
  lv_obj_align(back_btn.get_container(), LV_ALIGN_BOTTOM_RIGHT, 0, 0);

  // G6: Moonraker's own progress lines while an update runs. They arrive on
  // the websocket thread. When Moonraker updates itself it restarts, the
  // upgrade reply never comes, and the "complete" line (or the next status,
  // see show()) is what ends the busy state.
  ws.register_method_callback("notify_update_response", "UpdatesPanel", [this](json &n) {
    auto p = update_progress(n);
    if (!p.valid) {
      return;
    }
    std::lock_guard<std::mutex> lock(lv_lock);
    if (p.complete) {
      set_busy("");
      lv_label_set_text(status_label, p.text.c_str());
      ws.send_jsonrpc(UPDATE_STATUS_METHOD, json::object(), [this](json &status) {
        std::lock_guard<std::mutex> lock(lv_lock);
        show(status);
      });
    } else if (busy) {
      lv_label_set_text(status_label, p.text.c_str());
    }
  });
}

UpdatesPanel::~UpdatesPanel() {
  if (recheck_timer != NULL) {
    lv_timer_del(recheck_timer);
  }
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void UpdatesPanel::foreground() {
  lv_obj_move_foreground(cont);
}

// Every time the page comes on top (SettingsUi::on_shown).
void UpdatesPanel::shown() {
  // A reply lost to a Moonraker restart must not keep the page busy forever.
  waiting_for_reply = false;
  refresh(false);
  read_readiness(false);
  ws.send_jsonrpc(SYSTEM_INFO_METHOD, json::object(), [this](json &reply) {
    auto info = system_info(reply);
    std::string text = updates_os_note(info.text);
    std::lock_guard<std::mutex> lock(lv_lock);
    lv_label_set_text(image_label, text.c_str());
  });
}

void UpdatesPanel::set_busy(const std::string &text) {
  busy = !text.empty();
  lv_label_set_text(status_label, text.c_str());
  for (lv_obj_t *b : row_btns) {
    if (busy) lv_obj_add_state(b, LV_STATE_DISABLED); else lv_obj_clear_state(b, LV_STATE_DISABLED);
  }
  if (busy) {
    lv_obj_add_state(check_btn, LV_STATE_DISABLED);
    lv_obj_add_state(all_btn, LV_STATE_DISABLED);
  } else {
    lv_obj_clear_state(check_btn, LV_STATE_DISABLED);
  }
}

// Called on the LVGL thread; replies arrive on the websocket thread.
void UpdatesPanel::refresh(bool check_remote) {
  if (check_remote) {
    set_busy("Checking for updates...");
    ws.send_jsonrpc(UPDATE_REFRESH_METHOD, json::object(), [this](json &reply) {
      auto err = jsonrpc_error_message(reply);
      {
        std::lock_guard<std::mutex> lock(lv_lock);
        set_busy("");
        if (!err.empty()) {
          lv_label_set_text(status_label, ("Check failed: " + err).c_str());
        }
      }
      ws.send_jsonrpc(UPDATE_STATUS_METHOD, json::object(), [this](json &status) {
        std::lock_guard<std::mutex> lock(lv_lock);
        show(status);
      });
      // G73: Moonraker may now know newer versions - can they be installed?
      read_readiness(true);
    });
    return;
  }
  ws.send_jsonrpc(UPDATE_STATUS_METHOD, json::object(), [this](json &status) {
    std::lock_guard<std::mutex> lock(lv_lock);
    show(status);
  });
}

// G73: NebulaOS's check whether a pending Klipper/Moonraker update can be
// installed from the package index (advisory). check_now runs the check
// (pip --dry-run on the printer, may take minutes); otherwise the last result.
void UpdatesPanel::read_readiness(bool check_now) {
  ws.send_jsonrpc(check_now ? UPDATE_CHECK_READINESS_METHOD : UPDATE_READINESS_METHOD,
                  json::object(), [this, check_now](json &reply) {
    if (check_now && !jsonrpc_error_message(reply).empty()) {
      return;
    }
    const json &r = reply.contains("result") ? reply["result"] : reply;
    std::string result = r.is_object() && r.contains("result") && r["result"].is_string()
      ? r["result"].get<std::string>() : "";
    // BUSY/PRINTING/UNCHANGED/RUNNING: the stored result still stands.
    if (check_now && result != "NOT_READY" && result != "READY" && result != "NO_UPDATES"
        && result != "UNKNOWN") {
      read_readiness(false);
      return;
    }
    std::lock_guard<std::mutex> lock(lv_lock);
    readiness = reply;
    if (!last_status.is_null()) show(last_status);
  });
}

// LVGL lock held by the caller.
void UpdatesPanel::show(const json &status_reply) {
  last_status = status_reply;
  rows = update_rows(status_reply);
  std::string waiting_text = apply_readiness(rows, readiness);
  row_btns.clear();
  lv_obj_clean(list);
  int available = 0;
  if (rows.empty()) {
    SettingsUi::hint(list, "No update information yet. Tap Check now.");
  }
  for (size_t i = 0; i < rows.size(); ++i) {
    const auto &r = rows[i];
    lv_obj_t *row = lv_obj_create(list);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(row, 30, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *name = lv_label_create(row);
    lv_label_set_text(name, r.label.c_str());
    lv_obj_set_style_text_font(name, &lv_font_montserrat_12, 0);
    lv_obj_align(name, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *detail = lv_label_create(row);
    lv_label_set_text(detail, (r.version + "  -  " + r.state).c_str());
    lv_obj_set_style_text_font(detail, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(detail, r.valid ? lv_palette_lighten(LV_PALETTE_GREY, 1)
                                                : lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_obj_align(detail, LV_ALIGN_TOP_LEFT, 0, 16);

    if (r.available) {
      ++available;
      lv_obj_t *b = lv_btn_create(row);
      lv_obj_set_size(b, 76, 28);
      lv_obj_align(b, LV_ALIGN_RIGHT_MID, 0, 0);
      lv_obj_t *l = lv_label_create(b);
      lv_label_set_text(l, "Update");
      lv_obj_center(l);
      lv_obj_set_user_data(b, (void *)(uintptr_t)i);
      lv_obj_add_event_cb(b, &UpdatesPanel::_handle_callback, LV_EVENT_CLICKED, this);
      row_btns.push_back(b);
    }
  }
  // G56: a repository Moonraker is still reading: ask again shortly.
  bool checking = std::any_of(rows.begin(), rows.end(), [](const UpdateRow &r) { return r.checking; });
  if (checking && recheck_timer == NULL) {
    recheck_timer = lv_timer_create([](lv_timer_t *t) {
      UpdatesPanel *self = (UpdatesPanel *)t->user_data;
      self->recheck_timer = NULL;
      lv_timer_del(t);
      if (!self->busy) self->refresh(false);
    }, 10000, this);
  }
  if (updates_busy(status_reply)) {
    set_busy("An update is running...");
  } else if (busy && !waiting_for_reply) {
    // Moonraker restarted during its own update: no update runs any more.
    set_busy("");
    lv_label_set_text(status_label, "Update finished.");
  } else if (!busy) {
    lv_label_set_text(status_label, !waiting_text.empty() ? waiting_text.c_str()
                                    : available ? "" : (rows.empty() ? "" : "Everything is up to date."));
  }
  if (available > 0 && !busy) {
    lv_obj_clear_state(all_btn, LV_STATE_DISABLED);
  } else {
    lv_obj_add_state(all_btn, LV_STATE_DISABLED);
  }
}

void UpdatesPanel::upgrade(const std::string &name, const std::string &label) {
  if (SettingsUi::refuse_if_printing()) {
    return;
  }
  std::string text = name.empty()
    ? "Install every available update now? Klipper stops while Klipper or the "
      "extensions update."
    : "Install the " + label + " update now?" +
      (name == "klipper" || name == "nebulaos-klipper-extensions"
         ? " Klipper stops during the update." : "");
  bool waiting = std::any_of(rows.begin(), rows.end(), [&name](const UpdateRow &r) {
    return r.waiting && (name.empty() || r.name == name);
  });
  if (waiting) {
    text += "\n\nThe NebulaOS package index does not have everything this version needs yet. "
            "The update will probably fail and be undone.";
  }
  SettingsUi::confirm(name.empty() ? "Update all" : ("Update " + label).c_str(), text.c_str(),
    "Update", SettingsUi::Tone::Normal, [this, name, label]() {
      set_busy("Updating " + (name.empty() ? std::string("everything") : label) +
               "... (the screen reconnects if Moonraker restarts)");
      json params = name.empty() ? json::object() : json{{"name", name}};
      waiting_for_reply = true;
      ws.send_jsonrpc(UPDATE_UPGRADE_METHOD, params, [this](json &reply) {
        auto err = jsonrpc_error_message(reply);
        {
          std::lock_guard<std::mutex> lock(lv_lock);
          waiting_for_reply = false;
          set_busy("");
          bool refused = SettingsUi::show_if_refused(reply);
          lv_label_set_text(status_label, err.empty() ? "Update finished."
                            : refused ? "Not updated." : ("Update failed: " + err).c_str());
        }
        ws.send_jsonrpc(UPDATE_STATUS_METHOD, json::object(), [this](json &status) {
          std::lock_guard<std::mutex> lock(lv_lock);
          show(status);
        });
      });
    });
}

void UpdatesPanel::handle_callback(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
    return;
  }
  lv_obj_t *obj = lv_event_get_current_target(e);
  if (obj == back_btn.get_container()) {
    lv_obj_move_background(cont);
  } else if (obj == check_btn) {
    refresh(true);
  } else if (obj == all_btn) {
    upgrade("", "");
  } else if (obj == image_btn) {
    image_panel.foreground();
  } else {
    size_t i = (size_t)(uintptr_t)lv_obj_get_user_data(obj);
    if (i < rows.size()) {
      upgrade(rows[i].name, rows[i].label);
    }
  }
}

#ifdef SIMULATOR
void UpdatesPanel::sim_fill() {
  show(json::parse(R"({"result":{"busy":false,"version_info":{
    "klipper":{"version":"v0.13.0-58","remote_version":"v0.13.0-58","is_valid":true,"commits_behind_count":0},
    "moonraker":{"version":"v0.9.3-12","remote_version":"v0.9.3-15","is_valid":true,"commits_behind_count":3},
    "mainsail":{"version":"v2.13.0","remote_version":"v2.14.0"},
    "nebulaos-klipper-extensions":{"version":"8f86925","remote_version":"8f86925","is_valid":true,"commits_behind_count":0}}}})"));
  lv_label_set_text(image_label, updates_os_note("NebulaOS df415dc  (2026-10-11), slot B").c_str());
}
#endif
