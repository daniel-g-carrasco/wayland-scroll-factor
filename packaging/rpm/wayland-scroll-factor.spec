Name:           wayland-scroll-factor
Version:        0.3.2
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
Suggests:       hyprland

%description
Wayland Scroll Factor provides touchpad scroll and gesture tuning for Wayland
desktops. It supports GNOME Wayland through a guarded per-user preload backend
and Hyprland through native runtime scroll settings plus an optional gesture
launcher.

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
%{_bindir}/wsf-hyprland
%{_bindir}/wsf-session-wrapper
%{_bindir}/wsf-start-hyprland
%{_libdir}/wayland-scroll-factor/
%{_datadir}/applications/io.github.danielgrasso.WaylandScrollFactor.desktop
%{_datadir}/metainfo/io.github.danielgrasso.WaylandScrollFactor.metainfo.xml
%{_datadir}/icons/hicolor/128x128/apps/io.github.danielgrasso.WaylandScrollFactor.png
%{_datadir}/icons/hicolor/256x256/apps/io.github.danielgrasso.WaylandScrollFactor.png
%{_datadir}/icons/hicolor/512x512/apps/io.github.danielgrasso.WaylandScrollFactor.png
%{_datadir}/wayland-sessions/wayland-scroll-factor-hyprland.desktop
%{_datadir}/wayland-scroll-factor/hyprland/wsf.lua

%changelog
* Thu Jun 11 2026 Daniel Grasso <daniel@the-empty.place> - 0.3.2-1
- Initial RPM packaging template.
