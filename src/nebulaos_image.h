#ifndef __NEBULAOS_IMAGE_H__
#define __NEBULAOS_IMAGE_H__

#include "hv/json.hpp"

#include <string>
#include <vector>

using json = nlohmann::json;

// Pure, LVGL-free logic for Settings > Updates > System image (on-printer
// image updates, decided 2026-10-14). machine.nebulaos.image.* is
// nebulaos_osconfig/image.py: a KE .img (NebulaOS or Creality, the same
// package format Creality's own updater takes) from a USB stick or
// config/uploads/system-image is written into the boot slot (A/B) that is NOT running,
// then the printer restarts into it. After a NebulaOS image the new slot gets
// one trial boot: if Klipper and the screen do not come up, NebulaOS goes back
// to the previous slot once. Only offered on the touchscreen.
//
// This file may be distributed under the terms of the GNU GPLv3 license.

extern const char *const IMAGE_LIST_METHOD;     // machine.nebulaos.image.list
extern const char *const IMAGE_CHECK_METHOD;    // machine.nebulaos.image.check {"file"}
extern const char *const IMAGE_INSTALL_METHOD;  // machine.nebulaos.image.install {"file","sha256"}
extern const char *const IMAGE_STATUS_METHOD;   // machine.nebulaos.image.status
extern const char *const IMAGE_ACK_METHOD;      // machine.nebulaos.image.ack

struct ImageSlot {
  std::string system;     // "nebulaos" | "creality" | "unknown"
  std::string kernel;     // uImage name, "" when unreadable
  std::string build;      // NebulaOS build commit (7 hex), "" when unknown
  std::string date;       // its build date (YYYY-MM-DD)
  std::string version;    // another system's own version, "" when unknown
};

struct ImageSlots {
  bool known = false;     // running slot known
  std::string running;    // "A" | "B"
  std::string other;      // the slot an install writes
  ImageSlot a, b;
  const ImageSlot &of(const std::string &slot) const { return slot == "A" ? a : b; }
};

// From the "slots" object of image.list / image.status / image.check.
ImageSlots image_slots(const json &slots);
// "NebulaOS" / "other system" / "unknown system". G57: the screen never
// names the other vendor - it talks about NebulaOS and boot slots A/B.
std::string image_system_text(const std::string &system);
// G46/G48: "NebulaOS build 14a9280 (2026-10-15)", "other system, version
// 1.1.0.12", "NebulaOS" - as much as is known.
std::string image_identity_text(const std::string &system, const std::string &build,
                                const std::string &date, const std::string &version);
// "Slot A: NebulaOS build 14a9280 (2026-10-15), running" - one line per slot.
std::string image_slot_line(const ImageSlots &s, const std::string &slot);

struct ImageFile {
  std::string id;         // "upload/x.img" | "usb/sda1/x.img"
  std::string name;
  std::string source;     // "upload" | "usb"
  long size = 0;
  std::string build;      // from the .img's NebulaOS build record
  std::string date;
  std::string version;    // ota_version of the package
  std::string detail;     // "USB stick, 112 MB - NebulaOS build 14a9280 (2026-10-15)"
};

struct ImageFiles {
  bool ok = false;
  std::string error;      // when !ok
  std::vector<ImageFile> files;
  bool usb_present = false;
  ImageSlots slots;
};

ImageFiles image_files(const json &list_reply);

// After image.check: whether it may be installed, and the confirmation.
struct ImageCheck {
  bool ok = false;
  std::string file;
  std::string sha256;
  std::string system;     // what the package holds
  std::string title;      // confirmation title, or "Cannot install"
  std::string text;       // confirmation text, or why not
};

ImageCheck image_check(const json &check_reply);
json image_check_params(const std::string &file_id);
json image_install_params(const ImageCheck &c);
// A message for a failed image.install call; "" when the call worked.
std::string image_install_error(const json &reply);

// The running job from image.status.
struct ImageJob {
  bool active = false;      // running or restarting
  bool restarting = false;
  bool failed = false;
  int percent = 0;
  std::string text;         // for the busy box / the failure notice
};

ImageJob image_job(const json &status_reply);

// The result of the last install, once, from image.status "last" (written at
// the trial boot): shown at start-up, then acknowledged with IMAGE_ACK_METHOD.
struct ImageLast {
  bool show = false;
  std::string title;
  std::string text;
};

ImageLast image_last(const json &status_reply);
// A trial boot is still deciding (image.status "trial" set): ask again later.
bool image_trial_pending(const json &status_reply);

// Settings > Reset > Swap boot slot: the confirmation, from what the other slot
// holds (image.status "slots"); the generic text when unknown.
std::string swap_confirm_text(const json &status_reply);

#endif // __NEBULAOS_IMAGE_H__
