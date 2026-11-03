#!/usr/bin/env bash
# scripts/check-version.sh [X.Y.Z]
#
# Reads the version from every file that carries one and fails when they
# disagree. With an argument (the tag without its v, as the release, COPR
# and AUR workflows pass it) each must also equal it, and CHANGELOG.md must
# hold no unreleased entries. Written after v1.0.1 shipped .deb files called
# 1.0.0 (2026-10-30): scripts/bump-version.sh writes all of these at once.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

EXPECTED="${1:-}"
if [ -n "$EXPECTED" ] && ! [[ "$EXPECTED" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "error: '$EXPECTED' is not X.Y.Z (pass the tag without its v)" >&2
  exit 2
fi

NAMES=(meson.build "rpm spec" "debian changelog" metainfo CHANGELOG.md README.md)
declare -A FOUND
FOUND[meson.build]="$(sed -n "s/^  version: '\([0-9.]*\)',\$/\1/p" meson.build)"
FOUND["rpm spec"]="$(sed -n 's/^Version: *\([0-9.]*\)$/\1/p' packaging/rpm/wayland-scroll-factor.spec)"
FOUND["debian changelog"]="$(sed -n '1s/^wayland-scroll-factor (\([0-9.]*\)-[0-9]*) .*/\1/p' packaging/debian/debian/changelog)"
FOUND[metainfo]="$(grep -m1 -o '<release version="[0-9.]*"' data/io.github.danielgrasso.WaylandScrollFactor.metainfo.xml | sed 's/.*"\(.*\)"/\1/')"
FOUND[CHANGELOG.md]="$(grep -m1 -o '^## \[[0-9][0-9.]*\]' CHANGELOG.md | tr -d '#[] ')"
FOUND[README.md]="$(sed -n 's/.*Current release: \([0-9.]*\)\..*/\1/p' README.md | head -n 1)"

REF="${EXPECTED:-${FOUND[meson.build]}}"
bad=0
for name in "${NAMES[@]}"; do
  v="${FOUND[$name]}"
  if [ -z "$v" ]; then
    printf '  %-18s (no version found)\n' "$name"
    bad=1
  elif [ "$v" = "$REF" ]; then
    printf '  %-18s %s\n' "$name" "$v"
  else
    printf '  %-18s %s   <- differs\n' "$name" "$v"
    bad=1
  fi
done

if [ -n "$EXPECTED" ]; then
  pending="$(awk '
    /^## \[Unreleased\]/ { on = 1; next }
    on && /^## \[/ { exit }
    on && /^- / { n++ }
    END { print n + 0 }
  ' CHANGELOG.md)"
  if [ "$pending" != 0 ]; then
    echo "  CHANGELOG.md still has $pending unreleased entr$([ "$pending" = 1 ] && echo y || echo ies) above [$EXPECTED]"
    bad=1
  fi
fi

if [ "$bad" != 0 ]; then
  if [ -n "$EXPECTED" ]; then
    echo "error: the files do not all say $EXPECTED; run scripts/bump-version.sh $EXPECTED before tagging" >&2
  else
    echo "error: the version files disagree; scripts/bump-version.sh writes them all at once" >&2
  fi
  exit 1
fi
echo "version ${REF}: all six agree"
