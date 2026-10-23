#ifndef __SETTINGS_UI_H__
#define __SETTINGS_UI_H__

#include "lvgl/lvgl.h"
#include "nebulaos_refusal.h"

#include <functional>
#include <string>

// Shared building blocks for the Settings pages (S1, 2026-10-11), so every page
// has the same title, row spacing, buttons and confirmation dialog:
//
//   +--------------------------------------------------------+
//   | Title                                                  |
//   | left column (rows)           | right column (buttons)  |
//   |                              |               [<- Back] |
//   +--------------------------------------------------------+
//
// This file may be distributed under the terms of the GNU GPLv3 license.

namespace SettingsUi {

// Full-screen page with a title; returns the page. `body` gets the area below
// the title, laid out as a row (put columns into it).
lv_obj_t *page(const char *title, lv_obj_t **body);

// A transparent flex column of the given width inside `parent`.
lv_obj_t *column(lv_obj_t *parent, lv_coord_t width_pct);

// "Label ............ [widget]": a full-width row; the widget is right-aligned
// by the caller with lv_obj_align(w, LV_ALIGN_RIGHT_MID, 0, 0).
lv_obj_t *row(lv_obj_t *parent, const char *label);

// "Key   value" info line; returns the value label.
lv_obj_t *info_line(lv_obj_t *parent, const char *key);

// Calls `shown` every time `page` becomes the top page of the screen: when it
// is opened, and again when a page opened over it is closed - so a page always
// shows current state, whichever way it was reached. Runs on the LVGL thread
// with the LVGL lock held (from an lv_timer), at most every 200 ms.
void on_shown(lv_obj_t *page, std::function<void()> shown);

// A thin divider line.
void divider(lv_obj_t *parent);

// A small grey help text.
lv_obj_t *hint(lv_obj_t *parent, const char *text);

// A full-width button with a small icon (one of the material images, shown at
// half size) and a label.
lv_obj_t *icon_button(lv_obj_t *parent, const void *img, const char *text,
                      lv_event_cb_t cb, void *user);

enum class Tone { Normal, Warning, Danger };

// Colours a button (or an icon tile's container) for its tone.
void set_tone(lv_obj_t *obj, Tone tone);

// Modal confirmation: title, text, Cancel and a confirm button in `tone`.
// `on_confirm` runs on the LVGL thread when confirmed.
void confirm(const char *title, const char *text, const char *confirm_label,
             Tone tone, const std::function<void()> &on_confirm);

// G66 (2026-10-16): like confirm(), with one checkbox (`option`, unticked)
// between the text and the buttons; the text comes from `text_for(ticked)` and
// follows the checkbox. `option` NULL: no checkbox. `on_confirm(ticked)` runs
// on the LVGL thread when confirmed.
void confirm_option(const char *title, const std::function<std::string(bool)> &text_for,
                    const char *option, const char *confirm_label, Tone tone,
                    const std::function<void(bool)> &on_confirm);

// Closes every open confirmation as if Cancel was tapped.
void dismiss_confirms();

// Modal message with a single OK button (same look as confirm()).
void notice(const char *title, const char *text);

// G91 v3.4: THE refusal popup ("Not now" + the reason text, OK). Every
// update/maintenance action (image install, MCU flash, slot switch, clean
// reset, boot logo, cleanup, updates) uses it.
void refusal(const Refusal &r);
// Shows the popup when `reply` is a NebulaOS refusal; true if it did.
bool show_if_refused(const json &reply);
// The client-side pre-check (a hint only; NebulaOS decides): popup and true
// while a print runs or is paused.
bool refuse_if_printing();

// Modal "please wait" box without buttons, above everything (also above the
// "waiting for printer" screen while Klipper restarts). Returns its text
// label for updates; close it with close_busy(label).
lv_obj_t *busy(const char *title, const char *text);
void close_busy(lv_obj_t *label);

// ---- G94 (Run 18): one look for every page ----------------------------------
// The page title: top-left, montserrat_16, at the same place as on the Settings
// pages (8 px from the left, 6 px from the top). Floating, so it works for flex,
// grid and absolute pages; the page's top padding becomes kTitleSpace so its
// content starts below the title (reserve=false: the page already keeps the
// space free).
constexpr lv_coord_t kTitleSpace = 28;
lv_obj_t *page_title(lv_obj_t *page, const char *text, bool reserve = true);
// The same for a label the page already owns (its parent is the page).
void place_title(lv_obj_t *label, bool reserve = true);
// The Back button: the icon tile (ButtonContainer with the "back" image) in the
// bottom-right corner, 6 px from both edges, whatever the page's padding.
void place_back(lv_obj_t *back_tile);
// For pages that do not own a ButtonContainer member: creates the Back tile on
// `page` (kept for the program's lifetime, like the pages), places it and
// returns its container - the object `cb` sees as the event's current target.
lv_obj_t *back_tile(lv_obj_t *page, lv_event_cb_t cb, void *user);
// The text size of every text button (Settings, guided pages, Z offset, ...);
// icon tiles keep their 10 px labels.
const lv_font_t *button_font();
// A text button's label in button_font() (the first label child).
void style_text_button(lv_obj_t *btn);

}  // namespace SettingsUi

#endif // __SETTINGS_UI_H__
