# Dependencies

This page lists WSF dependencies explicitly. In this document, `wsf` means the
command installed by this project. It is not a separate dependency to install
from your distribution unless your distribution packages Wayland Scroll Factor
itself.

## Runtime model

WSF is split into small pieces:

- `wsf`: command-line tool and diagnostics.
- `libwsf_preload.so`: optional preload library used by the GNOME backend and
  Hyprland pinch gesture backend.
- `wsf-gui`: optional GTK/libadwaita controller that calls the `wsf` command.
- `wsf-hyprland`, `wsf-session-wrapper`, `wsf-start-hyprland`: optional
  Hyprland launch helpers.

The core CLI and preload library are built in C and depend only on the system C
library and dynamic loader APIs. WSF does not link against libinput at build
time; the preload library resolves the libinput symbols it needs at runtime.

## Required For Building

| Dependency | Why WSF needs it | Upstream |
| --- | --- | --- |
| C compiler (`gcc` or compatible) | Builds `wsf` and `libwsf_preload.so` | <https://gcc.gnu.org/> |
| Meson | Configures the build | <https://mesonbuild.com/> |
| Ninja | Runs the Meson-generated build | <https://ninja-build.org/> |
| pkg-config/pkgconf | Standard build tooling used by distro build environments | <https://github.com/pkgconf/pkgconf> |
| Git | Cloning the repository and git-based installs | <https://git-scm.com/> |

## Required For The GUI

The GUI is optional. If you only use the CLI, these are not needed.

| Dependency | Why WSF needs it | Upstream |
| --- | --- | --- |
| Python 3 | Runs `wsf-gui` | <https://www.python.org/> |
| PyGObject | Python bindings for GTK/GObject | <https://pygobject.gnome.org/> |
| GTK 4 | GUI toolkit | <https://www.gtk.org/> |
| libadwaita | GNOME/Adwaita widgets and application style | <https://gnome.pages.gitlab.gnome.org/libadwaita/> |

## Runtime/Diagnostics Dependencies

| Dependency | Required? | Why WSF needs it | Upstream |
| --- | --- | --- | --- |
| GNOME Shell | GNOME backend only | The preload backend targets `gnome-shell` on Wayland | <https://gitlab.gnome.org/GNOME/gnome-shell> |
| Hyprland | Hyprland backend only | Native scroll backend and optional gesture preload launcher | <https://hypr.land/> |
| libinput | Normally already part of the desktop stack | WSF wraps selected libinput functions at runtime inside the compositor | <https://wayland.freedesktop.org/libinput/doc/latest/> |
| `libinput` CLI tool | Optional | `wsf doctor` uses it to print the libinput version when available | <https://wayland.freedesktop.org/libinput/doc/latest/tools.html> |
| systemd user manager | GNOME backend | `wsf enable`, `wsf disable`, and `wsf repair` use per-user `environment.d` | <https://www.freedesktop.org/software/systemd/man/latest/environment.d.html> |

## Package Names

These commands install the build tools and optional GUI/diagnostic dependencies
for a source install. Package names can vary by release; if a command fails,
use your distribution's package search for the dependency name in the tables
above.

### Arch Linux

```bash
sudo pacman -S --needed base-devel meson ninja pkgconf git \
  python python-gobject gtk4 libadwaita libinput-tools
```

Arch users can usually install the packaged version instead:

```bash
yay -S wayland-scroll-factor
```

### Debian / Ubuntu

```bash
sudo apt update
sudo apt install build-essential meson ninja-build pkg-config git \
  python3 python3-gi gir1.2-adw-1 libgtk-4-1 libadwaita-1-0 libinput-tools
```

### Fedora

```bash
sudo dnf install gcc gcc-c++ make meson ninja-build pkgconf-pkg-config git \
  python3 python3-gobject gtk4 libadwaita libinput-utils
```

Some Fedora-derived systems package the libinput diagnostic CLI as
`libinput-tools` instead of `libinput-utils`.

### openSUSE

```bash
sudo zypper install gcc gcc-c++ make meson ninja pkg-config git \
  python3 python3-gobject gtk4 libadwaita-1-0 libinput-tools
```

### Gentoo

```bash
sudo emerge --ask dev-build/meson dev-build/ninja virtual/pkgconfig dev-vcs/git \
  sys-devel/gcc dev-lang/python dev-python/pygobject gui-libs/gtk \
  gui-libs/libadwaita dev-libs/libinput
```

Gentoo package references:

- `dev-libs/libinput`: <https://packages.gentoo.org/packages/dev-libs/libinput>
- `dev-python/pygobject`: <https://packages.gentoo.org/packages/dev-python/pygobject>
- `gui-libs/gtk`: <https://packages.gentoo.org/packages/gui-libs/gtk>
- `gui-libs/libadwaita`: <https://packages.gentoo.org/packages/gui-libs/libadwaita>
- `dev-build/meson`: <https://packages.gentoo.org/packages/dev-build/meson>
- `dev-build/ninja`: <https://packages.gentoo.org/packages/dev-build/ninja>
- `virtual/pkgconfig`: <https://packages.gentoo.org/packages/virtual/pkgconfig>

WSF does not currently provide an official Gentoo ebuild. The source install
should work if the dependencies above are present, but Gentoo-specific packaging
is community territory for now.
