#include "settings_ui.h"
#include "utils.h"

#include <map>
#include <memory>
#include <set>

namespace SettingsUi {

static std::map<lv_obj_t *, std::function<void()>> shown_pages;
static lv_obj_t *top_page = nullptr;

static void watch_top(lv_timer_t *) {
  lv_obj_t *scr = lv_scr_act();
  uint32_t n = lv_obj_get_child_cnt(scr);
  lv_obj_t *top = n > 0 ? lv_obj_get_child(scr, n - 1) : nullptr;
  if (top == top_page) return;
  top_page = top;
  auto it = shown_pages.find(top);
  if (it != shown_pages.end()) it->second();
}

void on_shown(lv_obj_t *page, std::function<void()> shown) {
  if (shown_pages.empty()) lv_timer_create(&watch_top, 200, nullptr);
  shown_pages[page] = std::move(shown);
}

lv_obj_t *page(const char *title, lv_obj_t **body) {
  lv_obj_t *cont = lv_obj_create(lv_scr_act());
  lv_obj_move_background(cont);
  lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_pad_all(cont, 6, 0);
  lv_obj_set_style_pad_row(cont, 2, 0);
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);

  lv_obj_t *t = lv_label_create(cont);
  lv_label_set_text(t, title);
  lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);
  lv_obj_set_style_pad_left(t, 2, 0);

  lv_obj_t *b = lv_obj_create(cont);
  lv_obj_set_width(b, LV_PCT(100));
  lv_obj_set_flex_grow(b, 1);
  lv_obj_set_style_pad_all(b, 0, 0);
  lv_obj_set_style_pad_column(b, 10, 0);
  lv_obj_set_style_border_width(b, 0, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
  *body = b;
  return cont;
}

lv_obj_t *column(lv_obj_t *parent, lv_coord_t width_pct) {
  lv_obj_t *col = lv_obj_create(parent);
  lv_obj_set_size(col, LV_PCT(width_pct), LV_PCT(100));
  lv_obj_set_style_pad_all(col, 2, 0);
  lv_obj_set_style_pad_row(col, 6, 0);
  lv_obj_set_style_border_width(col, 0, 0);
  lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  return col;
}

