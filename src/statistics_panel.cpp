#include "statistics_panel.h"
#include "nebulaos_system.h"
#include "settings_ui.h"
#include "spdlog/spdlog.h"

LV_IMG_DECLARE(back);

static lv_obj_t *text_label(lv_obj_t *parent, const char *text) {
  lv_obj_t *l = lv_label_create(parent);
  lv_obj_set_width(l, LV_PCT(100));
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_line_space(l, 2, 0);
  lv_label_set_text(l, text);
  return l;
}

StatisticsPanel::StatisticsPanel(KWebSocketClient &c, std::mutex &lock)
  : ws(c)
  , lv_lock(lock)
  , cont(SettingsUi::page("Statistics", &body))
  , back_btn(cont, &back, "Back", &StatisticsPanel::_handle_callback, this)
{
  SettingsUi::on_shown(cont, [this]() { shown(); });
  lv_obj_t *left = SettingsUi::column(body, 58);
  lv_obj_t *right = SettingsUi::column(body, 39);

  // Print time is Klipper's: first extrusion to the end, without heating and
  // pauses. Before NebulaOS = the larger of Creality's counter and an
  // earlier Moonraker (both counted the same prints with OpenKE).
  summary_label = text_label(left, "Adding up...");
  recent_label = text_label(right, "");
  // One print per entry, at most two lines: mark, time, then the name from its
  // start, ending in "..." when it is longer (Run 23: cut names all read
  // "xyz-10mm-calibrati..."). Four entries keep the column clear of Back.
  recent_list = lv_obj_create(right);
  lv_obj_set_size(recent_list, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(recent_list, 0, 0);
  lv_obj_set_style_pad_row(recent_list, 4, 0);
  lv_obj_set_style_border_width(recent_list, 0, 0);
  lv_obj_set_style_bg_opa(recent_list, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(recent_list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(recent_list, LV_FLEX_FLOW_COLUMN);

  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_FLOATING);
  SettingsUi::place_back(back_btn.get_container());  // G94: one Back position
}

StatisticsPanel::~StatisticsPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void StatisticsPanel::foreground() {
  lv_obj_move_foreground(cont);
}

// Every time the page comes on top.
void StatisticsPanel::shown() {
  ws.send_jsonrpc(STATS_METHOD, json::object(), [this](json &reply) {
    StatsText t = stats_text(reply);
    if (!t.ok) {
      spdlog::info("statistics: {}", t.summary);
    }
    std::lock_guard<std::mutex> lock(lv_lock);
    show(t);
  });
}

// LVGL thread.
void StatisticsPanel::show(const StatsText &t) {
  lv_label_set_text(summary_label, t.summary.c_str());
  lv_label_set_text(recent_label, t.recent.c_str());
  lv_obj_clean(recent_list);
  const lv_coord_t two_lines = 2 * lv_font_get_line_height(&lv_font_montserrat_12) + 2;
  for (const auto &p : t.prints) {
    std::string line = p.time + "  " + p.name;
    lv_obj_t *l = text_label(recent_list, line.c_str());
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_height(l, LV_SIZE_CONTENT);
    lv_obj_set_style_max_height(l, two_lines, 0);
  }
}

#ifdef SIMULATOR
void StatisticsPanel::sim_fill(const char *reply) {
  show(stats_text(json::parse(reply)));
}
#endif

void StatisticsPanel::handle_callback(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  if (lv_event_get_current_target(e) == back_btn.get_container()) {
    lv_obj_move_background(cont);
  }
}
