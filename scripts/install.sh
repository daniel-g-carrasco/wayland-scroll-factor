#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PREFIX="${PREFIX:-$HOME/.local}"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"

if ! command -v meson >/dev/null 2>&1; then
  echo "meson not found. Install it first (e.g. with your distro package manager)." >&2
  exit 1
fi
if ! command -v ninja >/dev/null 2>&1; then
  echo "ninja not found. Install it first (e.g. with your distro package manager)." >&2
  exit 1
fi

if [ ! -f "$BUILD_DIR/build.ninja" ]; then
  meson setup "$BUILD_DIR" "$ROOT" --prefix="$PREFIX"
else
  meson setup --reconfigure "$BUILD_DIR" "$ROOT" --prefix="$PREFIX" --buildtype=release
fi

ninja -C "$BUILD_DIR"
meson install -C "$BUILD_DIR"

if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database "$PREFIX/share/applications" || true
fi

if command -v gtk-update-icon-cache >/dev/null 2>&1; then
  gtk-update-icon-cache -q "$PREFIX/share/icons/hicolor" || true
fi

echo "Installed to $PREFIX"
echo "You may need to log out and back in to see the launcher."
