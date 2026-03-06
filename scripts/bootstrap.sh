#!/usr/bin/env bash
set -euo pipefail

REPO_URL="https://github.com/daniel-g-carrasco/wayland-scroll-factor.git"
DEST="${DEST:-$HOME/wayland-scroll-factor}"

install_dnf_libinput_pkg() {
  if sudo dnf install -y libinput-utils; then
    return 0
  fi
  if sudo dnf install -y libinput-tools; then
    return 0
  fi
  echo "Warning: could not install libinput CLI package (libinput-utils/libinput-tools)." >&2
  echo "WSF can still run; 'wsf doctor' will show less diagnostics until it's installed." >&2
  return 0
}

install_deps() {
  if command -v apt-get >/dev/null 2>&1; then
    sudo apt-get update
    sudo apt-get install -y build-essential meson ninja-build pkg-config git \
      python3 python3-gi gir1.2-adw-1 libgtk-4-1 libadwaita-1-0 libinput-tools
    return 0
  fi

  if command -v dnf >/dev/null 2>&1; then
    sudo dnf install -y gcc gcc-c++ make meson ninja-build pkgconf-pkg-config git \
      python3 python3-gobject gtk4 libadwaita
    install_dnf_libinput_pkg
    return 0
  fi

  if command -v pacman >/dev/null 2>&1; then
    sudo pacman -S --needed base-devel meson ninja pkgconf git \
      python python-gobject gtk4 libadwaita libinput-tools
    return 0
  fi

  if command -v zypper >/dev/null 2>&1; then
    sudo zypper install -y gcc gcc-c++ make meson ninja pkg-config git \
      python3 python3-gobject gtk4 libadwaita-1-0 libinput-tools
    return 0
  fi

  echo "Unsupported distro: install dependencies manually." >&2
  return 1
}

install_deps

if [ -d "$DEST/.git" ]; then
  git -C "$DEST" pull --rebase
else
  git clone "$REPO_URL" "$DEST"
fi

"$DEST/scripts/install.sh"

echo "Done. Run 'wsf-gui' or 'wsf status' to verify."
