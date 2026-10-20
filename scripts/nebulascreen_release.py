#!/usr/bin/env python3
"""NebulaScreen release package (G82, Run 15 stage 1b).

One zip asset per release, installed by Moonraker's update_manager
(`type: zip`) into /usr/data/nebulaos/apps/nebulascreen and validated by the
NebulaOS update supervisor before it is used:

    nebulascreen            static ELF32 little-endian MIPS executable
    nebulabeep              static ELF32 little-endian MIPS executable
    themes/...              theme files
    release_info.json       project_name, project_owner, version,
                            nebulaos_ui_api (the image contract it needs)
    LICENSE, NOTICE         GPL-3; based on GuppyScreen by ballaswag

Never in the zip: symlinks, the user's config (nebulascreen.json /
guppyconfig.json), anything outside these entries.

    nebulascreen_release.py package --bin-dir build/bin --version v0.9.0 \\
        --out dist/nebulascreen-v0.9.0.zip [--epoch N]
    nebulascreen_release.py check dist/nebulascreen-v0.9.0.zip

The zip is deterministic: sorted entries, every timestamp = --epoch
(SOURCE_DATE_EPOCH of the pinned commit), fixed unix modes (0755 for the
executables, 0644 for files, 0755 for directories).

This file may be distributed under the terms of the GNU GPLv3 license.
"""
import argparse
import json
import os
import re
import stat
import struct
import sys
import time
import zipfile

PROJECT_NAME = "NebulaOS-nebulascreen"
PROJECT_OWNER = "coreflake1"
# The image path/name contract this app needs (see src/nebulascreen_paths.h):
# an image provides it in /opt/nebulaos-version.json "nebulaos_ui_api".
NEBULAOS_UI_API = 1
EXECUTABLES = ("nebulascreen", "nebulabeep")
TOP_FILES = ("release_info.json", "LICENSE", "NOTICE")
FORBIDDEN_NAMES = ("nebulascreen.json", "guppyconfig.json")
VERSION_RE = re.compile(r"^v\d+\.\d+\.\d+([-.][0-9A-Za-z.-]+)?$")
EM_MIPS = 8
PT_INTERP = 3


class ReleaseError(Exception):
    pass


def elf_problems(data, name):
    """[] for a static ELF32 little-endian MIPS executable."""
    if len(data) < 52 or data[:4] != b"\x7fELF":
        return ["%s is not an ELF file" % name]
    problems = []
    if data[4] != 1:
        problems.append("%s is not 32-bit" % name)
    if data[5] != 1:
        problems.append("%s is not little-endian" % name)
    if problems:
        return problems
    e_type, e_machine = struct.unpack_from("<HH", data, 16)
    if e_machine != EM_MIPS:
        problems.append("%s is not MIPS (e_machine %d)" % (name, e_machine))
    if e_type != 2:
        problems.append("%s is not an executable (e_type %d)" % (name, e_type))
    e_phoff, = struct.unpack_from("<I", data, 28)
    e_phentsize, e_phnum = struct.unpack_from("<HH", data, 42)
    for i in range(e_phnum):
        off = e_phoff + i * e_phentsize
        if off + 4 > len(data):
            problems.append("%s has a truncated program header" % name)
            break
        p_type, = struct.unpack_from("<I", data, off)
        if p_type == PT_INTERP:
            problems.append("%s is dynamically linked (has an interpreter)" % name)
            break
    return problems


def release_info(version):
    return {"project_name": PROJECT_NAME, "project_owner": PROJECT_OWNER,
            "version": version, "nebulaos_ui_api": NEBULAOS_UI_API}


def _entries(bin_dir, themes_dir, repo_dir, version):
    """[(archive name, bytes or None for a directory, mode)], sorted."""
    out = []
    for name in EXECUTABLES:
        path = os.path.join(bin_dir, name)
        if not os.path.isfile(path) or os.path.islink(path):
            raise ReleaseError("missing executable %s" % path)
        with open(path, "rb") as fh:
            out.append((name, fh.read(), 0o755))
    if not os.path.isdir(themes_dir):
        raise ReleaseError("missing themes directory %s" % themes_dir)
    out.append(("themes/", None, 0o755))
    for root, dirs, files in os.walk(themes_dir):
        dirs.sort()
        rel = os.path.relpath(root, themes_dir)
        for d in dirs:
            if os.path.islink(os.path.join(root, d)):
                raise ReleaseError("symlink in themes: %s" % os.path.join(root, d))
            out.append((os.path.normpath(os.path.join("themes", rel, d)) + "/", None, 0o755))
        for f in sorted(files):
            p = os.path.join(root, f)
            if os.path.islink(p):
                raise ReleaseError("symlink in themes: %s" % p)
            with open(p, "rb") as fh:
                out.append((os.path.normpath(os.path.join("themes", rel, f)), fh.read(), 0o644))
    info = json.dumps(release_info(version), indent=2, sort_keys=True) + "\n"
    out.append(("release_info.json", info.encode(), 0o644))
    for name in ("LICENSE", "NOTICE"):
        with open(os.path.join(repo_dir, name), "rb") as fh:
            out.append((name, fh.read(), 0o644))
    return sorted(out, key=lambda e: e[0])