lv_obj_t *row(lv_obj_t *parent, const char *label) {
  lv_obj_t *r = lv_obj_create(parent);
  lv_obj_set_size(r, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_min_height(r, 32, 0);
  lv_obj_set_style_pad_all(r, 0, 0);
  lv_obj_set_style_border_width(r, 0, 0);
  lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *l = lv_label_create(r);
  lv_label_set_text(l, label);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
  lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
  return r;
}

lv_obj_t *info_line(lv_obj_t *parent, const char *key) {
  lv_obj_t *r = lv_obj_create(parent);
  lv_obj_set_size(r, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(r, 0, 0);
  lv_obj_set_style_pad_column(r, 6, 0);
  lv_obj_set_style_border_width(r, 0, 0);
  lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
  lv_obj_t *k = lv_label_create(r);
  lv_label_set_text(k, key);
  lv_obj_set_width(k, 72);
  lv_obj_set_style_text_font(k, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(k, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);
  lv_obj_t *v = lv_label_create(r);
  lv_label_set_text(v, "-");
  lv_obj_set_flex_grow(v, 1);
  lv_label_set_long_mode(v, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(v, &lv_font_montserrat_12, 0);
  return v;
}

void divider(lv_obj_t *parent) {
  lv_obj_t *d = lv_obj_create(parent);
  lv_obj_set_size(d, LV_PCT(100), 1);
  lv_obj_set_style_border_width(d, 0, 0);
  lv_obj_set_style_pad_all(d, 0, 0);
  lv_obj_set_style_bg_color(d, lv_palette_darken(LV_PALETTE_GREY, 2), 0);
  lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
}

lv_obj_t *hint(lv_obj_t *parent, const char *text) {
  lv_obj_t *h = lv_label_create(parent);
  lv_obj_set_width(h, LV_PCT(100));
  lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(h, &lv_font_montserrat_10, 0);
  lv_obj_set_style_text_color(h, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);
  lv_label_set_text(h, text);
  return h;
}

lv_obj_t *icon_button(lv_obj_t *parent, const void *img, const char *text,
                      lv_event_cb_t cb, void *user) {
  lv_obj_t *b = lv_btn_create(parent);
  lv_obj_set_size(b, LV_PCT(100), 40);
  lv_obj_set_style_pad_all(b, 2, 0);
  lv_obj_set_style_pad_column(b, 4, 0);
  lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(b, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  if (img != NULL) {
    lv_obj_t *i = lv_img_create(b);
    lv_img_set_src(i, img);
    // The material images are 46 px (64 px on large screens); scaled to 26 px
    // they fit a 40 px button. REAL size mode makes the object the scaled size.
    lv_coord_t w = ((const lv_img_dsc_t *)img)->header.w;
    lv_coord_t target = 26;
    lv_img_set_size_mode(i, LV_IMG_SIZE_MODE_REAL);
    lv_img_set_zoom(i, (uint16_t)(256 * target / (w > 0 ? w : target)));
    lv_img_set_antialias(i, true);
    lv_obj_set_size(i, target, target);   // exactly one (scaled) copy, no tiling
  }
  lv_obj_t *l = lv_label_create(b);
  lv_label_set_text(l, text);
  lv_obj_set_flex_grow(l, 1);
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
  return b;
}

static lv_color_t tone_color(Tone tone) {
  switch (tone) {
    case Tone::Danger: return lv_color_hex(0xC62828);
    case Tone::Warning: return lv_color_hex(0xE65100);
    default: return lv_palette_main(LV_PALETTE_BLUE);
  }
}

void set_tone(lv_obj_t *obj, Tone tone) {
  if (tone == Tone::Normal) return;
  lv_obj_set_style_bg_color(obj, tone_color(tone), 0);
  lv_obj_set_style_border_color(obj, tone_color(tone), 0);
}

struct ConfirmCtx {
  lv_obj_t *overlay;
  std::function<void()> on_confirm;
};

static std::set<ConfirmCtx *> open_confirms;

static void confirm_clicked(lv_event_t *e) {
  auto *ctx = (ConfirmCtx *)lv_event_get_user_data(e);
  lv_obj_t *btn = lv_event_get_current_target(e);
  bool go = lv_obj_get_user_data(btn) != NULL;
  std::function<void()> cb = ctx->on_confirm;
  open_confirms.erase(ctx);
  lv_obj_del_async(ctx->overlay);
  delete ctx;
  if (go && cb) {
    cb();
  }
}

void dismiss_confirms() {
  for (ConfirmCtx *ctx : open_confirms) {
    lv_obj_del(ctx->overlay);
    delete ctx;
  }
  open_confirms.clear();
}

// Overlay + centered box with a title and a wrapped text; returns the box.
static lv_obj_t *dialog_box(const char *title, const char *text, lv_obj_t **overlay_out,
                            lv_obj_t **text_out) {
  lv_obj_t *overlay = lv_obj_create(lv_layer_top());
  KUtils::style_dialog_overlay(overlay);
  lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);

  lv_obj_t *box = lv_obj_create(overlay);
  KUtils::style_dialog_box(box);
  lv_obj_set_size(box, LV_PCT(88), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_row(box, 8, 0);
  lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_center(box);

  lv_obj_t *t = lv_label_create(box);
  lv_label_set_text(t, title);
  lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);

  lv_obj_t *msg = lv_label_create(box);
  lv_obj_set_width(msg, LV_PCT(100));
  lv_label_set_long_mode(msg, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(msg, &lv_font_montserrat_12, 0);
  lv_label_set_text(msg, text);
  *overlay_out = overlay;
  if (text_out != NULL) *text_out = msg;
  return box;
}

// Cancel (when `cancel_label` is set) and a confirm button in `tone`.
static void choice(const char *title, const char *text, const char *cancel_label,
                   const char *confirm_label, Tone tone, const std::function<void()> &on_confirm) {
  lv_obj_t *overlay;
  lv_obj_t *box = dialog_box(title, text, &overlay, NULL);

  lv_obj_t *btns = lv_obj_create(box);
  lv_obj_set_size(btns, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(btns, 0, 0);
  lv_obj_set_style_border_width(btns, 0, 0);
  lv_obj_set_style_bg_opa(btns, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(btns, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(btns, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(btns, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  auto *ctx = new ConfirmCtx{overlay, on_confirm};
  open_confirms.insert(ctx);
  const char *labels[] = {cancel_label, confirm_label};
  for (int i = 0; i < 2; ++i) {
    if (labels[i] == NULL) continue;
    lv_obj_t *b = lv_btn_create(btns);
    lv_obj_set_size(b, LV_PCT(44), 40);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, labels[i]);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_center(l);
    if (i == 0) {
      lv_obj_set_style_bg_color(b, lv_palette_darken(LV_PALETTE_GREY, 3), 0);
    } else {
      lv_obj_set_user_data(b, (void *)1);
      lv_obj_set_style_bg_color(b, tone_color(tone), 0);
    }
    lv_obj_add_event_cb(b, confirm_clicked, LV_EVENT_CLICKED, ctx);
  }
}

void confirm(const char *title, const char *text, const char *confirm_label,
             Tone tone, const std::function<void()> &on_confirm) {
  choice(title, text, "Cancel", confirm_label, tone, on_confirm);
}

struct OptionCtx {
  lv_obj_t *check;
  lv_obj_t *text;
  std::function<std::string(bool)> text_for;
};

static void option_toggled(lv_event_t *e) {
  auto *o = (OptionCtx *)lv_event_get_user_data(e);
  std::string t = o->text_for(lv_obj_has_state(o->check, LV_STATE_CHECKED));
  lv_label_set_text(o->text, t.c_str());
}

static void option_deleted(lv_event_t *e) {
  delete (OptionCtx *)lv_event_get_user_data(e);
}

void confirm_option(const char *title, const std::function<std::string(bool)> &text_for,
                    const char *option, const char *confirm_label, Tone tone,
                    const std::function<void(bool)> &on_confirm) {
  lv_obj_t *check = NULL;
  // The checkbox is read when confirmed; it lives in the dialog, which
  // confirm_clicked deletes asynchronously - after the callback has run.
  auto checked = std::make_shared<lv_obj_t *>(nullptr);
  choice(title, text_for(false).c_str(), "Cancel", confirm_label, tone, [checked, on_confirm]() {
    bool ticked = *checked != NULL && lv_obj_has_state(*checked, LV_STATE_CHECKED);
    on_confirm(ticked);
  });
  if (option == NULL) return;
  // choice() built: overlay (last child of the top layer) -> box -> [title, text, buttons].
  lv_obj_t *overlay = lv_obj_get_child(lv_layer_top(), lv_obj_get_child_cnt(lv_layer_top()) - 1);
  lv_obj_t *box = lv_obj_get_child(overlay, 0);
  lv_obj_t *text = lv_obj_get_child(box, 1);
  check = lv_checkbox_create(box);
  lv_checkbox_set_text(check, option);
  lv_obj_set_style_text_font(check, &lv_font_montserrat_12, 0);
  lv_obj_move_to_index(check, 2);
  *checked = check;
  auto *o = new OptionCtx{check, text, text_for};
  lv_obj_add_event_cb(check, option_toggled, LV_EVENT_VALUE_CHANGED, o);
  lv_obj_add_event_cb(check, option_deleted, LV_EVENT_DELETE, o);
}

void notice(const char *title, const char *text) {
  choice(title, text, NULL, "OK", Tone::Normal, std::function<void()>());
}

void refusal(const Refusal &r) {
  notice(REFUSAL_TITLE, r.text.c_str());
}

bool show_if_refused(const json &reply) {
  Refusal r = refusal_of(reply);
  if (r.refused) {
    refusal(r);
  }
  return r.refused;
}

bool refuse_if_printing() {
  Refusal r = refusal_hint(KUtils::is_printing(), KUtils::is_paused());
  if (r.refused) {
    refusal(r);
  }
  return r.refused;
}

lv_obj_t *busy(const char *title, const char *text) {
  lv_obj_t *overlay;
  lv_obj_t *label;
  dialog_box(title, text, &overlay, &label);
  return label;
}

void close_busy(lv_obj_t *label) {
  if (label == NULL) return;
  // label -> box -> overlay
  lv_obj_del_async(lv_obj_get_parent(lv_obj_get_parent(label)));
}

}  // namespace SettingsUi
