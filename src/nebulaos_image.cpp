#include "nebulaos_image.h"

const char *const IMAGE_LIST_METHOD = "machine.nebulaos.image.list";
const char *const IMAGE_CHECK_METHOD = "machine.nebulaos.image.check";
const char *const IMAGE_INSTALL_METHOD = "machine.nebulaos.image.install";
const char *const IMAGE_STATUS_METHOD = "machine.nebulaos.image.status";
const char *const IMAGE_ACK_METHOD = "machine.nebulaos.image.ack";
const char *const OTA_CHECK_METHOD = "machine.nebulaos.ota.check";
const char *const OTA_DOWNLOAD_METHOD = "machine.nebulaos.ota.download";
const char *const OTA_STATUS_METHOD = "machine.nebulaos.ota.status";

static const json &result_of(const json &reply) {
  static const json empty = json::object();
  if (reply.is_object() && reply.contains("result") && reply["result"].is_object()) {
    return reply["result"];
  }
  return empty;
}

static std::string str_at(const json &j, const json::json_pointer &p) {
  if (j.contains(p) && j[p].is_string()) {
    return j[p].template get<std::string>();
  }
  return "";
}

static bool bool_at(const json &j, const json::json_pointer &p, bool fallback) {
  if (j.contains(p) && j[p].is_boolean()) {
    return j[p].template get<bool>();
  }
  return fallback;
}

static long long_at(const json &j, const json::json_pointer &p) {
  if (j.contains(p) && j[p].is_number()) {
    return j[p].template get<long>();
  }
  return 0;
}

// "" when the reply has no error; else Moonraker's message ("CODE: text").
static std::string error_of(const json &reply) {
  if (!reply.is_object() || !reply.contains("error")) return "";
  const json &e = reply["error"];
  if (e.is_object() && e.contains("message") && e["message"].is_string()) {
    return e["message"].template get<std::string>();
  }
  return e.dump();
}

static bool starts_with(const std::string &s, const char *prefix) {
  return s.rfind(prefix, 0) == 0;
}

// The text after "CODE: ", or the whole message.
static std::string after_code(const std::string &msg) {
  auto pos = msg.find(": ");
  if (pos == std::string::npos || pos == 0) return msg;
  for (size_t i = 0; i < pos; ++i) {
    char c = msg[i];
    if (!((c >= 'A' && c <= 'Z') || c == '_' || (c >= '0' && c <= '9'))) return msg;
  }
  return msg.substr(pos + 2);
}

static std::string megabytes(long bytes) {
  return std::to_string((bytes + 500000) / 1000000) + " MB";
}

// -- slots ---------------------------------------------------------------------------

ImageSlots image_slots(const json &slots) {
  ImageSlots s;
  if (!slots.is_object()) return s;
  s.running = str_at(slots, "/running"_json_pointer);
  s.known = s.running == "A" || s.running == "B";
  if (s.known) s.other = s.running == "A" ? "B" : "A";
  for (auto *pair : {&s.a, &s.b}) {
    std::string k = pair == &s.a ? "/A" : "/B";
    pair->system = str_at(slots, json::json_pointer(k + "/system"));
    pair->kernel = str_at(slots, json::json_pointer(k + "/kernel"));
    pair->build = str_at(slots, json::json_pointer(k + "/build"));
    pair->date = str_at(slots, json::json_pointer(k + "/date"));
    pair->version = str_at(slots, json::json_pointer(k + "/version"));
  }
  return s;
}

std::string image_system_text(const std::string &system) {
  if (system == "nebulaos") return "NebulaOS";
  if (system == "creality") return "other system";
  return "unknown system";
}

std::string image_identity_text(const std::string &system, const std::string &build,
                                const std::string &date, const std::string &version) {
  if (!build.empty()) {
    return "NebulaOS build " + build + (date.empty() ? "" : " (" + date + ")");
  }
  std::string text = image_system_text(system);
  if (!version.empty()) text += ", version " + version;
  return text;
}

std::string image_slot_line(const ImageSlots &s, const std::string &slot) {
  const ImageSlot &sl = s.of(slot);
  std::string text = "Slot " + slot + ": " + (sl.kernel.empty() ? std::string("empty") :
    image_identity_text(sl.system, sl.build, sl.date, sl.version));
  if (s.known && s.running == slot) text += ", running";
  return text;
}

// -- files ---------------------------------------------------------------------------

