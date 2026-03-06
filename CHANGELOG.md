# Changelog

All notable changes to this project are documented in this file.

The format is inspired by Keep a Changelog and follows Semantic Versioning for tags.

## [0.2.1] - 2026-03-06

### Fixed
- `bootstrap.sh`: Fedora/DNF dependency handling now tries `libinput-utils` first, with fallback to `libinput-tools`.
- `install.sh`: Meson setup now passes the source directory explicitly, fixing one-shot installs launched outside the repo root.
- GUI startup crash fixed on environments where `Gtk.Widget.set_accessible_name()` is not available in Python bindings.

### Changed
- Project version bumped to `0.2.1`.

## [0.2.0] - 2026-01-18

### Added
- Initial public testing release.
- User-level preload library and CLI (`wsf`) for scroll/pinch tuning on GNOME Wayland.
- Optional GNOME/libadwaita GUI (`wsf-gui`) with diagnostics view.
- One-shot bootstrap installer and user install/uninstall scripts.
