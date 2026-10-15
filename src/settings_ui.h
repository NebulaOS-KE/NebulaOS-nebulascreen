#ifndef __SETTINGS_UI_H__
#define __SETTINGS_UI_H__

#include "lvgl/lvgl.h"

#include <functional>

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

// Closes every open confirmation as if Cancel was tapped.
void dismiss_confirms();

// Modal message with a single OK button (same look as confirm()).
void notice(const char *title, const char *text);

// Modal "please wait" box without buttons, above everything (also above the
// "waiting for printer" screen while Klipper restarts). Returns its text
// label for updates; close it with close_busy(label).
lv_obj_t *busy(const char *title, const char *text);
void close_busy(lv_obj_t *label);

}  // namespace SettingsUi

#endif // __SETTINGS_UI_H__
