/* nebulabeep (source keeps its upstream name guppybeep.c) - compatibility
 * client for the Ender-3 V3 KE buzzer.
 *
 * G108 (NebulaOS Run 19): the buzzer (PC03 = PWM channel 3) is a kernel PWM
 * channel owned by Klipper's nebulaos_beeper extra. This program no longer
 * touches hardware (formerly: PWM + GPIO registers through /dev/mem); it only
 * forwards the request to Klipper over the Klippy API socket, so old scripts
 * and macros that run /opt/nebulascreen/nebulabeep keep working. Nothing in
 * NebulaOS itself calls it any more; it still ships because the NebulaScreen
 * release format lists it.
 *
 *   nebulabeep tone <freq_hz> <ms>          -> NEBULAOS_BEEP
 *   nebulabeep m300 S<freq> P<ms>           -> NEBULAOS_BEEP (defaults S1000 P100)
 *   nebulabeep rtttl "<rtttl-string>"       -> NEBULAOS_PLAY_TUNE RTTTL=
 *   nebulabeep song <name> [songs-file]     -> NEBULAOS_PLAY_TUNE SONG= (Klipper's songs_file)
 *   nebulabeep click [ignored...]           -> API nebulaos_beeper/click
 *
 * Environment: KLIPPY_API_SOCKET (default /opt/printer_data/comms/klippy.sock).
 * Exit status: 0 = Klipper accepted the request, 1 = error (printed).
 */
#include <ctype.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define DEFAULT_SOCKET "/opt/printer_data/comms/klippy.sock"
#define REPLY_TIMEOUT_MS 10000

static int append(char *dst, size_t cap, size_t *len, const char *s) {
  size_t n = strlen(s);
  if (*len + n + 1 > cap) return -1;
  memcpy(dst + *len, s, n + 1);
  *len += n;
  return 0;
}

/* JSON-escape src into dst (quotes, backslashes, control chars dropped). */
static int append_json_str(char *dst, size_t cap, size_t *len, const char *src) {
  char one[3];
  for (; *src; src++) {
    unsigned char c = (unsigned char)*src;
    if (c < 0x20) continue;
    if (c == '"' || c == '\\') { one[0] = '\\'; one[1] = (char)c; one[2] = 0; }
    else { one[0] = (char)c; one[1] = 0; }
    if (append(dst, cap, len, one)) return -1;
  }
  return 0;
}

static int klippy_request(const char *method, const char *script) {
  const char *path = getenv("KLIPPY_API_SOCKET");
  if (!path || !*path) path = DEFAULT_SOCKET;
  char msg[4096];
  size_t len = 0;
  if (append(msg, sizeof msg, &len, "{\"id\":1,\"method\":\"") ||
      append(msg, sizeof msg, &len, method) ||
      append(msg, sizeof msg, &len, "\",\"params\":{")) goto too_long;
  if (script) {
    if (append(msg, sizeof msg, &len, "\"script\":\"") ||
        append_json_str(msg, sizeof msg, &len, script) ||
        append(msg, sizeof msg, &len, "\"")) goto too_long;
  }
  if (append(msg, sizeof msg, &len, "}}\x03")) goto too_long;

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof addr);
  addr.sun_family = AF_UNIX;
  if (strlen(path) >= sizeof addr.sun_path) { fprintf(stderr, "nebulabeep: socket path too long\n"); return 1; }
  strcpy(addr.sun_path, path);
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0 || connect(fd, (struct sockaddr *)&addr, sizeof addr) != 0) {
    fprintf(stderr, "nebulabeep: Klipper API %s: %s\n", path, strerror(errno));
    if (fd >= 0) close(fd);
    return 1;
  }
  size_t off = 0;
  while (off < len) {
    ssize_t n = send(fd, msg + off, len - off, MSG_NOSIGNAL);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) { fprintf(stderr, "nebulabeep: send: %s\n", strerror(errno)); close(fd); return 1; }
    off += (size_t)n;
  }
  char reply[4096];
  size_t rlen = 0;
  for (;;) {
    struct pollfd p = {fd, POLLIN, 0};
    if (poll(&p, 1, REPLY_TIMEOUT_MS) <= 0) { fprintf(stderr, "nebulabeep: no reply from Klipper\n"); close(fd); return 1; }
    ssize_t n = recv(fd, reply + rlen, sizeof reply - 1 - rlen, 0);
    if (n <= 0) { fprintf(stderr, "nebulabeep: Klipper closed the connection\n"); close(fd); return 1; }
    rlen += (size_t)n;
    reply[rlen] = 0;
    if (memchr(reply, '\x03', rlen) || rlen >= sizeof reply - 1) break;
  }
  close(fd);
  if (strstr(reply, "\"error\"")) {
    char *e = strchr(reply, '\x03');
    if (e) *e = 0;
    fprintf(stderr, "nebulabeep: %s\n", reply);
    return 1;
  }
  return 0;
too_long:
  fprintf(stderr, "nebulabeep: request too long\n");
  return 1;
}

static int beep(unsigned long f, unsigned long ms) {
  char script[128];
  snprintf(script, sizeof script, "NEBULAOS_BEEP FREQ=%lu DURATION=%lu", f, ms);
  return klippy_request("gcode/script", script);
}

int main(int argc, char **argv) {
  const char *mode = argc > 1 ? argv[1] : "";
  char script[2048];

  if (!strcmp(mode, "tone")) {
    unsigned long f = argc > 2 ? strtoul(argv[2], 0, 0) : 1000;
    unsigned long ms = argc > 3 ? strtoul(argv[3], 0, 0) : 200;
    return beep(f, ms);
  } else if (!strcmp(mode, "m300")) {
    unsigned long f = 1000, ms = 100;
    for (int i = 2; i < argc; i++) {
      char c = (char)toupper((unsigned char)argv[i][0]);
      const char *num = argv[i][1] ? argv[i] + 1 : (i + 1 < argc ? argv[++i] : "");
      if (c == 'S') f = strtoul(num, 0, 0);
      else if (c == 'P') ms = strtoul(num, 0, 0);
    }
    return beep(f, ms);
  } else if (!strcmp(mode, "rtttl") && argc > 2) {
    /* '#' starts a G-code comment: send sharps as 's'; drop quotes/spaces */
    char tune[1500];
    size_t n = 0;
    for (const char *p = argv[2]; *p && n < sizeof tune - 1; p++) {
      if (*p == '#') tune[n++] = 's';
      else if (*p != '"' && !isspace((unsigned char)*p)) tune[n++] = *p;
    }
    tune[n] = 0;
    snprintf(script, sizeof script, "NEBULAOS_PLAY_TUNE RTTTL=\"%s\"", tune);
    return klippy_request("gcode/script", script);
  } else if (!strcmp(mode, "song") && argc > 2) {
    char name[128];
    size_t n = 0;
    for (const char *p = argv[2]; *p && n < sizeof name - 1; p++)
      if (isalnum((unsigned char)*p) || *p == '_' || *p == '-') name[n++] = *p;
    name[n] = 0;
    if (argc > 3) fprintf(stderr, "nebulabeep: songs file argument ignored; Klipper reads its songs_file\n");
    snprintf(script, sizeof script, "NEBULAOS_PLAY_TUNE SONG=\"%s\"", name);
    return klippy_request("gcode/script", script);
  } else if (!strcmp(mode, "click")) {
    return klippy_request("nebulaos_beeper/click", NULL);
  }
  fprintf(stderr, "usage: %s tone <freq> <ms> | m300 S<f> P<ms> | rtttl <str> | song <name> | click\n", argv[0]);
  return 2;
}
