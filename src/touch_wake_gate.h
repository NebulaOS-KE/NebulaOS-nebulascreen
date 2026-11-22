#ifndef __TOUCH_WAKE_GATE_H__
#define __TOUCH_WAKE_GATE_H__

// #27 (2026-11-22): a touch on the sleeping (dark) display only wakes it.
//
// Popups (lv_msgbox_create(NULL, ...)) live on LVGL's top layer, ABOVE the
// black screen_saver that covers the active screen while the display sleeps.
// So the touch that woke the display used to land on whatever popup sat
// there: after a freeze, the "Power loss. Resume?" prompt was pressed by the
// wake-up tap and the printer heated and resumed on its own.
//
// The gate sits between the touch driver and LVGL. A press that starts while
// the display sleeps is reported to LVGL as RELEASED for as long as that
// finger stays down (the display wakes meanwhile); the next press is normal.
// Pure logic, no LVGL, so it is tested offline (make test-touch-wake-gate).
class TouchWakeGate {
public:
  // pressed: what the driver reported. sleeping: is the display asleep now.
  // Returns the pressed state LVGL may see. Sets *wake when the caller must
  // wake the display (LVGL counts no activity for a swallowed press).
  bool filter(bool pressed, bool sleeping, bool *wake) {
    *wake = false;
    if (!pressed) {
      down_ = false;
      swallowing_ = false;
      return false;
    }
    if (!down_) {            // a new press decides for its whole duration
      down_ = true;
      swallowing_ = sleeping;
      *wake = sleeping;
    }
    return !swallowing_;
  }

private:
  bool down_ = false;
  bool swallowing_ = false;
};

#endif
