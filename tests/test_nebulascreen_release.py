#!/usr/bin/env python3
"""Host tests for scripts/nebulascreen_release.py (G82 release zip).

Run: python3 tests/test_nebulascreen_release.py

This file may be distributed under the terms of the GNU GPLv3 license.
"""
import json
import os
import shutil
import stat
import struct
import sys
import tempfile
import unittest
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "scripts"))
import nebulascreen_release as rel  # noqa: E402


def fake_elf(machine=8, cls=1, data=1, interp=False, etype=2):
    """A minimal ELF32 header + one program header."""
    hdr = bytearray(52)
    hdr[:4] = b"\x7fELF"
    hdr[4], hdr[5], hdr[6] = cls, data, 1
    struct.pack_into("<HHI", hdr, 16, etype, machine, 1)
    struct.pack_into("<I", hdr, 28, 52)                 # e_phoff
    struct.pack_into("<HH", hdr, 42, 32, 1)             # e_phentsize, e_phnum
    ph = bytearray(32)
    struct.pack_into("<I", ph, 0, rel.PT_INTERP if interp else 1)
    return bytes(hdr + ph) + b"\0" * 64


class ReleaseTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.tmp)
        self.bin = os.path.join(self.tmp, "bin")
        self.themes = os.path.join(self.tmp, "themes")
        self.repo = os.path.join(self.tmp, "repo")
        for d in (self.bin, os.path.join(self.themes, "blue"), self.repo):
            os.makedirs(d)
        for name in rel.EXECUTABLES:
            with open(os.path.join(self.bin, name), "wb") as fh:
                fh.write(fake_elf())
        with open(os.path.join(self.themes, "blue", "theme.json"), "w") as fh:
            fh.write("{}")
        for name in ("LICENSE", "NOTICE"):
            with open(os.path.join(self.repo, name), "w") as fh:
                fh.write(name)
        self.out = os.path.join(self.tmp, "dist", "nebulascreen-v0.9.0.zip")

    def pack(self, version="v0.9.0", epoch=1792000000):
        return rel.package(self.bin, self.themes, self.repo, version, self.out, epoch)

    def test_package_is_valid_and_has_the_contract(self):
        self.pack()
        self.assertEqual(rel.check(self.out), [])
        with zipfile.ZipFile(self.out) as zf:
            info = json.loads(zf.read("release_info.json"))
            self.assertEqual(info, {"project_name": "NebulaOS-nebulascreen",
                                    "project_owner": "NebulaOS-KE",
                                    "version": "v0.9.0", "nebulaos_ui_api": 1})
            mode = zf.getinfo("nebulascreen").external_attr >> 16
            self.assertTrue(stat.S_ISREG(mode) and mode & 0o111)
            self.assertIn("themes/blue/theme.json", zf.namelist())
            self.assertIn("NOTICE", zf.namelist())

    def test_package_is_deterministic(self):
        self.pack()
        first = open(self.out, "rb").read()
        os.utime(os.path.join(self.bin, "nebulascreen"), (1, 1))
        self.pack()
        self.assertEqual(open(self.out, "rb").read(), first)

    def test_bad_version_is_refused(self):
        with self.assertRaises(rel.ReleaseError):
            self.pack(version="nightly-abc")

    def test_symlink_in_themes_is_refused(self):
        os.symlink("/etc/passwd", os.path.join(self.themes, "blue", "evil"))
        with self.assertRaises(rel.ReleaseError):
            self.pack()

    def test_missing_executable_is_refused(self):
        os.unlink(os.path.join(self.bin, "nebulabeep"))
        with self.assertRaises(rel.ReleaseError):
            self.pack()

    def test_elf_checks(self):
        self.assertEqual(rel.elf_problems(fake_elf(), "x"), [])
        self.assertTrue(rel.elf_problems(fake_elf(machine=62), "x"))     # x86-64
        self.assertTrue(rel.elf_problems(fake_elf(cls=2), "x"))          # 64-bit
        self.assertTrue(rel.elf_problems(fake_elf(data=2), "x"))         # big-endian
        self.assertIn("dynamically linked",
                      " ".join(rel.elf_problems(fake_elf(interp=True), "x")))
        self.assertTrue(rel.elf_problems(b"#!/bin/sh\n" * 10, "x"))

    def _zip(self, entries):
        path = os.path.join(self.tmp, "custom.zip")
        with zipfile.ZipFile(path, "w") as zf:
            for name, data, mode in entries:
                zi = zipfile.ZipInfo(name)
                zi.create_system = 3
                zi.external_attr = mode << 16
                zf.writestr(zi, data)
        return path

    def _good_entries(self):
        info = json.dumps(rel.release_info("v0.9.0"))
        return [("nebulascreen", fake_elf(), stat.S_IFREG | 0o755),
                ("nebulabeep", fake_elf(), stat.S_IFREG | 0o755),
                ("themes/blue/theme.json", "{}", stat.S_IFREG | 0o644),
                ("release_info.json", info, stat.S_IFREG | 0o644),
                ("LICENSE", "x", stat.S_IFREG | 0o644),
                ("NOTICE", "x", stat.S_IFREG | 0o644)]

    def test_check_refuses_config_symlink_and_extras(self):
        self.assertEqual(rel.check(self._zip(self._good_entries())), [])
        for bad in (("nebulascreen.json", "{}", stat.S_IFREG | 0o644),
                    ("themes/link", "/etc", stat.S_IFLNK | 0o777),
                    ("../escape", "x", stat.S_IFREG | 0o644),
                    ("installer.sh", "x", stat.S_IFREG | 0o755)):
            self.assertTrue(rel.check(self._zip(self._good_entries() + [bad])), bad[0])

    def test_check_refuses_a_wrong_release_info(self):
        entries = self._good_entries()
        entries[3] = ("release_info.json",
                      json.dumps({"project_name": "guppyscreen", "version": "v0.9.0"}),
                      stat.S_IFREG | 0o644)
        problems = " ".join(rel.check(self._zip(entries)))
        self.assertIn("project_name", problems)
        self.assertIn("nebulaos_ui_api", problems)

    def test_check_refuses_a_non_executable_binary(self):
        entries = self._good_entries()
        entries[0] = ("nebulascreen", fake_elf(), stat.S_IFREG | 0o644)
        self.assertIn("nebulascreen is not executable", rel.check(self._zip(entries)))


if __name__ == "__main__":
    unittest.main()
