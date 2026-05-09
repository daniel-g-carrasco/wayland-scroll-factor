# Changelog

All notable changes to this project are documented in this file.

The format is inspired by Keep a Changelog and follows Semantic Versioning for tags.

## [Unreleased]

### Added
- Experimental Hyprland native scroll backend using `hyprctl keyword input:touchpad:scroll_factor`.
- Experimental `wsf-hyprland` launcher shim for Hyprland pinch zoom/rotate
  gesture tuning through `start-hyprland --path`.
- New `docs/how-it-works.md` guide describing GNOME preload, Hyprland native
  scroll, Hyprland gesture preload, safety boundaries, and verification.
- New `wsf apply` command to reapply saved settings to supported live compositor backends.
- `wsf status` and `wsf doctor` now report Hyprland backend availability, the
  live touchpad scroll factor, and gesture preload state.
- Hyprland backend documentation and troubleshooting notes.

### Changed
- The GUI reads the live Hyprland touchpad scroll factor when running under Hyprland and keeps vertical/horizontal scroll sliders synchronized there, matching Hyprland's single native scroll factor.

## [0.2.2] - 2026-04-10

### Fixed
- `libwsf_preload.so` now removes its own `LD_PRELOAD` entry from child processes after load, reducing inherited-preload failures with sandboxed apps such as snaps.
- `wsf enable` / `wsf disable` now try `systemctl --user daemon-reexec` automatically so `~/.config/environment.d` changes are more likely to be picked up without requiring a full reboot.
- `wsf doctor` and `wsf status` now report whether `systemd --user` and the running `gnome-shell` process have actually picked up WSF.

### Changed
- Runtime factor changes now reload live inside an already-active `gnome-shell`, so `wsf set ...` no longer requires logout/login once WSF is enabled and loaded.
- Project version bumped to `0.2.2`.

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
