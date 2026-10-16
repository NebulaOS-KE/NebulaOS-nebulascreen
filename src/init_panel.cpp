#include "init_panel.h"
#include "nebulaos_image.h"
#include "nebulaos_system.h"
#include "settings_ui.h"
#include "utils.h"
#include "state.h"
#include "config.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cstdio>

InitPanel::InitPanel(MainPanel &mp, BedMeshPanel &bmp, FilamentRunoutPanel &frp, std::mutex& l)
  : cont(lv_obj_create(lv_scr_act()))
  , label(lv_label_create(cont))
  , main_panel(mp)
  , bedmesh_panel(bmp)
  , filament_runout_panel(frp)
  , lv_lock(l)
{
  lv_obj_set_size(cont, LV_PCT(55), LV_SIZE_CONTENT);
  lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 15);  
  
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(cont, lv_palette_darken(LV_PALETTE_GREY, 1), 0);
  
  lv_obj_set_size(label, LV_PCT(100), LV_SIZE_CONTENT);

  Config *conf = Config::get_instance();
  if (!conf->get_json("/default_printer").is_null()) {
    lv_label_set_text(label, LV_SYMBOL_WARNING " Waiting for printer to initialize...");
  } else {
    lv_label_set_text(label, "Welcome to Guppy Screen. Use the Setting Panel to add your printers.");
  }
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}

InitPanel::~InitPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void InitPanel::connected(KWebSocketClient &ws) {
  spdlog::debug("init panel connected");
  State *state = State::get_instance();
  state->reset();

  ws.send_jsonrpc("printer.objects.list", [this, &ws](json& d) {
    State *state = State::get_instance();
	state->set_data("printer_objs", d, "/result");

	ws.send_jsonrpc("server.files.roots",
			[](json& j) { State::get_instance()->set_data("roots", j, "/result"); });

	ws.send_jsonrpc("printer.info",
			[](json& j) { State::get_instance()->set_data("printer_info", j, "/result"); });
	
	json h = {
	  { "namespace", "fluidd" },
	  { "key", "console" }
	};
	ws.send_jsonrpc("server.database.get_item", h,
			[](json& j) { State::get_instance()->set_data("console", j, "/result/value"); });

	h = {
	  { "namespace", "guppyscreen" }
	};
	ws.send_jsonrpc("server.database.get_item", h,
			[](json& j) { State::get_instance()->set_data("guppysettings", j, "/result/value"); });	

	// G2: once after a clean NebulaOS reset, say that Wi-Fi has to be
	// connected again, then tell NebulaOS the notice was shown.
	// G65: NebulaOS's own one-time notices (e.g. config/nebulaos edits kept
	// by an update), each shown once and acknowledged.
	ws.send_jsonrpc(SYSTEM_INFO_METHOD, json::object(), [this, &ws](json &j) {
	  for (const auto &n : system_notices(j)) {
	    {
	      std::lock_guard<std::mutex> lock(this->lv_lock);
	      SettingsUi::notice("NebulaOS", n.text.c_str());
	    }
	    ws.send_jsonrpc(ACK_NOTICE_METHOD, json{{"id", n.id}}, [](json &) {});
	  }
	  if (!after_clean_reset(j)) {
	    return;
	  }
	  {
	    std::lock_guard<std::mutex> lock(this->lv_lock);
	    SettingsUi::notice(AFTER_RESET_TITLE, AFTER_RESET_TEXT);
	  }
	  ws.send_jsonrpc(ACK_CLEAN_RESET_METHOD, json::object(), [](json &) {});
	});

	// Once after a system image install: what the trial boot found.
	this->ws = &ws;
	image_result(40);

	// console
	this->main_panel.subscribe();

	// spoolman
	ws.send_jsonrpc("server.info", [this, &ws](json &j) {
	  spdlog::debug("server_info {}", j.dump());
	  State::get_instance()->set_data("server_info", j, "/result");
	  
	  auto &components = j["/result/components"_json_pointer];
	  if (!components.is_null()) {
	    const auto &has_spoolman = components.template get<std::vector<std::string>>();
	    if (std::find(has_spoolman.begin(), has_spoolman.end(), "spoolman") != has_spoolman.end()) {
	      this->main_panel.enable_spoolman();
	    }
	    // Power devices only exist with a [power] section (NebulaOS ships
	    // none); without it every machine.device_power.* call is an error.
	    if (std::find(has_spoolman.begin(), has_spoolman.end(), "power") != has_spoolman.end()) {
	      ws.send_jsonrpc("machine.device_power.devices", [this](json& d) {
	        main_panel.get_tune_panel().get_power_panel().create_devices(d);
	      });
	    }
	  }
	});

	auto display_sensors = state->get_display_sensors();
	this->main_panel.create_sensors(display_sensors);

	auto display_fans = state->get_display_fans();
	this->main_panel.create_fans(display_fans);

	auto display_leds = state->get_display_leds();
	this->main_panel.create_leds(display_leds);

	// subscribe to all objects except gcode_macro
	auto objs = d["/result/objects"_json_pointer];
	if (!objs.is_null()) {
	  json sub_objs;
	  bool has_m600_macro = false;
	  for (auto &obj : objs) {
	    std::string obj_name = obj.template get<std::string>();
	    if (obj_name.rfind("gcode_macro ", 0 ) != 0) {
	      sub_objs[obj_name] = nullptr;
	    } else if (obj_name == "gcode_macro M600") {
	      // gcode_macro objects are deliberately excluded from the live
	      // subscription above, so FilamentRunoutPanel can never discover
	      // this itself from printer_state (found for real on-device
	      // 2026-07-18 - it silently showed its own dialog on every
	      // runout regardless of M600 being installed, since printer_state
	      // never contains a "gcode_macro M600" key at all). This is the
	      // one place that ever sees the real, unfiltered object list.
	      has_m600_macro = true;
	    }
	  }
	  filament_runout_panel.set_has_m600_macro(has_m600_macro);

	  sub_objs["tmcstatus"] = nullptr;

	  json subs = {{ "objects", sub_objs }};
	  spdlog::debug("subcribing to {}", subs.dump());
	  ws.send_jsonrpc("printer.objects.subscribe", subs,
			  [this](json &data) {
			    State::get_instance()->set_data("printer_state",
							    data, "/result/status");
			    this->main_panel.init(data);
			    this->bedmesh_panel.refresh_views_with_lock(
									data["/result/status/bed_mesh"_json_pointer]);
			    spdlog::debug("done init");
			    std::lock_guard<std::mutex> lock(this->lv_lock);
			    lv_obj_add_flag(this->cont, LV_OBJ_FLAG_HIDDEN);
			    lv_obj_move_background(this->cont);
			  });
	}
  });

}

