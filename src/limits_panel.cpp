#include "settings_ui.h"
#include "limits_panel.h"
#include "state.h"
#include "spdlog/spdlog.h"

#include <cmath>

LV_IMG_DECLARE(refresh_img);
LV_IMG_DECLARE(back);

// The pinned upstream Klipper replaced max_accel_to_decel / ACCEL_TO_DECEL with
// minimum_cruise_ratio (0 <= ratio < 1); SET_VELOCITY_LIMIT ignores ACCEL_TO_DECEL.
static int ratio_to_pct(double ratio) {
  int pct = (int)std::lround(ratio * 100.0);
  return pct < 0 ? 0 : (pct > 99 ? 99 : pct);
}

LimitsPanel::LimitsPanel(KWebSocketClient &c, std::mutex &l)
  : NotifyConsumer(l)
  , ws(c)
  , cont(lv_obj_create(lv_scr_act()))
  , limit_cont(lv_obj_create(cont))
  , velocity(limit_cont, "Velocity (mm/s)", &refresh_img, "Reset", &refresh_img, NULL, &LimitsPanel::_handle_callback, this, "")
  , acceleration(limit_cont, "Acceleration (mm/s2)", &refresh_img, "Reset", &refresh_img, NULL, &LimitsPanel::_handle_callback, this, "")
  , square_corner(limit_cont, "Square Corner Velocity (mm/s)", &refresh_img, "Reset", &refresh_img, NULL, &LimitsPanel::_handle_callback, this, "")
  , min_cruise_ratio(limit_cont, "Minimum Cruise Ratio (%)", &refresh_img, "Reset", &refresh_img, NULL, &LimitsPanel::_handle_callback, this, "")
  , back_btn(cont, &back, "Back", &LimitsPanel::_handle_callback, this)
  , max_velocity_default(1000)
  , max_accel_default(20000)
  , min_cruise_ratio_default(50)
  , square_corner_default(5)
{
  lv_obj_move_background(cont);
  lv_obj_set_style_pad_all(cont, 0, 0);
  lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

  // G94 (Run 18): the list stops left of the Back tile (66 px + margins), so
  // Back never sits over a slider
  lv_obj_align(limit_cont, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_size(limit_cont, lv_pct(84), lv_pct(100));
  lv_obj_set_flex_flow(limit_cont, LV_FLEX_FLOW_COLUMN);

  // G94 (Run 18): the one title and Back button of every page
  SettingsUi::page_title(cont, "Limits");
  SettingsUi::place_back(back_btn.get_container());

  // Klipper accepts 0 <= MINIMUM_CRUISE_RATIO < 1.
  min_cruise_ratio.set_range(0, 99);

  ws.register_notify_update(this);
}

LimitsPanel::~LimitsPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void LimitsPanel::init(json &j) {
  State *s = State::get_instance();
  auto v = s->get_data("/printer_state/configfile/settings/printer"_json_pointer);
  if (!v.is_null()) {
    if (v.contains("max_velocity")) {
      max_velocity_default = v["max_velocity"].template get<int>();      
      velocity.set_range(1, max_velocity_default);
    }

    if (v.contains("max_accel")) {
      max_accel_default = v["max_accel"].template get<int>();
      acceleration.set_range(1, max_accel_default);
    }

    if (v.contains("minimum_cruise_ratio") && v["minimum_cruise_ratio"].is_number()) {
      min_cruise_ratio_default = ratio_to_pct(v["minimum_cruise_ratio"].template get<double>());
    }

    if (v.contains("square_corner_velocity")) {
      square_corner_default = v["square_corner_velocity"].template get<int>();
      square_corner.set_range(0, square_corner_default);
    }
    
  }
  
  v = j["/result/status/toolhead/max_velocity"_json_pointer];
  if (!v.is_null()) {
    velocity.update_value(v.template get<int>());
  }

  v = j["/result/status/toolhead/max_accel"_json_pointer];
  if (!v.is_null()) {
    acceleration.update_value(v.template get<int>());
  }

  v = j["/result/status/toolhead/minimum_cruise_ratio"_json_pointer];
  if (v.is_number()) {
    min_cruise_ratio.update_value(ratio_to_pct(v.template get<double>()));
  }

  v = j["/result/status/toolhead/square_corner_velocity"_json_pointer];
  if (!v.is_null()) {
    square_corner.update_value(v.template get<int>());
  }
}

void LimitsPanel::foreground() {
  lv_obj_move_foreground(cont);
}

void LimitsPanel::consume(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);
  auto v = j["/params/0/toolhead/max_velocity"_json_pointer];
  if (!v.is_null()) {
    velocity.update_value(v.template get<int>());
  }

  v = j["/params/0/toolhead/max_accel"_json_pointer];
  if (!v.is_null()) {
    acceleration.update_value(v.template get<int>());
  }

  v = j["/params/0/toolhead/minimum_cruise_ratio"_json_pointer];
  if (v.is_number()) {
    min_cruise_ratio.update_value(ratio_to_pct(v.template get<double>()));
  }

  v = j["/params/0/toolhead/square_corner_velocity"_json_pointer];
  if (!v.is_null()) {
    square_corner.update_value(v.template get<int>());
  }
  
}

void LimitsPanel::handle_callback(lv_event_t *e) {
  lv_obj_t *btn = lv_event_get_current_target(e);

  if (btn == back_btn.get_container()) {
    lv_obj_move_background(cont);
    return;
  }

  if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
    lv_obj_t *obj = lv_event_get_target(e);
    int v = lv_slider_get_value(obj);

    if (obj == velocity.get_slider()) {
      ws.gcode_script(fmt::format("SET_VELOCITY_LIMIT VELOCITY={}", v));

    } else if (obj == acceleration.get_slider()) {
      ws.gcode_script(fmt::format("SET_VELOCITY_LIMIT ACCEL={}", v));

    } else if (obj == min_cruise_ratio.get_slider()) {
      ws.gcode_script(fmt::format("SET_VELOCITY_LIMIT MINIMUM_CRUISE_RATIO={:.2f}", v / 100.0));
      
    } else if (obj == square_corner.get_slider()) {
      ws.gcode_script(fmt::format("SET_VELOCITY_LIMIT SQUARE_CORNER_VELOCITY={}", v));

    }
    
  } else if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    if (btn == velocity.get_off()) {
      ws.gcode_script(fmt::format("SET_VELOCITY_LIMIT VELOCITY={}", max_velocity_default));

    } else if (btn == acceleration.get_off()) {
      ws.gcode_script(fmt::format("SET_VELOCITY_LIMIT ACCEL={}", max_accel_default));

    } else if (btn == min_cruise_ratio.get_off()) {
      ws.gcode_script(fmt::format("SET_VELOCITY_LIMIT MINIMUM_CRUISE_RATIO={:.2f}",
        min_cruise_ratio_default / 100.0));
      
    } else if (btn == square_corner.get_off()) {
      ws.gcode_script(fmt::format("SET_VELOCITY_LIMIT SQUARE_CORNER_VELOCITY={}", square_corner_default));

    }
  }
}
