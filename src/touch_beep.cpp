#include "touch_beep.h"
#include "nebulascreen_paths.h"
#include "spdlog/spdlog.h"

#include <atomic>

#ifndef SIMULATOR
#include "touch_beep_client.h"
#include <memory>
#include <mutex>
#endif

namespace {
  std::atomic<bool> s_enabled{false};

  // Suppress a follow-up beep that lands within this window of the last one.
  // The panel reports at ~50Hz, so back-to-back CLICKED events from a single
  // jittery tap shouldn't double-fire.
  constexpr uint32_t DEBOUNCE_MS = 120;
  uint32_t s_last_tick = 0;

#ifndef SIMULATOR
  // G108: Klipper's nebulaos_beeper owns the buzzer; the click (a soft,
  // phone-like 260 Hz / 4 ms / 2% duty tick, configured in that extra) is
  // requested over the Klippy API socket (S55klipper's APISOCK). The screen
  // never touches the PWM hardware itself (formerly nebulabeep + /dev/mem).
  std::once_flag s_client_once;
  std::unique_ptr<TouchBeep::KlippyClickClient> s_client;
#endif
}

namespace TouchBeep {

void set_enabled(bool enabled) {
  s_enabled.store(enabled);
}

bool is_enabled() {
  return s_enabled.load();
}

void beep() {
#ifndef SIMULATOR
  // Non-blocking: the client's worker thread does the socket I/O.
  std::call_once(s_client_once, [] {
    s_client.reset(new KlippyClickClient(nebulascreen::kKlippyApiSocket));
  });
  s_client->request();
#else
  spdlog::debug("TouchBeep::beep() (no-op in simulator)");
#endif
}

void feedback_cb(lv_indev_drv_t * /*drv*/, uint8_t event_code) {
  if (!s_enabled.load()) {
    return;
  }
  // CLICKED only fires when a clickable widget is pressed and released without
  // scrolling - i.e. a real button tap, matching when stock Creality beeped.
  if (event_code != LV_EVENT_CLICKED) {
    return;
  }
  uint32_t now = lv_tick_get();
  if (s_last_tick != 0 && (now - s_last_tick) < DEBOUNCE_MS) {
    return;
  }
  s_last_tick = now;
  beep();
}

}