def package(bin_dir, themes_dir, repo_dir, version, out, epoch):
    if not VERSION_RE.match(version):
        raise ReleaseError("version %r is not vMAJOR.MINOR.PATCH" % (version,))
    entries = _entries(bin_dir, themes_dir, repo_dir, version)
    stamp = time.gmtime(max(int(epoch), 315532800))[:6]   # zip cannot go before 1980
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    tmp = out + ".tmp"
    with zipfile.ZipFile(tmp, "w", zipfile.ZIP_DEFLATED) as zf:
        for name, data, mode in entries:
            zi = zipfile.ZipInfo(name, date_time=stamp)
            zi.create_system = 3                       # unix: keep the modes
            if data is None:
                zi.external_attr = (stat.S_IFDIR | mode) << 16 | 0x10
                zf.writestr(zi, b"")
            else:
                zi.external_attr = (stat.S_IFREG | mode) << 16
                zi.compress_type = zipfile.ZIP_DEFLATED
                zf.writestr(zi, data)
    os.replace(tmp, out)
    problems = check(out)
    if problems:
        raise ReleaseError("; ".join(problems))
    return out


def check(path):
    """[] for a valid release zip, else the problems."""
    problems = []
    try:
        zf = zipfile.ZipFile(path)
    except (OSError, zipfile.BadZipFile) as exc:
        return ["not a zip: %s" % (exc,)]
    with zf:
        names = zf.namelist()
        infos = {i.filename: i for i in zf.infolist()}
        for name, info in infos.items():
            mode = info.external_attr >> 16
            if stat.S_ISLNK(mode):
                problems.append("symlink in the zip: %s" % name)
            if name.startswith("/") or ".." in name.split("/"):
                problems.append("unsafe path: %s" % name)
            if os.path.basename(name.rstrip("/")) in FORBIDDEN_NAMES:
                problems.append("a config file is in the zip: %s" % name)
            top = name.split("/", 1)[0]
            if top not in EXECUTABLES + TOP_FILES + ("themes",):
                problems.append("unexpected entry: %s" % name)
        for name in EXECUTABLES:
            if name not in infos:
                problems.append("missing %s" % name)
                continue
            if not (infos[name].external_attr >> 16) & 0o100:
                problems.append("%s is not executable" % name)
            problems += elf_problems(zf.read(name), name)
        if not any(n.startswith("themes/") and not n.endswith("/") for n in names):
            problems.append("no themes")
        for name in TOP_FILES:
            if name not in infos:
                problems.append("missing %s" % name)
        if "release_info.json" in infos:
            try:
                info = json.loads(zf.read("release_info.json").decode("utf-8"))
            except ValueError:
                info = None
                problems.append("release_info.json is not valid JSON")
            if isinstance(info, dict):
                if info.get("project_name") != PROJECT_NAME:
                    problems.append("release_info project_name is %r" % info.get("project_name"))
                if info.get("project_owner") != PROJECT_OWNER:
                    problems.append("release_info project_owner is %r" % info.get("project_owner"))
                if not VERSION_RE.match(str(info.get("version", ""))):
                    problems.append("release_info version is %r" % info.get("version"))
                if not isinstance(info.get("nebulaos_ui_api"), int):
                    problems.append("release_info has no integer nebulaos_ui_api")
    return problems


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("package")
    p.add_argument("--bin-dir", required=True)
    p.add_argument("--themes-dir", default=None)
    p.add_argument("--version", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--epoch", default=os.environ.get("SOURCE_DATE_EPOCH", "315532800"))
    c = sub.add_parser("check")
    c.add_argument("zip")
    args = ap.parse_args(argv)
    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    try:
        if args.cmd == "package":
            out = package(args.bin_dir, args.themes_dir or os.path.join(repo, "themes"),
                          repo, args.version, args.out, args.epoch)
            print("RELEASE_ZIP=%s" % out)
            return 0
        problems = check(args.zip)
    except ReleaseError as exc:
        print("RELEASE_ERROR: %s" % exc, file=sys.stderr)
        return 1
    for p in problems:
        print("RELEASE_PROBLEM: %s" % p, file=sys.stderr)
    print("RELEASE_CHECK=%s" % ("PASS" if not problems else "FAIL"))
    return 0 if not problems else 1


if __name__ == "__main__":
    sys.exit(main())
