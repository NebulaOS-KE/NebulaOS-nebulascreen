#include "print_status_panel.h"
#include "finetune_panel.h"
#include "settings_ui.h"
#include "state.h"
#include "utils.h"
#include "spdlog/spdlog.h"

#include <cmath>


LV_IMG_DECLARE(extruder);
LV_IMG_DECLARE(speed_up_img);
LV_IMG_DECLARE(extrude);
LV_IMG_DECLARE(clock_img);
LV_IMG_DECLARE(hourglass);
LV_IMG_DECLARE(bed);
LV_IMG_DECLARE(home_z);
LV_IMG_DECLARE(fan);
LV_IMG_DECLARE(heater);
LV_IMG_DECLARE(layers_img);

LV_IMG_DECLARE(fine_tune_img);
LV_IMG_DECLARE(delete_img);
LV_IMG_DECLARE(pause_img);
LV_IMG_DECLARE(resume);
LV_IMG_DECLARE(cancel);
LV_IMG_DECLARE(emergency);
LV_IMG_DECLARE(back);

double pi() { return std::atan(1)*4; }

PrintStatusPanel::PrintStatusPanel(KWebSocketClient &websocket_client,
				   std::mutex &lock,
				   lv_obj_t *mini_parent)
  : NotifyConsumer(lock)
  , ws(websocket_client)
  , finetune_panel(websocket_client, lock)
  , zoffset_panel(websocket_client, lock)
  , exclude_object_panel(websocket_client, lock)
  , mini_print_status(mini_parent, &PrintStatusPanel::_handle_callback, this)
  , status_cont(lv_obj_create(lv_scr_act()))
  , buttons_cont(lv_obj_create(status_cont))
  , finetune_btn(buttons_cont, &fine_tune_img, "Fine Tune", &PrintStatusPanel::_handle_callback, this)
  , objects_btn(buttons_cont, &delete_img, "Objects", &PrintStatusPanel::_handle_callback, this)
  , pause_btn(buttons_cont, &pause_img, "Pause", &PrintStatusPanel::_handle_callback, this)
  , resume_btn(buttons_cont, &resume, "Resume", &PrintStatusPanel::_handle_callback, this)
  , cancel_btn(buttons_cont, &cancel, "Cancel", &PrintStatusPanel::_handle_callback, this)
  , emergency_btn(buttons_cont, &emergency, "Stop", &PrintStatusPanel::_handle_callback, this,
		  ButtonContainer::ESTOP_PROMPT,
		  [&websocket_client]() {
		    spdlog::debug("emergency stop pressed");
		    websocket_client.send_jsonrpc("printer.emergency_stop");
		  })
  , back_btn(buttons_cont, &back, "Back", &PrintStatusPanel::_handle_callback, this)
  , thumbnail_cont(lv_obj_create(status_cont))
  , filename_label(lv_label_create(thumbnail_cont))
  , thumbnail(lv_img_create(thumbnail_cont))
  , pbar_cont(lv_obj_create(thumbnail_cont))
  , progress_bar(lv_bar_create(pbar_cont))
  , progress_label(lv_label_create(pbar_cont))
  , detail_cont(lv_obj_create(status_cont))
  , extruder_temp(detail_cont, &extruder, 100, "20")
  , bed_temp(detail_cont, &bed, 100, "21")
  , chamber_temp(detail_cont, &heater, 100, "22")
  , print_speed(detail_cont, &speed_up_img, 100, "0 mm/s")
  , z_offset(detail_cont, &home_z, 100, "0.0 mm")
  , flow_rate(detail_cont, &extrude, 100, "0.0 mm3/s")
  , layers(detail_cont, &layers_img, 100, "...")
  , fan0(detail_cont, &fan, 100, "0%")
  , elapsed(detail_cont, &clock_img, 100, "0:00:00")
  , time_left(detail_cont, &hourglass, 100, "--")
  , filament_diameter(1.75) // XXX: check config
  , extruder_target(-1)
  , heater_bed_target(-1)
  , chamber_target(-1)
{
  lv_obj_move_background(status_cont);
  lv_obj_clear_flag(status_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(status_cont, LV_PCT(100), LV_PCT(100));

  static lv_coord_t grid_main_row_dsc_detail[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
    LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  static lv_coord_t grid_main_col_dsc_detail[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(detail_cont, grid_main_col_dsc_detail, grid_main_row_dsc_detail);

  lv_obj_clear_flag(detail_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(detail_cont, 0, 0);

  //detail containter row 1
  lv_obj_set_grid_cell(extruder_temp.get_container(), LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
  lv_obj_set_grid_cell(bed_temp.get_container(), LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 0, 1);

  //detail containter row 2
  lv_obj_set_grid_cell(chamber_temp.get_container(), LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
  lv_obj_set_grid_cell(print_speed.get_container(), LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 1, 1);

  //detail containter row 3
  lv_obj_set_grid_cell(z_offset.get_container(), LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 2, 1);
  lv_obj_set_grid_cell(flow_rate.get_container(), LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 2, 1);
  // tap the Z-offset metric to open the baby-stepping aid during a print
  lv_obj_add_flag(z_offset.get_container(), LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(z_offset.get_container(), &PrintStatusPanel::_handle_callback,
    LV_EVENT_CLICKED, this);

  //detail containter row 4
  lv_obj_set_grid_cell(layers.get_container(), LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 3, 1);
  lv_obj_set_grid_cell(elapsed.get_container(), LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 3, 1);

  //detail containter row 5
  lv_obj_set_grid_cell(fan0.get_container(), LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 4, 1);
  lv_obj_set_grid_cell(time_left.get_container(), LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 4, 1);
  // lv_obj_set_grid_cell(fan2.get_container(), LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_START, 4, 1);

  static lv_coord_t grid_main_row_dsc[] = {LV_GRID_FR(1), LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
  static lv_coord_t grid_main_col_dsc[] = {LV_GRID_FR(4), LV_GRID_FR(5), LV_GRID_TEMPLATE_LAST};

  lv_obj_set_grid_dsc_array(status_cont, grid_main_col_dsc, grid_main_row_dsc);

  lv_obj_set_size(buttons_cont, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_clear_flag(buttons_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(buttons_cont, 0, 0);
  lv_obj_set_flex_flow(buttons_cont, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(buttons_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_set_flex_grow(finetune_btn.get_container(), 1);
  lv_obj_set_flex_grow(objects_btn.get_container(), 1);
  lv_obj_set_flex_grow(pause_btn.get_container(), 1);
  lv_obj_set_flex_grow(resume_btn.get_container(), 1);
  lv_obj_set_flex_grow(cancel_btn.get_container(), 1);
  lv_obj_set_flex_grow(emergency_btn.get_container(), 1);
  lv_obj_set_flex_grow(back_btn.get_container(), 1);

  // Hidden until a print with labelled objects is active (see consume()).
  lv_obj_add_flag(objects_btn.get_container(), LV_OBJ_FLAG_HIDDEN);

  lv_obj_set_style_pad_all(pbar_cont, 0, 0);
  lv_obj_set_size(pbar_cont, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_clear_flag(pbar_cont, LV_OBJ_FLAG_SCROLLABLE);

  auto hscale = (double)lv_disp_get_physical_ver_res(NULL) / 480.0;

  lv_obj_set_size(progress_bar, LV_PCT(100), 20 * hscale);
  lv_bar_set_value(progress_bar, 0, LV_ANIM_OFF);

  lv_label_set_text(progress_label, "0%");
  lv_obj_center(progress_label);

  lv_obj_set_flex_flow(thumbnail_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(thumbnail_cont, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_size(thumbnail_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(thumbnail_cont, 0, 0);
  lv_obj_clear_flag(thumbnail_cont, LV_OBJ_FLAG_SCROLLABLE);

  // Filename label: between thumbnail and progress bar. lv_label_create defaults
  // to no width, so set a percent width + DOT long-mode so a 60-char gcode name
  // truncates instead of overflowing the column. Move to index 1 (after
  // thumbnail, before pbar_cont) so the column reads top-to-bottom:
  // thumbnail / filename / progress bar.
  lv_obj_set_width(filename_label, LV_PCT(100));
  lv_label_set_long_mode(filename_label, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(filename_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(filename_label, "");
  lv_obj_move_to_index(filename_label, 1);

  // G112: while START_PRINT runs, its steps stand in for the thumbnail.
  start_label = lv_label_create(thumbnail_cont);
  lv_obj_set_width(start_label, LV_PCT(100));
  lv_label_set_long_mode(start_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(start_label, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_line_space(start_label, 3, 0);
  lv_obj_move_to_index(start_label, 0);
  lv_obj_add_flag(start_label, LV_OBJ_FLAG_HIDDEN);

  // row 1
  lv_obj_set_grid_cell(thumbnail_cont, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
  lv_obj_set_grid_cell(detail_cont, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 0, 1);

  //row 2
  lv_obj_set_grid_cell(buttons_cont, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_STRETCH, 1, 1);

  // ---- completion overlay (own full-screen layer, shown by show_completion) ----
  complete_cont = lv_obj_create(lv_scr_act());
  lv_obj_move_background(complete_cont);
  lv_obj_set_size(complete_cont, LV_PCT(100), LV_PCT(100));
  lv_obj_clear_flag(complete_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(complete_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(complete_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(complete_cont, 12, 0);

  complete_title = lv_label_create(complete_cont);
  lv_obj_set_style_text_font(complete_title, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_align(complete_title, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(complete_title, "");

  complete_file = lv_label_create(complete_cont);
  lv_obj_set_width(complete_file, LV_PCT(80));
  lv_label_set_long_mode(complete_file, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(complete_file, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(complete_file, &lv_font_montserrat_16, 0);
  lv_label_set_text(complete_file, "");

  complete_stats = lv_label_create(complete_cont);
  lv_obj_set_style_text_align(complete_stats, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(complete_stats, lv_palette_lighten(LV_PALETTE_GREY, 2), 0);
  lv_label_set_text(complete_stats, "");

  complete_done_btn = lv_btn_create(complete_cont);
  lv_obj_set_size(complete_done_btn, 160, 48);
  lv_obj_t *done_lbl = lv_label_create(complete_done_btn);
  lv_label_set_text(done_lbl, "Done");
  lv_obj_center(done_lbl);
  lv_obj_add_event_cb(complete_done_btn, &PrintStatusPanel::_handle_callback, LV_EVENT_CLICKED, this);

  ws.register_notify_update(this);
}

PrintStatusPanel::~PrintStatusPanel() {
  if (status_cont != NULL) {
    lv_obj_del(status_cont);
    status_cont = NULL;
  }

  ws.unregister_notify_update(this);
}

void PrintStatusPanel::foreground() {
  // populate();
  lv_obj_move_foreground(status_cont);
}

void PrintStatusPanel::background() {
  lv_obj_move_background(status_cont);
}

void PrintStatusPanel::show_completion(const std::string &state) {
  State *s = State::get_instance();
  bool ok = (state == "complete");

  lv_label_set_text(complete_title, ok ? LV_SYMBOL_OK "  Print complete"
                                       : LV_SYMBOL_CLOSE "  Print cancelled");
  lv_obj_set_style_text_color(complete_title,
    ok ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_RED), 0);

  auto fn = s->get_data("/printer_state/print_stats/filename"_json_pointer);
  lv_label_set_text(complete_file, fn.is_string() ? fn.template get<std::string>().c_str() : "");

  std::string stats;
  auto dur = s->get_data("/printer_state/print_stats/print_duration"_json_pointer);
  if (dur.is_number()) {
    int t = (int)dur.template get<double>();
    int h = t / 3600, m = (t % 3600) / 60, sec = t % 60;
    stats = h > 0 ? fmt::format("Time  {}h {:02d}m", h, m)
                  : fmt::format("Time  {}m {:02d}s", m, sec);
  }
  auto fil = s->get_data("/printer_state/print_stats/filament_used"_json_pointer);
  if (fil.is_number()) {
    stats += fmt::format("     Filament  {:.2f} m", fil.template get<double>() / 1000.0);
  }
  lv_label_set_text(complete_stats, stats.c_str());

  lv_obj_move_background(status_cont);
  lv_obj_clear_flag(complete_cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(complete_cont);
}

void PrintStatusPanel::reset() {
  lv_obj_move_background(complete_cont);  // a new print clears any completion summary
  lv_bar_set_value(progress_bar, 0, LV_ANIM_OFF);
  lv_label_set_text(progress_label, "0%");
  print_speed.update_label("0 mm/s");
  flow_rate.update_label("0.0 mm3/s");
  elapsed.update_label("0:00:00");
  time_left.update_label("--");
  est_in = PrintEstimateInput();
  current_file = json();
  fetch_estimate_settings();
  lv_label_set_text(filename_label, "");

  auto v = State::get_instance()
    ->get_data("/printer_state/configfile/config/extruder/filament_diameter"_json_pointer);
  filament_diameter = v.is_null() ? 1.750 : std::stod(v.template get<std::string>());
  extruder_target = -1;
  heater_bed_target = -1;

  // free src
  lv_img_set_src(thumbnail, NULL);
  // hack to color in empty space.
  ((lv_img_t*)thumbnail)->src_type = LV_IMG_SRC_SYMBOL;

  mini_print_status.reset();
}

void PrintStatusPanel::init(json &fans) {
  fan_speeds.clear();
  std::vector<std::string> values;
  for (auto &f : fans.items()) {
    std::string fan_name = f.key();

    auto fan_value = State::get_instance()
      ->get_data(json::json_pointer(fmt::format("/printer_state/{}/value", fan_name)));
    if (!fan_value.is_null()) {
      int v = static_cast<int>(fan_value.template get<double>() * 100);
      fan_speeds.insert({fan_name, v});
      values.push_back(fmt::format("{}%", v));
    }

    fan_value = State::get_instance()
      ->get_data(json::json_pointer(fmt::format("/printer_state/{}/speed", fan_name)));
    if (!fan_value.is_null()) {
      int v = static_cast<int>(fan_value.template get<double>() * 100);
      fan_speeds.insert({fan_name, v});
      values.push_back(fmt::format("{}%", v));
    }
  }

  fan0.update_label(fmt::format("{}", fmt::join(values, ", ")).c_str());

  reset();
  populate();
  json pstat_state = State::get_instance()
    ->get_data("/printer_state/print_stats/state"_json_pointer);
  if (!pstat_state.is_null()) {
    auto pstatus = pstat_state.template get<std::string>();
    // Overlay rides over Homing/Extrude, so show it only while actively
    // printing; when paused swap it for the small chip; when idle hide both.
    if (pstatus == "paused") {
      mini_print_status.hide();
      mini_print_status.show_chip();
    } else if (pstatus != "printing") {
      mini_print_status.hide();
      mini_print_status.hide_chip();
    } else {
      mini_print_status.hide_chip();
    }
    mini_print_status.update_status(pstatus);
  } else {
    mini_print_status.show();
  }

}

void PrintStatusPanel::populate() {
  State* s = State::get_instance();
  json printfile = s->get_data("/printer_state/print_stats/filename"_json_pointer);
  if (!printfile.is_null()) {
    const std::string fname = printfile.template get<std::string>();
    if (fname.length() > 0) {
      lv_label_set_text(filename_label, fname.c_str());
      json fname_input = {{"filename", fname }};
      ws.send_jsonrpc("server.files.metadata", fname_input,
		      [fname, this](json &d) { this->handle_metadata(fname, d); });

      mini_print_status.show();
    }
  }

  auto pstate = s->get_data("/printer_state/print_stats/state"_json_pointer);
  if (!pstate.is_null() && pstate.template get<std::string>() == "paused") {
    lv_obj_clear_flag(resume_btn.get_container(), LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(pause_btn.get_container(), LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(resume_btn.get_container(), LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(pause_btn.get_container(), LV_OBJ_FLAG_HIDDEN);
  }

  // progress percentage (Mainsail's calculation)
  absorb_estimate_fields(s->get_data("/printer_state"_json_pointer));
  update_progress_display();

  auto v = s->get_data(
      "/printer_state/gcode_move/homing_origin/2"_json_pointer);
  if (!v.is_null()) {
    z_offset.update_label(fmt::format("{:.5} mm", v.template get<double>()).c_str());
  }

  auto v_target = s->get_data("/printer_state/extruder/target"_json_pointer);
  if (!v_target.is_null()) extruder_target = v_target.template get<int>();

  v_target = s->get_data("/printer_state/heater_bed/target"_json_pointer);
  if (!v_target.is_null()) heater_bed_target = v_target.template get<int>();

  std::string chamber_id = "temperature_sensor chamber_temp";
  json display_sensors = s->get_display_sensors();
  for (auto& item : display_sensors.items()) {
    if (item.value().contains("display_name") && item.value()["display_name"] == "Chamber") {
      chamber_id = item.key();
      break;
    }
  }
  v_target = s->get_data(json::json_pointer(fmt::format("/printer_state/{}/target", chamber_id)));
  if (!v_target.is_null()) chamber_target = v_target.template get<int>();
}

void PrintStatusPanel::handle_metadata(const std::string &gcode_file, json &j) {
  current_file = j["/result"_json_pointer];
  {
    // the file's own facts Mainsail's progress/estimates use
    auto num = [this](const char *key, double &out) {
      auto v = current_file.is_object() ? current_file.find(key) : current_file.end();
      bool ok = current_file.is_object() && v != current_file.end() && v->is_number();
      out = ok ? v->template get<double>() : 0.0;
      return ok;
    };
    est_in.file_filename = gcode_file;
    num("gcode_start_byte", est_in.gcode_start_byte);
    num("gcode_end_byte", est_in.gcode_end_byte);
    est_in.have_filament_total = num("filament_total", est_in.filament_total);
    std::lock_guard<std::mutex> lock(lv_lock);
    update_progress_display();
    update_time_progress();
  }

  auto thumb_detail = KUtils::get_thumbnail(gcode_file, j);
  std::string fullpath = thumb_detail.first;
  if (fullpath.length() > 0) {
    spdlog::trace("thumb path: {}", fullpath);
    std::lock_guard<std::mutex> lock(lv_lock);
    const std::string img_path = "A:" + fullpath;

    lv_img_set_src(thumbnail, img_path.c_str());
    auto thumb_w = thumb_detail.second.first;
    auto thumb_h = thumb_detail.second.second;

    auto available_w = lv_obj_get_width(thumbnail_cont);
    auto available_h = lv_obj_get_height(thumbnail_cont) - lv_obj_get_height(pbar_cont) - lv_obj_get_style_pad_row(thumbnail_cont, LV_PART_MAIN);

    double scale_x = (double)available_w / (double)thumb_w;
    double scale_y = (double)available_h / (double)thumb_h;
    double scale = std::min(scale_x, scale_y);

    uint32_t lv_zoom = static_cast<uint32_t>(scale * 256.0);
    lv_img_set_pivot(thumbnail, thumb_w / 2, thumb_h);
    lv_img_set_zoom(thumbnail, lv_zoom);
    mini_print_status.update_img(img_path, thumb_detail.second.first);
  }
}


void PrintStatusPanel::consume(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);

  // Only surface the Exclude Objects button when the active print actually has
  // labelled objects - otherwise it just crowds the button row.
  auto objs = State::get_instance()->get_data("/printer_state/exclude_object/objects"_json_pointer);
  if (objs.is_array() && !objs.empty()) {
    lv_obj_clear_flag(objects_btn.get_container(), LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(objects_btn.get_container(), LV_OBJ_FLAG_HIDDEN);
  }

  // G112: START_PRINT's step and the temperatures it waits for.
  {
    bool changed = false;
    static const auto step_ptr = "/params/0/gcode_macro START_PRINT/step"_json_pointer;
    static const auto soak_ptr = "/params/0/gcode_macro START_PRINT/soak_seconds"_json_pointer;
    json st = j.contains(step_ptr) ? j[step_ptr] : json();
    if (st.is_string()) {
      std::string step = st.template get<std::string>();
      if (step == "preparing" && start_in.step != "preparing") start_in.soaked = false;
      if (step == "soak" && start_in.step != "soak") {
        start_in.soaked = true;
        soak_seen = std::chrono::steady_clock::now();
      }
      start_in.step = step;
      changed = true;
    }
    if (j.contains(soak_ptr) && j[soak_ptr].is_number()) {
      start_in.soak_seconds = j[soak_ptr].template get<int>();
      changed = true;
    }
    for (auto *p : {"/params/0/heater_bed", "/params/0/extruder"}) {
      if (j.contains(json::json_pointer(p))) changed = true;
    }
    if (changed) update_start();
  }

  auto printfile = j["/params/0/print_stats/filename"_json_pointer];
  if (!printfile.is_null()) {
    // filename change indicates a start of a print
    reset();
    populate();
    foreground(); // auto move to front when print is detected
  }

  auto& pstate = j["/params/0/print_stats/state"_json_pointer];
  if (!pstate.is_null()) {
    auto print_status = pstate.template get<std::string>();
    if (print_status != "printing" && print_status != "paused") {
      mini_print_status.hide();
      mini_print_status.hide_chip();
      // Print just ended (#94). Show a completion summary (result + stats +
      // Done) instead of dismissing - otherwise the user is left with no
      // confirmation. Other terminal states (e.g. standby) just dismiss.
      if (last_print_state == "printing" || last_print_state == "paused") {
        if (print_status == "complete" || print_status == "cancelled") {
          show_completion(print_status);
        } else {
          lv_obj_move_background(status_cont);
        }
      }
    } else if (print_status == "paused") {
      // Swap the big overlay for a small "Paused" chip so the Homing/Extrude
      // buttons it sat over are revealed and tappable (purge/load filament,
      // jog) while a tap still returns to the full status panel. Don't dismiss
      // the full status panel - the job isn't over.
      mini_print_status.hide();
      mini_print_status.show_chip();
    } else {
      // actively printing: make sure any leftover completion summary is gone
      // (covers a new print started externally, including a same-file reprint)
      lv_obj_move_background(complete_cont);
      mini_print_status.show();
      mini_print_status.hide_chip();
    }

    mini_print_status.update_status(print_status);
    last_print_state = print_status;
  }

  // Keep the on-screen filename label in sync with mid-print rename / new file
  // events. The state-update notify also surfaces these.
  if (!printfile.is_null()) {
    lv_label_set_text(filename_label, printfile.template get<std::string>().c_str());
  }

  // the inputs of Mainsail's progress / time-left calculation
  if (j.contains("params") && j["params"].is_array() && !j["params"].empty()) {
    absorb_estimate_fields(j["params"][0]);
  }

  auto v = j["/params/0/extruder/target"_json_pointer];
  if (!v.is_null()) {
    extruder_target = v.template get<int>();
  }

  v = j["/params/0/heater_bed/target"_json_pointer];
  if (!v.is_null()) {
    heater_bed_target = v.template get<int>();
  }

  v = j["/params/0/extruder/temperature"_json_pointer];
  if (!v.is_null()) {
    if (extruder_target > 0) {
      extruder_temp.update_label(fmt::format("{} / {}", v.template get<int>(), extruder_target).c_str());
    } else {
      extruder_temp.update_label(fmt::format("{}", v.template get<int>()).c_str());
    }
  }

  v = j["/params/0/heater_bed/temperature"_json_pointer];
  if (!v.is_null()) {
    if (heater_bed_target > 0) {
      bed_temp.update_label(fmt::format("{} / {}", v.template get<int>(), heater_bed_target).c_str());
    } else {
      bed_temp.update_label(fmt::format("{}", v.template get<int>()).c_str());
    }
  }

  bool chamber_temp_found = false;
  int parsed_chamber_temp = 0;

  std::string chamber_id = "temperature_sensor chamber_temp";
  json display_sensors = State::get_instance()->get_display_sensors();
  for (auto& item : display_sensors.items()) {
    if (item.value().contains("display_name") && item.value()["display_name"] == "Chamber") {
      chamber_id = item.key();
      break;
    }
  }

  // Use the exact configured chamber_id, which handles both target and temperature
  auto target_ptr = json::json_pointer(fmt::format("/params/0/{}/target", chamber_id));
  if (j.contains(target_ptr)) {
    chamber_target = j[target_ptr].template get<int>();
  }

  auto temp_ptr = json::json_pointer(fmt::format("/params/0/{}/temperature", chamber_id));
  if (j.contains(temp_ptr)) {
    parsed_chamber_temp = j[temp_ptr].template get<int>();
    chamber_temp_found = true;
  }

  if (chamber_temp_found) {
    if (chamber_target > 0) {
      chamber_temp.update_label(fmt::format("{} / {}", parsed_chamber_temp, chamber_target).c_str());
    } else {
      chamber_temp.update_label(fmt::format("{}", parsed_chamber_temp).c_str());
    }
  }
  // speed
  auto speed = j["/params/0/motion_report/live_velocity"_json_pointer];
  if (!speed.is_null()) {
    int s = static_cast<int>(speed.template get<double>());
    print_speed.update_label((std::to_string(s) + " mm/s").c_str());
  }

  // zoffset
  v = j["/params/0/gcode_move/homing_origin/2"_json_pointer];
  if (!v.is_null()) {
    z_offset.update_label(fmt::format("{:.5} mm", v.template get<double>()).c_str());
  }

  std::vector<std::string> values;
  for (auto &f : fan_speeds) {
    std::string fan_name = f.first;

    int fv = f.second;
    auto fan_value = j[json::json_pointer(fmt::format("/params/0/{}/value", fan_name))];
    if (!fan_value.is_null()) {
      fv = static_cast<int>(fan_value.template get<double>() * 100);
      f.second = fv;
    }

    fan_value = j[json::json_pointer(fmt::format("/params/0/{}/speed", fan_name))];
    if (!fan_value.is_null()) {
      fv = static_cast<int>(fan_value.template get<double>() * 100);
      f.second = fv;
    }
    values.push_back(fmt::format("{}%", fv));
  }

  fan0.update_label(fmt::format("{}", fmt::join(values, ", ")).c_str());

  // progress percentage and times (Mainsail's calculation)
  if (!j["/params/0/print_stats"_json_pointer].is_null()
      || !j["/params/0/virtual_sdcard"_json_pointer].is_null()
      || !j["/params/0/display_status"_json_pointer].is_null()) {
    update_progress_display();
    update_time_progress();
  }

  v = j["/params/0/motion_report/live_extruder_velocity"_json_pointer];
  if (!v.is_null()) {
    double flow = pi() / 4 * std::pow(filament_diameter, 2) * v.template get<double>();
    flow_rate.update_label(fmt::format("{:.1f} mm3/s", flow > 0.0 ? flow : 0.0).c_str());
  }

  v = j["/params/0/pause_resume/is_paused"_json_pointer];
  if (!v.is_null()) {
    bool is_paused = v.template get<bool>();
    if (is_paused) {
      resume_btn.enable();
      lv_obj_clear_flag(resume_btn.get_container(), LV_OBJ_FLAG_HIDDEN);

      pause_btn.disable();
      lv_obj_add_flag(pause_btn.get_container(), LV_OBJ_FLAG_HIDDEN);

    } else {
      pause_btn.enable();
      lv_obj_clear_flag(pause_btn.get_container(), LV_OBJ_FLAG_HIDDEN);

      resume_btn.disable();
      lv_obj_add_flag(resume_btn.get_container(), LV_OBJ_FLAG_HIDDEN);
    }
  }

  // layers
  v = j["/params/0/print_stats/info"_json_pointer];
  update_layers(v);
}

void PrintStatusPanel::handle_callback(lv_event_t *event) {
  lv_obj_t *btn = lv_event_get_current_target(event);
  if (btn == back_btn.get_container()) {
    lv_obj_move_background(status_cont);

  } else if (btn == complete_done_btn) {
    lv_obj_move_background(complete_cont);

  } else if (btn == z_offset.get_container()) {
    zoffset_panel.foreground();

  } else if (btn == emergency_btn.get_container()) {
    ws.send_jsonrpc("printer.emergency_stop");
  } else if (btn == pause_btn.get_container()) {
    if (start_running(start_in.step)) {
      // G112: Klipper runs START_PRINT to the end before it can pause.
      SettingsUi::confirm("Pause",
        "The printer is still preparing (heating, bed mesh, purge line). A pause takes "
        "effect after that - Cancel waits too. Only the emergency stop acts at once.",
        "Pause after start", SettingsUi::Tone::Warning, [this]() {
          ws.send_jsonrpc("printer.print.pause");
          pause_btn.disable();
          pause_requested = true;
          update_start();
        });
      return;
    }
    ws.send_jsonrpc("printer.print.pause");
    pause_btn.disable();

  } else if (btn == resume_btn.get_container()) {
    ws.send_jsonrpc("printer.print.resume");
    resume_btn.disable();
  } else if (btn == cancel_btn.get_container()) {
    auto s = State::get_instance();
    auto etarget_j = s->get_data("/printer_state/extruder/target"_json_pointer);
    auto etemp_j   = s->get_data("/printer_state/extruder/temperature"_json_pointer);
    auto btarget_j = s->get_data("/printer_state/heater_bed/target"_json_pointer);
    auto btemp_j   = s->get_data("/printer_state/heater_bed/temperature"_json_pointer);
    auto pdur_j    = s->get_data("/printer_state/print_stats/print_duration"_json_pointer);
    double etarget = etarget_j.is_number() ? etarget_j.template get<double>() : 0.0;
    double etemp   = etemp_j.is_number()   ? etemp_j.template get<double>()   : 0.0;
    double btarget = btarget_j.is_number() ? btarget_j.template get<double>() : 0.0;
    double btemp   = btemp_j.is_number()   ? btemp_j.template get<double>()   : 0.0;
    double pdur    = pdur_j.is_number()    ? pdur_j.template get<double>()    : 1.0;
    bool e_heating  = etarget > 0 && etemp < etarget - 2.0;
    bool b_heating  = btarget > 0 && btemp < btarget - 2.0;
    bool in_startup = pdur < 0.1;

    std::string delay;
    if (e_heating && b_heating)
      delay = fmt::format("\n\nExtruder {:.0f}/{:.0f}°C + Bed {:.0f}/{:.0f}°C\n"
                          "Cancel is queued and will run when heating finishes.", etemp, etarget, btemp, btarget);
    else if (e_heating)
      delay = fmt::format("\n\nExtruder heating: {:.0f}/{:.0f}°C\n"
                          "Cancel is queued and will run when heating finishes.", etemp, etarget);
    else if (b_heating)
      delay = fmt::format("\n\nBed heating: {:.0f}/{:.0f}°C\n"
                          "Cancel is queued and will run when heating finishes.", btemp, btarget);
    else if (in_startup)
      delay = "\n\nStartup in progress — cancel is queued\n"
              "and will run when startup finishes.";

    static const char *cancel_dlg_btns[] = {"Cancel Print", "Back", ""};
    std::string cancel_msg = fmt::format("Cancel the print?{}", delay);
    lv_obj_t *mbox = lv_msgbox_create(NULL, NULL, cancel_msg.c_str(), cancel_dlg_btns, false);
    lv_obj_add_event_cb(mbox, [](lv_event_t *ev) {
      lv_obj_t *obj = lv_obj_get_parent(lv_event_get_target(ev));
      auto *self = static_cast<PrintStatusPanel *>(lv_event_get_user_data(ev));
      if (lv_msgbox_get_active_btn(obj) == 0) {
        spdlog::debug("cancel print confirmed");
        self->ws.send_jsonrpc("printer.print.cancel");
      }
      lv_msgbox_close(obj);
    }, LV_EVENT_VALUE_CHANGED, this);
    KUtils::style_lock_mbox(mbox, 90);
    lv_obj_align(((lv_msgbox_t *)mbox)->text, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_t *btnm = lv_msgbox_get_btns(mbox);
    lv_obj_add_event_cb(btnm, [](lv_event_t *e) {
      lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
      if (dsc->part == LV_PART_ITEMS && dsc->id == 0) {
        dsc->rect_dsc->bg_color = lv_color_hex(0xC62828);
        dsc->rect_dsc->bg_opa   = LV_OPA_COVER;
        dsc->label_dsc->color   = lv_color_white();
      }
    }, LV_EVENT_DRAW_PART_BEGIN, NULL);
  } else if (btn == finetune_btn.get_container()) {
    finetune_panel.foreground();
  } else if (btn == objects_btn.get_container()) {
    exclude_object_panel.foreground();
  } else if (btn == mini_print_status.get_container()
             || btn == mini_print_status.get_chip()) {
    foreground();
  }
}

void PrintStatusPanel::update_time_progress() {
  // Mainsail's "Estimate" (time left) and print time, same values, same format.
  double left = print_time_left(est_in, est_settings);
  if (left > 0) {
    time_left.update_label(format_duration(left).c_str());
    mini_print_status.update_eta(static_cast<int64_t>(left));
  } else {
    time_left.update_label("--");
  }
  elapsed.update_label(format_duration(est_in.print_duration).c_str());
}

void PrintStatusPanel::update_progress_display() {
  // Mainsail: Math.floor(getPrintPercent * 100)
  int pct = static_cast<int>(std::floor(print_percent(est_in, est_settings) * 100));
  lv_bar_set_value(progress_bar, pct, LV_ANIM_ON);
  lv_label_set_text(progress_label, fmt::format("{}%", pct).c_str());
  mini_print_status.update_progress(pct);
}

void PrintStatusPanel::absorb_estimate_fields(const json &st) {
  if (!st.is_object()) {
    return;
  }
  auto num = [](const json &obj, const char *key, double &out) {
    auto v = obj.find(key);
    if (v != obj.end() && v->is_number()) out = v->template get<double>();
  };
  auto ps = st.find("print_stats");
  if (ps != st.end() && ps->is_object()) {
    num(*ps, "print_duration", est_in.print_duration);
    num(*ps, "filament_used", est_in.filament_used);
    auto fn = ps->find("filename");
    if (fn != ps->end() && fn->is_string()) est_in.print_filename = fn->template get<std::string>();
  }
  auto sd = st.find("virtual_sdcard");
  if (sd != st.end() && sd->is_object()) {
    num(*sd, "file_position", est_in.file_position);
    num(*sd, "progress", est_in.sd_progress);
  }
  auto ds = st.find("display_status");
  if (ds != st.end() && ds->is_object()) {
    num(*ds, "progress", est_in.slicer_progress);
  }
}

void PrintStatusPanel::fetch_estimate_settings() {
  // Mainsail keeps its gui.general settings in Moonraker's database; a printer
  // where they were never changed has none (Mainsail's defaults apply).
  json q = {{"namespace", "mainsail"}, {"key", "general"}};
  ws.send_jsonrpc("server.database.get_item", q, [this](json &d) {
    auto v = d["/result/value"_json_pointer];
    std::lock_guard<std::mutex> lock(lv_lock);
    est_settings = print_estimate_settings(v);
    update_progress_display();
    update_time_progress();
  });
}

void PrintStatusPanel::update_layers(json &info) {
  layers.update_label(fmt::format("{} / {}", current_layer(info), max_layer(info)).c_str());
}

int PrintStatusPanel::max_layer(json &info) {
  if (!info.is_null()) {
    auto v = info["/total_layer"_json_pointer];
    if (!v.is_null()) {
      return v.template get<int>();
    }
  }

  if (!current_file.is_null()) {
    auto v = current_file["/layer_count"_json_pointer];
    if (!v.is_null()) {
      return v.template get<int>();
    } else {
      auto first_layer_height = current_file["/first_layer_height"_json_pointer];
      auto layer_height = current_file["/layer_height"_json_pointer];
      auto object_height = current_file["/object_height"_json_pointer];

      if (!first_layer_height.is_null() && !layer_height.is_null() && !object_height.is_null()) {
        auto layer = static_cast<int>(std::ceil((object_height.template get<double>() - first_layer_height.template get<double>()) / layer_height.template get<double>() + 1));
        return layer > 0 ? layer : 0;
      }
    }
  }
  return 0;
}

int PrintStatusPanel::current_layer(json &info) {
  if (!info.is_null()) {
    auto v = info["/current_layer"_json_pointer];
    if (!v.is_null()) {
      return v.template get<int>();
    }
  }

  if (!current_file.is_null()) {
    State *s = State::get_instance();
    auto pd = s->get_data("/printer_state/print_stats/print_duration"_json_pointer);
    auto zpos = s->get_data("/printer_state/gcode_move/gcode_position/2"_json_pointer);

    auto first_layer_height = current_file["/first_layer_height"_json_pointer];
    auto layer_height = current_file["/layer_height"_json_pointer];

    if (!pd.is_null()
        && pd.template get<int>() > 0
        && !zpos.is_null()
        && !first_layer_height.is_null()
        && !layer_height.is_null()) {
      auto layer = static_cast<int>(std::ceil((zpos.template get<double>() - first_layer_height.template get<double>()) / layer_height.template get<double>() + 1));
      auto total = max_layer(info);
      if (layer > total) {
        return total;
      }

      if (layer > 0) {
        return layer;
      }
    }
  }

  return 0;
}

FineTunePanel &PrintStatusPanel::get_finetune_panel() {
  return finetune_panel;
}

ZOffsetPanel &PrintStatusPanel::get_zoffset_panel() {
  return zoffset_panel;
}

// G112. LVGL lock held.
void PrintStatusPanel::update_start() {
  auto *s = State::get_instance();
  auto num = [s](const char *ptr) {
    auto v = s->get_data(json::json_pointer(ptr));
    return v.is_number() ? v.template get<double>() : 0.0;
  };
  start_in.bed = num("/printer_state/heater_bed/temperature");
  start_in.bed_target = num("/printer_state/heater_bed/target");
  start_in.nozzle = num("/printer_state/extruder/temperature");
  start_in.nozzle_target = num("/printer_state/extruder/target");
  start_in.soak_elapsed = (int)std::chrono::duration_cast<std::chrono::seconds>(
    std::chrono::steady_clock::now() - soak_seen).count();
  auto rows = start_steps(start_in);
  if (rows.empty()) {
    pause_requested = false;
    lv_obj_add_flag(start_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(thumbnail, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  std::string text = "Starting the print\n";
  for (const auto &r : rows) {
    const char *mark = r.state == StartStepState::Done ? LV_SYMBOL_OK "  "
                     : r.state == StartStepState::Current ? LV_SYMBOL_RIGHT "  " : "      ";
    text += std::string(mark) + r.text + "\n";
  }
  if (pause_requested) text += LV_SYMBOL_PAUSE "  Pause requested - after the start";
  else text.pop_back();
  lv_label_set_text(start_label, text.c_str());
  lv_obj_clear_flag(start_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(thumbnail, LV_OBJ_FLAG_HIDDEN);
}

#ifdef SIMULATOR
void PrintStatusPanel::sim_setup_mock_data() {
  // Mid-print snapshot: 20% in, 10 minutes elapsed.
  // To preview the auto-dismiss (#94) on this build, change "printing" to
  // "complete" and rebuild - the panel should NOT foreground.
  json mock = {{"params", {{
    {"print_stats", {
      {"filename", "BunnyHighResLongFilenameTest_v3.gcode"},
      {"state", "printing"},
      {"print_duration", 600.0},
      {"filament_used", 1234.5},
      {"info", {{"current_layer", 12}, {"total_layer", 60}}}
    }},
    {"virtual_sdcard", {{"progress", 0.20}}},
    {"extruder", {{"temperature", 210}, {"target", 220}}},
    {"heater_bed", {{"temperature", 62}, {"target", 65}}},
    // Labelled objects so the conditional "Objects" button shows in the sim.
    {"exclude_object", {
      {"objects", {
        {{"name", "benchy_1"}, {"center", {60, 60}},
         {"polygon", {{40, 40}, {80, 40}, {80, 80}, {40, 80}}}},
        {{"name", "benchy_2"}, {"center", {165, 85}},
         {"polygon", {{145, 60}, {185, 60}, {185, 110}, {145, 110}}}},
        {{"name", "LongBracketMountFilename_v3_id2"}, {"center", {110, 165}},
         {"polygon", {{90, 145}, {130, 145}, {130, 185}, {90, 185}}}}
      }},
      {"excluded_objects", {"benchy_2"}},
      {"current_object", "benchy_1"}
    }}
  }}}};
  State::get_instance()->consume(mock);
  // consume() will reset() + populate() + foreground() because filename is
  // present, then drive update_time_progress() with progress 0.20.
  consume(mock);

  // reset() (inside consume) cleared the metadata; inject a filament total
  // the way handle_metadata would and recompute (Mainsail's calculation):
  //   file     = 600/0.20            - 600 = 2400 s
  //   filament = 600/(1234.5/5000.0) - 600 = 1830 s (rounded)
  //   mean = 2115 s -> "0:35:15" left.
  current_file = {{"filament_total", 5000.0}};
  est_in.filament_total = 5000.0;
  est_in.have_filament_total = true;
  {
    std::lock_guard<std::mutex> lock(lv_lock);
    update_time_progress();
    mini_print_status.sim_thumb();  // placeholder preview for the home overlay
  }

  // preview the completion summary (no real print finishes in the sim)
  lv_timer_t *t = lv_timer_create([](lv_timer_t *tm) {
    ((PrintStatusPanel *)tm->user_data)->show_completion("complete");
  }, 11000, this);
  lv_timer_set_repeat_count(t, 1);
}
#endif
