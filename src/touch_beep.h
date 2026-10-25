#ifndef __TOUCH_BEEP_H__
#define __TOUCH_BEEP_H__

#include "lvgl/lvgl.h"

// Audible UI click feedback, mirroring stock Creality KE behaviour: a short
// soft tick of the KE buzzer on each button tap. Opt-in; off by default.
// The buzzer is owned by Klipper (nebulaos_beeper, G108); the click is a
// request to it over the Klippy API socket (touch_beep_client.h).
namespace TouchBeep {
  // Enable/disable the click feedback (driven by the Settings toggle / config).
  void set_enabled(bool enabled);
  bool is_enabled();

  // Fire the buzzer once, non-blocking. No-op in the simulator.
  void beep();

  // LVGL input-device feedback hook: beeps on LV_EVENT_CLICKED when enabled.
  // Registered as indev_drv.feedback_cb so it fires for any clickable widget
  // without per-widget wiring, and never on scroll/drag (no CLICKED there).
  void feedback_cb(lv_indev_drv_t *drv, uint8_t event_code);
}

#endif // __TOUCH_BEEP_H__