ImageFiles image_files(const json &list_reply) {
  ImageFiles out;
  std::string err = error_of(list_reply);
  if (!err.empty()) {
    out.error = "Could not list the images: " + after_code(err);
    return out;
  }
  const json &r = result_of(list_reply);
  if (r.empty()) {
    out.error = "No answer from the printer.";
    return out;
  }
  out.ok = true;
  out.usb_present = r.contains("usb") && r["usb"].is_array() && !r["usb"].empty();
  out.slots = image_slots(r.contains("slots") ? r["slots"] : json());
  if (r.contains("files") && r["files"].is_array()) {
    for (const auto &f : r["files"]) {
      ImageFile i;
      i.id = str_at(f, "/id"_json_pointer);
      i.name = str_at(f, "/name"_json_pointer);
      i.source = str_at(f, "/source"_json_pointer);
      i.size = long_at(f, "/size"_json_pointer);
      i.build = str_at(f, "/build"_json_pointer);
      i.date = str_at(f, "/date"_json_pointer);
      i.version = str_at(f, "/version"_json_pointer);
      if (i.id.empty()) continue;
      i.detail = (i.source == "usb" ? "USB stick, " : i.source == "ota" ? "Downloaded, "
                                                                        : "Uploaded, ") +
                 megabytes(i.size);
      if (!i.build.empty()) {
        i.detail += " - " + image_identity_text("nebulaos", i.build, i.date, "");
      } else if (!i.version.empty()) {
        i.detail += " - version " + i.version;
      }
      out.files.push_back(i);
    }
  }
  return out;
}

// -- check and install ---------------------------------------------------------------

json image_check_params(const std::string &file_id) {
  return json{{"file", file_id}};
}

json image_install_params(const ImageCheck &c) {
  return json{{"file", c.file}, {"sha256", c.sha256}};
}

static std::string not_installable(const std::string &msg) {
  if (starts_with(msg, "PRINTING") || starts_with(msg, "REFUSED")) {
    return "Not possible while a print is active. Nothing was changed.";
  }
  if (starts_with(msg, "BUSY")) {
    return "An image install is already running.";
  }
  if (starts_with(msg, "FILE_GONE") || starts_with(msg, "FILE_CHANGED")) {
    return "The file changed or is gone (USB stick removed?). Nothing was changed.";
  }
  if (starts_with(msg, "INVALID_IMAGE") || starts_with(msg, "INVALID_FILE")) {
    return "This file cannot be installed: " + after_code(msg) + ". Nothing was changed.";
  }
  if (starts_with(msg, "SLOT_UNKNOWN") || starts_with(msg, "PARTITION_MISMATCH") ||
      starts_with(msg, "TARGET_MOUNTED") || starts_with(msg, "TARGET_IS_RUNNING")) {
    return "The other boot slot cannot be written safely: " + after_code(msg) +
           ". Nothing was changed.";
  }
  if (starts_with(msg, "NO_7ZA")) {
    return "This NebulaOS version cannot open image packages. Nothing was changed.";
  }
  return after_code(msg) + ". Nothing was changed.";
}

ImageCheck image_check(const json &check_reply) {
  ImageCheck c;
  std::string err = error_of(check_reply);
  const json &r = result_of(check_reply);
  if (err.empty() && str_at(r, "/result"_json_pointer) != "OK") {
    err = "Unexpected answer from the printer";
  }
  if (!err.empty()) {
    c.title = "Cannot install";
    c.text = not_installable(err);
    return c;
  }
  ImageSlots s = image_slots(r.contains("slots") ? r["slots"] : json());
  std::string target = str_at(r, "/target"_json_pointer);
  if (!s.known || (target != "A" && target != "B") || target == s.running) {
    c.title = "Cannot install";
    c.text = "Cannot tell which boot slot (A/B) is free. Nothing was changed.";
    return c;
  }
  c.ok = true;
  c.file = str_at(r, "/file"_json_pointer);
  c.sha256 = str_at(r, "/sha256"_json_pointer);
  c.system = str_at(r, "/system"_json_pointer);
  std::string version = str_at(r, "/version"_json_pointer);
  std::string what = image_identity_text(c.system, str_at(r, "/build"_json_pointer),
                                         str_at(r, "/date"_json_pointer), version);
  const ImageSlot &now = s.of(target);
  std::string replaces = now.kernel.empty() ? "" :
    " It replaces " + image_identity_text(now.system, now.build, now.date, now.version) +
    " in slot " + target + ".";
  std::string keep = "The printer keeps running from slot " + s.running +
                     " while it installs (a few minutes), then restarts into slot " + target + ".";

  if (c.system == "creality") {
    c.title = "Install other system";
    c.text = "Installs " + what + " into slot " + target + "." + replaces + "\n" + keep + "\n"
      "This image is not NebulaOS: there is no automatic way back. To return, install a "
      "NebulaOS .img with that system's own updater, or use USB recovery.\n"
      "That system may re-flash the printer board (MCU) when it starts. "
      "Do not switch the printer off while it does.";
  } else {
    c.title = c.system == "nebulaos" ? "Install NebulaOS image" : "Install unknown system";
    c.text = (c.system == "nebulaos" ? "" :
               "This image is not NebulaOS, and its system is unknown.\n") +
      std::string("Installs ") + what + " into slot " + target + "." + replaces + "\n" + keep +
      "\nIf Klipper or the screen does not come up there, the printer goes back to "
      "slot " + s.running + " once, by itself.\n"
      "Do not switch the printer off while it installs.";
  }
  return c;
}

