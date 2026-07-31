#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

usage() {
  cat <<'EOF'
Usage:
  scripts/test-containers.sh [fedora|ubuntu|debian|arch|opensuse|all]

Runs a distro smoke test in Podman:
  - installs build/runtime validation dependencies inside the container
  - configures and builds WSF with Meson
  - installs WSF into the disposable container
  - checks CLI JSON output
  - checks Python GUI syntax
  - validates installed desktop/metainfo files when validators are available

These tests do not validate real compositor input behavior. GNOME
gesture behavior must still be tested in a real graphical Wayland session.
EOF
}

ENGINE="${CONTAINER_ENGINE:-}"
if [ -z "$ENGINE" ]; then
  if command -v podman >/dev/null 2>&1; then
    ENGINE="podman"
  elif command -v docker >/dev/null 2>&1; then
    ENGINE="docker"
  fi
fi

if [ -z "$ENGINE" ] || ! command -v "$ENGINE" >/dev/null 2>&1; then
  echo "container engine not found (install podman or docker)" >&2
  exit 1
fi

RUN_SECURITY_ARGS=()
if [ "$ENGINE" = "podman" ]; then
  RUN_SECURITY_ARGS=(--security-opt label=disable)
fi

target="${1:-fedora}"
case "$target" in
  -h|--help)
    usage
    exit 0
    ;;
  fedora|ubuntu|debian|arch|opensuse|all)
    ;;
  *)
    usage >&2
    exit 2
    ;;
esac

run_case() {
  local name="$1"
  local image="$2"
  local install_cmd="$3"

  echo "==> ${name}: ${image}"
  "$ENGINE" run --rm \
    "${RUN_SECURITY_ARGS[@]}" \
    -v "${ROOT}:/work:ro" \
    -w /work \
    "$image" \
    bash -lc "
      set -euo pipefail
      ${install_cmd}
      rm -rf /tmp/wsf-build /tmp/wsf-home
      mkdir -p /tmp/wsf-home
      meson setup /tmp/wsf-build /work --prefix=/usr --buildtype=release
      meson compile -C /tmp/wsf-build
      meson install -C /tmp/wsf-build
      HOME=/tmp/wsf-home /usr/bin/wsf set 0.35
      HOME=/tmp/wsf-home /usr/bin/wsf get --json | grep 'scroll_vertical_factor'
      HOME=/tmp/wsf-home /usr/bin/wsf status --json | grep '\"library\"'
      python3 -m py_compile /usr/bin/wsf-gui
      if command -v desktop-file-validate >/dev/null 2>&1; then
        desktop-file-validate /usr/share/applications/io.github.danielgrasso.WaylandScrollFactor.desktop
      fi
      # Hyprland is an opt-in meson feature: the default build must not
      # install any Hyprland artifacts.
      test ! -e /usr/share/wayland-sessions/wayland-scroll-factor-hyprland.desktop
      test ! -e /usr/bin/wsf-hyprland
      test ! -e /usr/bin/wsf-start-hyprland
      test ! -e /usr/bin/wsf-session-wrapper
      test ! -e /usr/share/wayland-scroll-factor/hyprland/wsf.lua
      if command -v appstreamcli >/dev/null 2>&1; then
        appstreamcli validate --no-net /usr/share/metainfo/io.github.danielgrasso.WaylandScrollFactor.metainfo.xml
      fi
    "
}

run_fedora() {
  run_case \
    "fedora" \
    "docker.io/library/fedora:latest" \
    "dnf -y --setopt=install_weak_deps=False install gcc gcc-c++ make meson ninja-build pkgconf-pkg-config git python3 python3-gobject gtk4 libadwaita desktop-file-utils appstream grep"
}

run_ubuntu() {
  run_case \
    "ubuntu" \
    "docker.io/library/ubuntu:24.04" \
    "apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends build-essential meson ninja-build pkg-config git python3 python3-gi gir1.2-adw-1 libgtk-4-1 libadwaita-1-0 desktop-file-utils appstream grep"
}

run_debian() {
  run_case \
    "debian" \
    "docker.io/library/debian:stable" \
    "apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends build-essential meson ninja-build pkg-config git python3 python3-gi gir1.2-adw-1 libgtk-4-1 libadwaita-1-0 desktop-file-utils appstream grep"
}

run_arch() {
  run_case \
    "arch" \
    "docker.io/library/archlinux:latest" \
    "pacman -Sy --noconfirm --needed base-devel meson ninja pkgconf git python python-gobject gtk4 libadwaita desktop-file-utils appstream grep"
}

run_opensuse() {
  run_case \
    "opensuse" \
    "registry.opensuse.org/opensuse/tumbleweed:latest" \
    "zypper --non-interactive install --no-recommends gcc gcc-c++ make meson ninja pkg-config git python3 python3-gobject gtk4 libadwaita-1-0 desktop-file-utils AppStream grep"
}

case "$target" in
  fedora) run_fedora ;;
  ubuntu) run_ubuntu ;;
  debian) run_debian ;;
  arch) run_arch ;;
  opensuse) run_opensuse ;;
  all)
    run_fedora
    run_ubuntu
    run_debian
    run_arch
    run_opensuse
    ;;
esac
