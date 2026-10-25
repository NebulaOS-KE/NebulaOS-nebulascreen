#include "touch_beep_client.h"

#include <cerrno>
#include <cstring>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace TouchBeep {

namespace {
// Wait at most this long for Klippy's reply before the next click; the reply
// is only drained so the socket never fills, its content is not needed.
constexpr int REPLY_TIMEOUT_MS = 300;
}

KlippyClickClient::KlippyClickClient(std::string socket_path)
    : path_(std::move(socket_path)), worker_(&KlippyClickClient::run, this) {}

KlippyClickClient::~KlippyClickClient() {
  {
    std::lock_guard<std::mutex> lk(mu_);
    stop_ = true;
  }
  cv_.notify_all();
  worker_.join();
  disconnect();
}

std::string KlippyClickClient::click_message(unsigned id) {
  std::string msg = "{\"id\":" + std::to_string(id) +
                    ",\"method\":\"nebulaos_beeper/click\",\"params\":{}}";
  msg.push_back('\x03');
  return msg;
}

void KlippyClickClient::request() {
  {
    std::lock_guard<std::mutex> lk(mu_);
    pending_ = true;
  }
  cv_.notify_one();
}

unsigned KlippyClickClient::sent() const {
  std::lock_guard<std::mutex> lk(mu_);
  return sent_;
}

bool KlippyClickClient::ensure_connected() {
  if (fd_ >= 0) {
    return true;
  }
  struct sockaddr_un addr;
  if (path_.size() >= sizeof(addr.sun_path)) {
    return false;
  }
  int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd < 0) {
    return false;
  }
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  memcpy(addr.sun_path, path_.c_str(), path_.size());
  if (connect(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) != 0) {
    close(fd);
    return false;
  }
  fd_ = fd;
  return true;
}

void KlippyClickClient::disconnect() {
  if (fd_ >= 0) {
    close(fd_);
    fd_ = -1;
  }
}

bool KlippyClickClient::send_click() {
  std::string msg = click_message(next_id_++);
  size_t off = 0;
  while (off < msg.size()) {
    // MSG_NOSIGNAL: a Klippy restart must never SIGPIPE the screen.
    ssize_t n = send(fd_, msg.data() + off, msg.size() - off, MSG_NOSIGNAL);
    if (n < 0 && errno == EINTR) {
      continue;
    }
    if (n <= 0) {
      return false;
    }
    off += static_cast<size_t>(n);
  }
  // Drain the reply (ends with 0x03); EOF means Klippy went away.
  char buf[512];
  for (;;) {
    struct pollfd pfd = {fd_, POLLIN, 0};
    int r = poll(&pfd, 1, REPLY_TIMEOUT_MS);
    if (r <= 0) {
      return true; // no reply in time: keep the connection, carry on
    }
    ssize_t n = recv(fd_, buf, sizeof(buf), 0);
    if (n <= 0) {
      return n < 0 && errno == EINTR;
    }
    if (memchr(buf, '\x03', static_cast<size_t>(n)) != nullptr) {
      return true;
    }
  }
}

void KlippyClickClient::run() {
  std::unique_lock<std::mutex> lk(mu_);
  for (;;) {
    cv_.wait(lk, [this] { return pending_ || stop_; });
    if (stop_) {
      return;
    }
    pending_ = false;
    lk.unlock();
    bool ok = false;
    // One retry on a fresh connection: the old one may be from before a
    // Klippy restart.
    for (int attempt = 0; attempt < 2 && !ok; attempt++) {
      if (!ensure_connected()) {
        break;
      }
      ok = send_click();
      if (!ok) {
        disconnect();
      }
    }
    lk.lock();
    if (ok) {
      sent_++;
    }
  }
}

} // namespace TouchBeep
