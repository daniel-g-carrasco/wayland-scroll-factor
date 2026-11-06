#!/usr/bin/env bash
# scripts/check-icons.sh [ICONS_DIR]
#
# Loads the app icon the way GNOME does and fails when a menu would show a
# blank square. Written after #45: v1.0.1 to v1.0.3 shipped an SVG whose
# <svg> tag came after a four-line comment, gdk-pixbuf did not recognise the
# file, and Ubuntu's menus showed nothing. Inkscape and Qt opened it fine,
# so looking at the icon in a launcher was no proof.
#
# Checks, through gdk-pixbuf (python3-gi, the SVG loader from librsvg):
#   - the SVG loads at the menu sizes (16 to 64 px) and at its own size;
#   - every PNG loads and is as large as the folder it sits in says;
#   - the icon name the desktop file asks for is the name of every file.
#
# ICONS_DIR defaults to data/icons in the source tree; CI runs it there, and
# the distro smoke tests run it on /usr/share/icons after `meson install`,
# with each distribution's own gdk-pixbuf.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ICONS_DIR="${1:-$ROOT/data/icons}"
DESKTOP_IN="$ROOT/data/io.github.danielgrasso.WaylandScrollFactor.desktop.in"

ICON_NAME="$(sed -n 's/^Icon=//p' "$DESKTOP_IN" | head -n 1)"
if [ -z "$ICON_NAME" ]; then
  echo "error: no Icon= line in $DESKTOP_IN" >&2
  exit 2
fi

exec python3 - "$ICONS_DIR" "$ICON_NAME" <<'PY'
import os
import re
import sys

import gi

gi.require_version("GdkPixbuf", "2.0")
from gi.repository import GdkPixbuf  # noqa: E402

icons_dir, name = sys.argv[1], sys.argv[2]
apps = os.path.join(icons_dir, "hicolor")
bad = 0


def fail(message):
    global bad
    bad += 1
    print(f"  FAIL {message}")


def load(path, size=None):
    if size is None:
        return GdkPixbuf.Pixbuf.new_from_file(path)
    return GdkPixbuf.Pixbuf.new_from_file_at_size(path, size, size)


svg = os.path.join(apps, "scalable", "apps", f"{name}.svg")
if not os.path.isfile(svg):
    fail(f"missing {svg}")
else:
    for size in (16, 24, 32, 48, 64):
        try:
            pb = load(svg, size)
        except Exception as exc:  # GLib.Error: the format was not recognised
            fail(f"{name}.svg at {size} px: {exc}")
            break
        if (pb.get_width(), pb.get_height()) != (size, size):
            fail(f"{name}.svg at {size} px renders {pb.get_width()}x{pb.get_height()}")
    else:
        print(f"  ok   {name}.svg loads at 16 to 64 px")

pngs = 0
# An installed hicolor tree holds index.theme and other apps' icons: only
# the fixed-size folders count, and only our own files in them.
for entry in sorted(os.listdir(apps)):
    found = re.fullmatch(r"(\d+)x(\d+)", entry)
    folder = os.path.join(apps, entry, "apps")
    if not found or found.group(1) != found.group(2) or not os.path.isdir(folder):
        continue
    width = int(found.group(1))
    for file in sorted(os.listdir(folder)):
        path = os.path.join(folder, file)
        if os.path.splitext(file)[0] != name:
            continue
        pngs += 1
        try:
            pb = load(path)
        except Exception as exc:
            fail(f"{entry}/apps/{file}: {exc}")
            continue
        if (pb.get_width(), pb.get_height()) != (width, width):
            fail(f"{entry}/apps/{file} is {pb.get_width()}x{pb.get_height()}, the folder says {width}")
        else:
            print(f"  ok   {entry}/apps/{file} loads, {width}x{width}")
if pngs == 0:
    fail(f"no PNG sizes under {apps}")

if bad:
    print(f"icons: {bad} problem(s); a GNOME menu would show a blank icon")
    sys.exit(1)
print(f"icons: {name} loads through gdk-pixbuf, SVG and {pngs} PNG sizes")
PY
