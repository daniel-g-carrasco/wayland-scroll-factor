#!/usr/bin/env bash
# Disposable Ubuntu distrobox for iterating on the WSF GUI without touching
# the host (or rebasing an atomic distro). distrobox shares $HOME and the
# Wayland socket, so `wsf-gui` launched from the box opens on your host
# session.
#
#   scripts/dev-distrobox.sh create   # one-time: make the box + deps + build
#   scripts/dev-distrobox.sh build    # rebuild + reinstall wsf in the box
#   scripts/dev-distrobox.sh gui      # launch the GUI from the box
#   scripts/dev-distrobox.sh cli ...  # run the box's wsf with args
#   scripts/dev-distrobox.sh enter    # shell into the box
#   scripts/dev-distrobox.sh rm       # delete the box
#
# Typical loop: edit code -> `build` -> `gui`.
#
# SCOPE: this exercises the GUI APP (window, About dialog, sliders, toasts)
# and the CLI it drives. It does NOT test real scroll-injection behaviour —
# the preload has to be loaded by the compositor's own gnome-shell, which a
# shared-session container does not give you. Use a separate VM for that.
set -euo pipefail

BOX="${WSF_DEV_BOX:-wsf-dev}"
IMAGE="${WSF_DEV_IMAGE:-ubuntu:24.04}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="/tmp/wsf-dev-build-${BOX}"

# Build + runtime deps. The C parts (preload + CLI) need only a toolchain;
# the Python GUI needs the GTK4/libadwaita GObject introspection at runtime.
DEPS=(
  build-essential meson ninja-build pkg-config git
  python3 python3-gi gir1.2-gtk-4.0 gir1.2-adw-1
  libgtk-4-1 libadwaita-1-0 desktop-file-utils
)

need_distrobox() {
  command -v distrobox >/dev/null 2>&1 && return 0
  echo "distrobox not found. It ships with Bluefin/Bazzite; otherwise:" >&2
  echo "  brew install distrobox   (or your distro's package)" >&2
  exit 1
}

box_exists() { distrobox list 2>/dev/null | awk '{print $3}' | grep -qx "$BOX"; }

cmd_create() {
  need_distrobox
  if box_exists; then
    echo "Box '$BOX' already exists — installing deps + building."
  else
    echo "Creating box '$BOX' from $IMAGE ..."
    distrobox create --yes --name "$BOX" --image "$IMAGE"
  fi
  distrobox enter "$BOX" -- bash -lc "
    set -e
    sudo apt-get update -qq
    sudo DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends ${DEPS[*]}
  "
  cmd_build
  cat <<EOF

Ready. Iterate with:
  $0 build    # after editing code
  $0 gui      # launch the GUI on your session
EOF
}

cmd_build() {
  need_distrobox
  box_exists || { echo "Box '$BOX' does not exist — run: $0 create" >&2; exit 1; }
  distrobox enter "$BOX" -- bash -lc "
    set -e
    cd '$ROOT'
    rm -rf '$BUILD_DIR'
    meson setup '$BUILD_DIR' --prefix=/usr --buildtype=debug
    ninja -C '$BUILD_DIR'
    sudo ninja -C '$BUILD_DIR' install
    echo -n 'installed: '; wsf version
  "
}

cmd_gui() {
  need_distrobox
  box_exists || { echo "Box '$BOX' does not exist — run: $0 create" >&2; exit 1; }
  distrobox enter "$BOX" -- wsf-gui
}

cmd_cli() {
  need_distrobox
  box_exists || { echo "Box '$BOX' does not exist — run: $0 create" >&2; exit 1; }
  distrobox enter "$BOX" -- wsf "$@"
}

cmd_enter() { need_distrobox; distrobox enter "$BOX"; }

cmd_rm() {
  need_distrobox
  distrobox rm --force "$BOX" && echo "Removed box '$BOX'."
}

case "${1:-}" in
  create) cmd_create ;;
  build)  cmd_build ;;
  gui)    cmd_gui ;;
  cli)    shift; cmd_cli "$@" ;;
  enter)  cmd_enter ;;
  rm)     cmd_rm ;;
  -h|--help|"")
    sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'
    ;;
  *)
    echo "unknown command: $1" >&2
    echo "run: $0 --help" >&2
    exit 2
    ;;
esac