void InitPanel::disconnected(KWebSocketClient &ws) {
  spdlog::debug("init panel disconnected");
  std::lock_guard<std::mutex> lock(lv_lock);
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(cont);
}

#ifdef SIMULATOR
void InitPanel::sim_hide() {
  lv_obj_add_flag(cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_background(cont);
}
#endif

void InitPanel::set_message(const char *message) {
	lv_label_set_text(label, message);
}

// Once after a system image install: what the trial boot found (confirmed, or
// went back to the previous slot), then acknowledge it. The trial decides only
// after Klipper and this screen are up, usually after the first connect, so
// while it is pending ask again every 10 s (`tries` times, ~7 minutes).
void InitPanel::image_result(int tries) {
  ws->send_jsonrpc(IMAGE_STATUS_METHOD, json::object(), [this, tries](json &j) {
    auto last = image_last(j);
    std::lock_guard<std::mutex> lock(this->lv_lock);
    if (last.show) {
      SettingsUi::notice(last.title.c_str(), last.text.c_str());
      ws->send_jsonrpc(IMAGE_ACK_METHOD, json::object(), [](json &) {});
      return;
    }
    if (!image_trial_pending(j) || tries <= 0 || image_timer != NULL) {
      return;
    }
    image_timer = lv_timer_create([](lv_timer_t *t) {
      auto *self = (InitPanel *)t->user_data;
      lv_timer_del(t);
      self->image_timer = NULL;
      self->image_result(self->image_tries);
    }, 10000, this);
    image_tries = tries - 1;
  });
}
