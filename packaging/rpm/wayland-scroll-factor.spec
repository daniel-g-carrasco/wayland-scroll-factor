Name:           wayland-scroll-factor
Version:        1.0.4
Release:        1%{?dist}
Summary:        Touchpad scroll and gesture tuning for Wayland

License:        MIT
URL:            https://github.com/daniel-g-carrasco/wayland-scroll-factor
Source0:        %{url}/archive/refs/tags/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  meson
BuildRequires:  ninja-build
BuildRequires:  python3
BuildRequires:  desktop-file-utils
BuildRequires:  appstream

Requires:       python3
Requires:       python3-gobject
Requires:       gtk4
Requires:       libadwaita

Recommends:     libinput-utils

%description
Wayland Scroll Factor provides touchpad scroll and gesture tuning for Wayland
desktops. It supports GNOME Wayland through a guarded per-user preload backend.

%prep
%autosetup -n %{name}-%{version}

%build
%meson
%meson_build

%install
%meson_install

install -Dm0644 LICENSE %{buildroot}%{_licensedir}/%{name}/LICENSE

%check
python3 -m py_compile gui/wsf_gui.py
desktop-file-validate %{_vpath_builddir}/data/io.github.danielgrasso.WaylandScrollFactor.desktop
appstreamcli validate --no-net data/io.github.danielgrasso.WaylandScrollFactor.metainfo.xml

%files
%license %{_licensedir}/%{name}/LICENSE
%doc README.md CHANGELOG.md docs
%{_bindir}/wsf
%{_bindir}/wsf-gui
%{_libdir}/wayland-scroll-factor/
%{_datadir}/applications/io.github.danielgrasso.WaylandScrollFactor.desktop
%{_datadir}/metainfo/io.github.danielgrasso.WaylandScrollFactor.metainfo.xml
%{_datadir}/icons/hicolor/scalable/apps/io.github.danielgrasso.WaylandScrollFactor.svg
%{_datadir}/icons/hicolor/128x128/apps/io.github.danielgrasso.WaylandScrollFactor.png
%{_datadir}/icons/hicolor/256x256/apps/io.github.danielgrasso.WaylandScrollFactor.png
%{_datadir}/icons/hicolor/512x512/apps/io.github.danielgrasso.WaylandScrollFactor.png

%changelog
* Sat Nov 07 2026 Daniel Grasso <daniel@the-empty.place> - 1.0.4-1
- Icon: the SVG app icon opened with a comment before the `<svg>`
  tag, and gdk-pixbuf did not recognise it as an image, so GNOME
  menus showed a blank icon (Ubuntu 26.04, Yaru). The comment now
  sits inside the `<svg>` element; the artwork is unchanged. (#45,
  reported by @dradoaica)
- `scripts/check-icons.sh` loads the app icon the way GNOME does,
  through gdk-pixbuf: the SVG at menu sizes, every PNG at the size
  its folder says, all under the name the desktop file asks for. CI
  runs it on every push, and the distro smoke tests run it on the
  installed icons with each distribution's own gdk-pixbuf, so an icon
  that a menu cannot show never reaches a release again.
- The Arch smoke lane upgrades the container in full (`pacman -Syu`):
  a plain `-Sy` left Python 3.14.9 next to the image's older glibc,
  and meson could not start.
* Tue Nov 03 2026 Daniel Grasso <daniel@the-empty.place> - 1.0.3-1
- `scripts/bump-version.sh` writes a new version into the six files
  that carry one, with the Debian, rpm and AppStream entries generated
  from the CHANGELOG; `scripts/check-version.sh` reads them back,
  in CI on every push and before every publish on a tag.
- Preload: a factor change is applied from the next scroll or pinch
  event instead of up to 250 ms later. The config directory is watched
  with a non-blocking inotify fd; the 250 ms `stat()` stays as the
  fallback for network homes and for a config directory created after
  login. (#42, by @ritesh-777)
* Fri Jul 31 2026 Daniel Grasso <daniel@the-empty.place> - 1.0.0-1
- Hyprland support is now an opt-in meson feature (-Dhyprland=enabled),
  disabled and unpackaged by default.

* Sat Jun 13 2026 Daniel Grasso <daniel@the-empty.place> - 0.3.5-1
- Show the version everywhere: `wsf version` / `wsf --version`, a
  `wsf_version`/`wsf-version` field in `status` and `doctor` (text and
  --json), and a GTK About dialog plus a version subtitle in the GUI.

* Sat Jun 13 2026 Daniel Grasso <daniel@the-empty.place> - 0.3.4-1
- Preload hardening: thread-safe init (pthread_once) and atomic runtime
  factors; stop re-reading/parsing the config on every 250 ms reload tick
  during scrolling (only re-parse when the file actually changed).
- Bounded config parsing (256-byte lines, 64 KiB file ceiling).
- Atomic environment.d writes; `wsf enable` preserves foreign LD_PRELOAD;
  WSF_LIB_PATH is validated before use.
- Build hardening: PIE, stack-protector-strong, FORTIFY_SOURCE=2, RELRO,
  hidden symbol visibility (only the 6 interposed libinput wrappers
  exported).

* Thu Jun 11 2026 Daniel Grasso <daniel@the-empty.place> - 0.3.3-1
- Add GitHub Release asset workflow and package publication documentation.
- Add RPM and Debian package build test automation.

* Thu Jun 11 2026 Daniel Grasso <daniel@the-empty.place> - 0.3.2-1
- Initial RPM packaging template.
