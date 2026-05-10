#!/usr/bin/env bash
set -euo pipefail

PREFIX="${PREFIX:-$HOME/.local}"

rm -f "$PREFIX/bin/wsf"
rm -f "$PREFIX/bin/wsf-gui"
rm -f "$PREFIX/bin/wsf-hyprland"
rm -f "$PREFIX/bin/wsf-session-wrapper"
rm -f "$PREFIX/bin/wsf-start-hyprland"
rm -f "$PREFIX/lib/wayland-scroll-factor/libwsf_preload.so"
rm -rf "$PREFIX/lib/wayland-scroll-factor"
rm -f "$PREFIX/share/applications/io.github.danielgrasso.WaylandScrollFactor.desktop"
rm -f "$PREFIX/share/wayland-sessions/wayland-scroll-factor-hyprland.desktop"
rm -f "$PREFIX/share/metainfo/io.github.danielgrasso.WaylandScrollFactor.metainfo.xml"
rm -f "$PREFIX/share/icons/hicolor/128x128/apps/io.github.danielgrasso.WaylandScrollFactor.png"
rm -f "$PREFIX/share/icons/hicolor/256x256/apps/io.github.danielgrasso.WaylandScrollFactor.png"
rm -f "$PREFIX/share/icons/hicolor/512x512/apps/io.github.danielgrasso.WaylandScrollFactor.png"

if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database "$PREFIX/share/applications" || true
fi

if command -v gtk-update-icon-cache >/dev/null 2>&1; then
  gtk-update-icon-cache -q "$PREFIX/share/icons/hicolor" || true
fi

echo "Removed files from $PREFIX"
echo "To remove user config and env file, run:"
echo "  rm -f ~/.config/environment.d/wayland-scroll-factor.conf"
echo "  rm -rf ~/.config/wayland-scroll-factor"