std::string image_install_error(const json &reply) {
  std::string err = error_of(reply);
  if (!err.empty()) return not_installable(err);
  if (str_at(result_of(reply), "/result"_json_pointer) != "STARTED") {
    return "Unexpected answer from the printer. Nothing was changed.";
  }
  return "";
}

// -- the running job -----------------------------------------------------------------

ImageJob image_job(const json &status_reply) {
  ImageJob j;
  const json &r = result_of(status_reply);
  if (!r.contains("job") || !r["job"].is_object()) return j;
  const json &job = r["job"];
  std::string state = str_at(job, "/state"_json_pointer);
  std::string phase = str_at(job, "/phase"_json_pointer);
  j.percent = (int)long_at(job, "/percent"_json_pointer);
  if (state == "running") {
    j.active = true;
    if (phase == "unpacking") {
      j.text = "Unpacking the image...";
    } else if (phase == "writing") {
      j.text = "Writing the other boot slot: " + std::to_string(j.percent) + "%";
    } else if (phase == "switching") {
      j.text = "Selecting the new slot...";
    } else {
      j.text = "Checking the image...";
    }
  } else if (state == "restarting") {
    j.active = true;
    j.restarting = true;
    j.percent = 100;
    std::string target = str_at(job, "/target"_json_pointer);
    j.text = "Installed. Restarting into slot " + target + "...";
  } else if (state == "finished" && phase == "next_power_on") {
    // G91 v2.2: a print started after the slot was selected and the selection
    // could not be put back: installed, but no reboot under the print.
    j.next_power_on = true;
    j.percent = 100;
    j.text = "Install finished; the printer starts the new system at the next power-on.";
  } else if (state == "failed" && phase == "stopped_print") {
    // G91 v2.2: a print started during the install; the worker stopped at a
    // point of no return (it does not wait for the print any more).
    j.failed = true;
    j.stopped = true;
    j.text = "Stopped because a print was started. Nothing changed. "
             "Install again after the print.";
  } else if (state == "failed") {
    j.failed = true;
    std::string msg = str_at(job, "/message"_json_pointer);
    j.text = (msg.empty() ? std::string("The install failed") : msg) + ".\n\n";
    if (bool_at(job, "/target_cleared"_json_pointer, false)) {
      j.text += "The other boot slot was partly written and is now empty: it cannot be "
                "started until an image is installed into it. This printer keeps running "
                "as before.";
    } else {
      j.text += "The printer keeps running as before.";
    }
  }
  return j;
}

ImageLast image_last(const json &status_reply) {
  ImageLast l;
  const json &r = result_of(status_reply);
  if (!r.contains("last") || !r["last"].is_object()) return l;
  const json &last = r["last"];
  std::string result = str_at(last, "/result"_json_pointer);
  std::string version = str_at(last, "/version"_json_pointer);
  std::string to = str_at(last, "/to"_json_pointer);
  std::string from = str_at(last, "/from"_json_pointer);
  std::string note = str_at(last, "/note"_json_pointer);
  std::string what = image_system_text(str_at(last, "/system"_json_pointer)) +
                     (version.empty() ? "" : " " + version);
  l.show = true;
  if (result == "CONFIRMED") {
    l.title = "System image installed";
    l.text = "Now running " + what + " from slot " + to + ".\n"
             "The previous version stays in slot " + from + ".";
  } else if (result == "ROLLED_BACK") {
    l.title = "Went back to the previous version";
    l.text = what + " in slot " + to + " did not start properly" +
             (note.empty() ? "" : " (" + note + ")") + ".\n"
             "The printer went back to slot " + from + ", which runs as before.";
  } else if (result == "NOT_STARTED") {
    l.title = "New version did not start";
    l.text = "The printer started from slot " + from + " instead of the new slot " + to +
             ". It runs as before.";
  } else if (result == "ROLLBACK_FAILED") {
    l.title = "Could not go back";
    l.text = what + " in slot " + to + " did not start properly, and the previous slot " +
             from + " could not be selected. Check Settings > System > Logs.";
  } else {
    l.show = false;
  }
  return l;
}

bool image_trial_pending(const json &status_reply) {
  const json &r = result_of(status_reply);
  return r.contains("trial") && r["trial"].is_object() && !r["trial"].empty();
}

