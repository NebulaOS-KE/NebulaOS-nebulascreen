// Offline tests for the touch click client (G108): the click is a request to
// Klipper's nebulaos_beeper over the Klippy API socket, never a hardware write.
// Build/run: make test-touch-beep-client
//
// This file may be distributed under the terms of the GNU GPLv3 license.
#include "minitest.h"
#include "../src/touch_beep_client.h"

#include <cerrno>
#include <chrono>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>

using TouchBeep::KlippyClickClient;

static std::string sock_path(const char *name) {
  const char *tmp = getenv("TMPDIR");
  std::string p = std::string(tmp && *tmp ? tmp : "/tmp") + "/" + name +
                  std::to_string(getpid());
  unlink(p.c_str());
  return p;
}

// -2: AF_UNIX sockets are not permitted here (a sandboxed build host); the
// socket tests then report SKIP instead of failing.
static int listen_on(const std::string &path) {
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0 && (errno == EPERM || errno == EACCES)) {
    std::cout << "[SKIP] AF_UNIX not permitted on this host" << std::endl;
    return -2;
  }
  struct sockaddr_un a;
  memset(&a, 0, sizeof(a));
  a.sun_family = AF_UNIX;
  strncpy(a.sun_path, path.c_str(), sizeof(a.sun_path) - 1);
  if (bind(fd, (struct sockaddr *)&a, sizeof(a)) != 0 || listen(fd, 4) != 0) {
    return -1;
  }
  return fd;
}

// Read one 0x03-terminated message (or "" on timeout/EOF).
static std::string read_msg(int fd, int timeout_ms = 2000) {
  std::string out;
  char c;
  for (;;) {
    struct pollfd p = {fd, POLLIN, 0};
    if (poll(&p, 1, timeout_ms) <= 0) return "";
    if (recv(fd, &c, 1, 0) != 1) return "";
    if (c == '\x03') return out;
    out.push_back(c);
  }
}

static bool wait_sent(KlippyClickClient &c, unsigned n) {
  for (int i = 0; i < 200; i++) {
    if (c.sent() >= n) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return false;
}

TEST(touch_beep, message_format) {
  std::string m = KlippyClickClient::click_message(7);
  ASSERT_EQ(m, std::string("{\"id\":7,\"method\":\"nebulaos_beeper/click\",\"params\":{}}\x03"));
}

TEST(touch_beep, sends_click_and_reuses_connection) {
  std::string path = sock_path("tbc");
  int srv = listen_on(path);
  if (srv == -2) return;
  ASSERT_TRUE(srv >= 0);
  {
    KlippyClickClient c(path);
    c.request();
    int conn = accept(srv, nullptr, nullptr);
    ASSERT_TRUE(conn >= 0);
    std::string m = read_msg(conn);
    ASSERT_TRUE(m.find("\"method\":\"nebulaos_beeper/click\"") != std::string::npos);
    ASSERT_TRUE(m.find("\"id\":1") != std::string::npos);
    send(conn, "{\"id\":1,\"result\":{}}\x03", 21, 0);
    ASSERT_TRUE(wait_sent(c, 1));
    c.request();   // same connection, next id
    std::string m2 = read_msg(conn);
    ASSERT_TRUE(m2.find("\"id\":2") != std::string::npos);
    send(conn, "{\"id\":2,\"result\":{}}\x03", 21, 0);
    ASSERT_TRUE(wait_sent(c, 2));
    close(conn);
  }
  close(srv);
  unlink(path.c_str());
}

TEST(touch_beep, request_never_blocks_without_klippy) {
  std::string path = sock_path("tbc-missing");
  KlippyClickClient c(path);
  auto t0 = std::chrono::steady_clock::now();
  for (int i = 0; i < 100; i++) c.request();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - t0).count();
  ASSERT_TRUE(ms < 50);
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  ASSERT_EQ(c.sent(), 0u);
}

TEST(touch_beep, reconnects_after_klippy_restart) {
  std::string path = sock_path("tbc-restart");
  int srv = listen_on(path);
  if (srv == -2) return;
  ASSERT_TRUE(srv >= 0);
  KlippyClickClient c(path);
  c.request();
  int conn = accept(srv, nullptr, nullptr);
  read_msg(conn);
  send(conn, "{}\x03", 3, 0);
  ASSERT_TRUE(wait_sent(c, 1));
  close(conn);                      // Klippy restarts: old connection gone
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  c.request();
  int conn2 = accept(srv, nullptr, nullptr);
  ASSERT_TRUE(conn2 >= 0);
  std::string m = read_msg(conn2);
  ASSERT_TRUE(m.find("nebulaos_beeper/click") != std::string::npos);
  send(conn2, "{}\x03", 3, 0);
  ASSERT_TRUE(wait_sent(c, 2));
  close(conn2);
  close(srv);
  unlink(path.c_str());
}

int main() {
  return minitest::run_all("touch_beep_client") > 0 ? 1 : 0;
}
