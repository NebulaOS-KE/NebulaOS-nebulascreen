#ifndef __TOUCH_BEEP_CLIENT_H__
#define __TOUCH_BEEP_CLIENT_H__

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

// G108: the touch click is played by Klipper's nebulaos_beeper extra, the
// only owner of the buzzer (kernel PWM channel). The request goes straight
// to the Klippy API socket (endpoint "nebulaos_beeper/click"): Klippy serves
// it from its reactor, not behind the G-code mutex, so a click is not delayed
// by a running M109/M190 or a busy print queue, and it does not go through
// Moonraker. request() never blocks the UI thread: a worker thread owns one
// persistent connection; a click that cannot be delivered (Klippy down) is
// dropped silently.
namespace TouchBeep {

class KlippyClickClient {
public:
  explicit KlippyClickClient(std::string socket_path);
  ~KlippyClickClient();
  KlippyClickClient(const KlippyClickClient &) = delete;
  KlippyClickClient &operator=(const KlippyClickClient &) = delete;

  // Queue one click (coalesces with a click not yet sent). Non-blocking.
  void request();

  // Klippy API request for one click, including the 0x03 terminator.
  static std::string click_message(unsigned id);

  // Delivered requests (for tests/diagnostics).
  unsigned sent() const;

private:
  void run();
  bool ensure_connected();
  void disconnect();
  bool send_click();

  std::string path_;
  int fd_ = -1;
  unsigned next_id_ = 1;
  unsigned sent_ = 0;
  bool pending_ = false;
  bool stop_ = false;
  mutable std::mutex mu_;
  std::condition_variable cv_;
  std::thread worker_;
};

} // namespace TouchBeep

#endif // __TOUCH_BEEP_CLIENT_H__
