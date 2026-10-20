#!/bin/bash
# NebulaScreen release build (G82, Run 15 stage 1b) - the ONE build recipe,
# used by this repo's tag CI and by NebulaOS-firmware's image build (04).
#
#   scripts/build-release.sh v0.9.0
#
# Environment: CROSS_COMPILE (default mipsel-linux-), the toolchain on PATH,
# NEBULASCREEN_THEME (default blue), SOURCE_DATE_EPOCH (default: the commit
# date of HEAD - never the wall clock).
#
# Output: build/bin/{nebulascreen,nebulabeep} (stripped, static MIPS) and
# dist/nebulascreen-<version>.zip (checked by scripts/nebulascreen_release.py).
set -euo pipefail

VERSION="${1:?usage: scripts/build-release.sh vMAJOR.MINOR.PATCH}"
REPO="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO"
CROSS_COMPILE="${CROSS_COMPILE:-mipsel-linux-}"
STRIP="${STRIP:-${CROSS_COMPILE}strip}"
EPOCH="${SOURCE_DATE_EPOCH:-$(git -C "$REPO" show -s --format=%ct HEAD)}"
case "$EPOCH" in ''|*[!0-9]*) echo "FATAL: no SOURCE_DATE_EPOCH" >&2; exit 1 ;; esac

export SOURCE_DATE_EPOCH="$EPOCH" NEBULASCREEN_VERSION="$VERSION" \
	GUPPY_THEME="${NEBULASCREEN_THEME:-blue}"

# The two submodule patches (the same check-then-apply as NebulaOS-firmware
# 00-fetch-vendor-sources.sh): safe on an already patched checkout.
for entry in "0002-spdlog_fmt_initializer_list.patch spdlog" "0003-lvgl-dpi-text-scale.patch lvgl"; do
	patch_file=${entry% *}
	sub=${entry#* }
	if git -C "$sub" apply --check "$REPO/patches/$patch_file" 2>/dev/null; then
		git -C "$sub" apply "$REPO/patches/$patch_file"
	fi
done

rm -rf build libhv/build-mips spdlog/build-mips dist
# Host libraries first, exactly as NebulaOS-firmware 04 does; build-mips.sh
# builds the MIPS ones and restores these afterwards.
env -u CROSS_COMPILE make libhv.a
env -u CROSS_COMPILE make libspdlog.a
CROSS_COMPILE="$CROSS_COMPILE" bash scripts/build-mips.sh
"$STRIP" build/bin/nebulascreen build/bin/nebulabeep

# The binary must be a function of the commit, not the calendar.
EXPECT_DATE=$(LC_ALL=C date -u -d "@$EPOCH" '+%b %e %Y')
# (via a file: with pipefail, grep -q closing the pipe early made strings
# fail with SIGPIPE and the check report a missing date that was there)
strings -a build/bin/nebulascreen > build/nebulascreen.strings
if ! grep -qF "$EXPECT_DATE" build/nebulascreen.strings; then
	echo "FATAL: nebulascreen does not embed the commit date ($EXPECT_DATE) - SOURCE_DATE_EPOCH did not reach the compiler" >&2
	echo "       SOURCE_DATE_EPOCH=$SOURCE_DATE_EPOCH; date-shaped strings actually embedded:" >&2
	grep -aoE '(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec) [ 0-9][0-9] [0-9]{4}' build/nebulascreen.strings | sort -u >&2 || true
	exit 1
fi

python3 scripts/nebulascreen_release.py package --bin-dir build/bin \
	--version "$VERSION" --out "dist/nebulascreen-$VERSION.zip" --epoch "$EPOCH"
python3 scripts/nebulascreen_release.py check "dist/nebulascreen-$VERSION.zip"
