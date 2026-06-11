#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

usage() {
  cat <<'EOF'
Usage:
  scripts/test-packages.sh [rpm|deb|all]

Builds distro packages inside disposable Podman containers.
No package is installed on the host.
EOF
}

if ! command -v podman >/dev/null 2>&1; then
  echo "podman not found" >&2
  exit 1
fi

target="${1:-all}"
case "$target" in
  -h|--help)
    usage
    exit 0
    ;;
  rpm|deb|all)
    ;;
  *)
    usage >&2
    exit 2
    ;;
esac

project_version() {
  sed -n "s/[[:space:]]*version:[[:space:]]*'\\([^']*\\)'.*/\\1/p" "$ROOT/meson.build" | head -n1
}

run_rpm() {
  local version
  version="$(project_version)"

  podman run --rm \
    --security-opt label=disable \
    -v "${ROOT}:/work:ro" \
    -w /work \
    docker.io/library/fedora:latest \
    bash -lc "
      set -euo pipefail
      dnf -y --setopt=install_weak_deps=False install \
        rpm-build gcc meson ninja-build python3 python3-gobject gtk4 libadwaita \
        desktop-file-utils appstream tar gzip findutils
      rm -rf /tmp/rpmbuild /tmp/wsf-src
      mkdir -p /tmp/rpmbuild/SOURCES /tmp/rpmbuild/SPECS /tmp/wsf-src
      tar \
        --exclude='./.git' \
        --exclude='./build' \
        --exclude='./packaging/aur-worktrees' \
        -C /work -cf - . | tar -C /tmp/wsf-src -xf -
      mv /tmp/wsf-src /tmp/wayland-scroll-factor-${version}
      tar -C /tmp -czf /tmp/rpmbuild/SOURCES/wayland-scroll-factor-${version}.tar.gz wayland-scroll-factor-${version}
      cp /work/packaging/rpm/wayland-scroll-factor.spec /tmp/rpmbuild/SPECS/
      rpmbuild --define '_topdir /tmp/rpmbuild' -ba /tmp/rpmbuild/SPECS/wayland-scroll-factor.spec
      find /tmp/rpmbuild/RPMS /tmp/rpmbuild/SRPMS -type f -print
    "
}

run_deb() {
  local version
  version="$(project_version)"

  podman run --rm \
    --security-opt label=disable \
    -v "${ROOT}:/work:ro" \
    -w /work \
    docker.io/library/debian:stable \
    bash -lc "
      set -euo pipefail
      apt-get update
      DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        build-essential dpkg-dev debhelper meson ninja-build pkg-config gcc \
        python3 python3-gi gir1.2-gtk-4.0 gir1.2-adw-1 desktop-file-utils \
        appstream ca-certificates tar xz-utils
      rm -rf /tmp/wayland-scroll-factor-${version}
      mkdir -p /tmp/wayland-scroll-factor-${version}
      tar \
        --exclude='./.git' \
        --exclude='./build' \
        --exclude='./packaging/aur-worktrees' \
        -C /work -cf - . | tar -C /tmp/wayland-scroll-factor-${version} -xf -
      rm -rf /tmp/wayland-scroll-factor-${version}/debian
      cp -a /work/packaging/debian/debian /tmp/wayland-scroll-factor-${version}/debian
      chmod +x /tmp/wayland-scroll-factor-${version}/debian/rules
      cd /tmp/wayland-scroll-factor-${version}
      dpkg-buildpackage -us -uc -b
      find /tmp -maxdepth 1 -type f \\( -name '*.deb' -o -name '*.buildinfo' -o -name '*.changes' \\) -print
    "
}

case "$target" in
  rpm) run_rpm ;;
  deb) run_deb ;;
  all)
    run_rpm
    run_deb
    ;;
esac