std::string swap_confirm_text(const json &status_reply) {
  const json &r = result_of(status_reply);
  ImageSlots s = image_slots(r.contains("slots") ? r["slots"] : json());
  if (s.known) {
    const ImageSlot &o = s.of(s.other);
    std::string what = image_identity_text(o.system, o.build, o.date, o.version);
    if (o.system == "nebulaos") {
      return "The printer restarts into slot " + s.other + " (" + what + ").\n"
             "Come back the same way: Settings > Reset > Swap boot slot there.";
    }
    if (o.system == "creality") {
      return "The printer restarts into slot " + s.other + " (" + what + ").\n"
             "It is not NebulaOS: coming back needs SSH on that system (see the NebulaOS "
             "docs on switching slots), installing a NebulaOS .img with its own updater, "
             "or USB recovery.\n"
             "Do not switch the printer off and on while that system runs: it can "
             "re-flash the printer board.";
    }
  }
  return "The printer restarts into the other boot slot (A/B).\n"
         "Coming back needs SSH on that system (see the NebulaOS docs on switching "
         "slots) or USB recovery.\n"
         "Do not switch the printer off and on while the other slot runs: its "
         "firmware can re-flash the printer board.";
}

// -- updates over the internet (G79) ---------------------------------------------------

OtaCheck ota_check(const json &check_reply) {
  OtaCheck c;
  std::string err = error_of(check_reply);
  const json &r = result_of(check_reply);
  if (!err.empty()) {
    c.title = "No update check";
    c.text = starts_with(err, "PRINTING") || starts_with(err, "REFUSED")
               ? "Not possible while a print is active."
               : after_code(err) + ".";
    return c;
  }
  std::string result = str_at(r, "/result"_json_pointer);
  std::string running = str_at(r, "/running_version"_json_pointer);
  c.version = str_at(r, "/version"_json_pointer);
  c.available = bool_at(r, "/available"_json_pointer, false);
  if (c.available) {
    c.title = "NebulaOS " + c.version;
    long size = long_at(r, "/img/size"_json_pointer);
    c.text = "NebulaOS " + c.version + " is available" +
             (running.empty() ? "" : " (this printer runs " + running + ")") + ".\n"
             "Download it from GitHub (" + megabytes(size) + ")? The printer checks it "
             "against the release, then it appears in the list; tap Install to install it.";
    return c;
  }
  c.title = "System update";
  if (result == "UP_TO_DATE") {
    c.text = "NebulaOS is up to date" + (running.empty() ? "" : " (" + running + ")") + ".";
  } else if (result == "NOT_FROM_THIS_VERSION") {
    c.text = "NebulaOS " + c.version + " cannot be installed over this version over the "
             "internet. See the release notes on GitHub.";
  } else if (result == "RUNNING_VERSION_UNKNOWN") {
    c.text = "This printer's NebulaOS version is unknown, so no update is offered.";
  } else {
    std::string msg = str_at(r, "/message"_json_pointer);
    c.text = msg.empty() ? "No update information (" + result + ")." : msg;
  }
  return c;
}

std::string ota_download_error(const json &reply) {
  std::string err = error_of(reply);
  if (!err.empty()) {
    if (starts_with(err, "PRINTING") || starts_with(err, "REFUSED")) {
      return "Not possible while a print is active.";
    }
    return after_code(err) + ".";
  }
  std::string result = str_at(result_of(reply), "/result"_json_pointer);
  if (result == "ALREADY_DOWNLOADED") {
    return "It is already downloaded: tap Install next to it in the list.";
  }
  if (result != "STARTED") {
    return "Unexpected answer from the printer.";
  }
  return "";
}

bool ota_already_downloaded(const json &reply) {
  return str_at(result_of(reply), "/result"_json_pointer) == "ALREADY_DOWNLOADED";
}

OtaDownload ota_download(const json &status_reply) {
  OtaDownload d;
  const json &r = result_of(status_reply);
  if (!r.contains("download") || !r["download"].is_object()) return d;
  const json &job = r["download"];
  std::string state = str_at(job, "/state"_json_pointer);
  d.percent = (int)long_at(job, "/percent"_json_pointer);
  std::string version = str_at(job, "/version"_json_pointer);
  if (state == "running") {
    d.active = true;
    d.text = "Downloading NebulaOS " + version + ": " + std::to_string(d.percent) + "%";
  } else if (state == "finished") {
    d.finished = true;
    d.percent = 100;
    d.text = "NebulaOS " + version + " is downloaded and checked. Tap Install next to it "
             "in the list.";
  } else if (state == "failed") {
    d.failed = true;
    std::string msg = str_at(job, "/message"_json_pointer);
    d.text = (msg.empty() ? std::string("The download failed.") : msg) +
             "\nNothing was installed.";
  }
  return d;
}
