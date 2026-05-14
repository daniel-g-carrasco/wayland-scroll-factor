# Changelog

All notable changes to this project are documented in this file.

The format is inspired by Keep a Changelog and follows Semantic Versioning for tags.

## [Unreleased]

### Added
- Hyprland: install an optional `Hyprland (WSF gestures)` Wayland session and a
  `wsf-start-hyprland` launcher so users can enable gesture preload from the
  greeter without editing login-manager configuration.
- `wsf repair` command to repair stale or missing per-user GNOME preload setup
  without hand-editing `~/.config/environment.d`.

### Fixed
- GUI: copying diagnostics now uses GTK4 clipboard content providers instead of
  the removed `Gdk.Clipboard.set_text()` API.
- GUI: avoid requesting a zero-height window minimum, which could trigger
  pixman invalid-rectangle warnings on some GTK/libadwaita stacks.
- User-manager diagnostics now read `systemctl --user show-environment`
  portably and filter it inside WSF, avoiding systems where
  `show-environment LD_PRELOAD` is not accepted.
- `wsf repair` validates the exact installed `libwsf_preload.so` path, uses the
  effective final `LD_PRELOAD=` assignment, removes stale WSF preload entries,
  and preserves unrelated preload entries.

## [0.3.0] - 2026-05-09

### Added
- Hyprland native scroll backend using `hyprctl keyword input:touchpad:scroll_factor`.
- `wsf-hyprland` launcher shim for Hyprland pinch zoom/rotate
  gesture tuning through `start-hyprland --path`.
- `wsf-session-wrapper` for greeters such as tuigreet, including
  setups where remembered sessions bypass a static `--cmd`.
- New `docs/how-it-works.md` guide describing GNOME preload, Hyprland native
  scroll, Hyprland gesture preload, safety boundaries, and verification.
- New `wsf apply` command to reapply saved settings to supported live compositor backends.
- `wsf status` and `wsf doctor` now report Hyprland backend availability, the
  live touchpad scroll factor, and gesture preload state.
- Hyprland backend documentation and troubleshooting notes.

### Changed
- The GUI reads the live Hyprland touchpad scroll factor when running under Hyprland and keeps vertical/horizontal scroll sliders synchronized there, matching Hyprland's single native scroll factor.
- GNOME preload reload behavior is more resilient on recent Mutter/GNOME builds.
- Factor parsing is locale-independent, so decimal config values such as `0.05`
  work correctly under comma-decimal locales.
- Hyprland diagnostics now report whether the gesture preload is actually mapped
  inside the running compositor process.

### Fixed
- GNOME scroll scaling no longer falls back to `1.0` under locales where
  `strtod()` expects comma decimals.
- Hyprland pinch zoom/rotate activation now has a documented session-wrapper
  path for greeters that select or remember sessions.

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
