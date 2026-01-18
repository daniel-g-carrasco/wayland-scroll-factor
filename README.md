# Wayland Scroll Factor (WSF)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

<p align="center">
  <img src="data/icons/hicolor/512x512/apps/io.github.danielgrasso.WaylandScrollFactor.png"
       alt="WSF icon" width="180" height="180">
</p>

<p align="center">
  <b>Tune touchpad gesture feel on Wayland</b><br>
  Predictable two‑finger scrolling (vertical + horizontal) and pinch zoom/rotate sensitivity on modern Linux desktops — starting with GNOME on Wayland.<br>
  <i>Status: testing</i>
</p>

---

## What is WSF?

**WSF (Wayland Scroll Factor)** is a small utility that helps you adjust the *feel* of common touchpad gestures on Wayland:

- **Two‑finger scroll** (vertical and horizontal)
- **Pinch‑to‑zoom** sensitivity
- **Pinch rotate** sensitivity

WSF is intentionally narrow in scope and designed to be **safe, reversible, and practical** on modern GNOME Wayland setups (tested primarily on Arch + GNOME).

---

## Why does this exist?

On Wayland, touchpad gesture behavior is typically handled by the compositor (e.g. GNOME Shell/Mutter). Many users run into one or more of these issues depending on hardware and distro defaults:

- Scroll feels **too fast/too slow** and there is **no simple system slider**.
- Horizontal scroll behavior is inconsistent across apps.
- Pinch‑to‑zoom in maps/photos can feel **hard to control**.
- Older hacks/workarounds can become **fragile** across GNOME/libinput updates.

WSF exists to provide a **user‑level**, easy‑to‑roll‑back approach until upstream environments expose consistent, user‑facing controls.

---

## How it works (high level)

WSF ships two components:

1) **CLI (`wsf`)**  
   Reads/writes config and controls enable/disable and diagnostics.

2) **User‑level preload library (`libwsf_preload.so`)**  
   Interposes a small set of libinput functions used for scrolling and gestures and applies configurable scaling factors.

### Safety design choices

- **Per‑user only**: avoids `/etc/ld.so.preload`. Config is under `~/.config`.
- **Process guard rail**: the preload library is a no‑op unless the process is `gnome-shell` (so unrelated apps are not affected).
- **Touchpad‑only scroll scaling**: scroll scaling is applied only to finger/continuous sources, preserving mouse wheel behavior.
- **Narrow scope**: focuses on scroll + pinch sensitivity to reduce breakage across updates.
- **Diagnostics first**: `wsf doctor` reports symbol availability, active factors, and environment status.

> Enabling/disabling requires **logout/login** (or session restart) because environment changes must be picked up by GNOME Shell.

---

## Quick install (one‑shot)

This script attempts to:
1) install dependencies via your package manager,
2) clone the repo,
3) run the user install.

It detects your distro (Arch, Ubuntu/Debian, Fedora) and runs the appropriate package manager commands. If dependency install fails, you can rerun the script after installing packages manually.

```bash
curl -fsSL https://raw.githubusercontent.com/daniel-g-carrasco/wayland-scroll-factor/main/scripts/bootstrap.sh | bash
```

**Notes**
- Requires `sudo` only for dependency installation.
- GUI requires **libadwaita ≥ 1.4** (Ubuntu 22.04 and Debian 12 will install, but the GUI won’t run; the CLI still works).
- As with any `curl | bash` install: feel free to inspect the script first.

---

## Quick start

Set a factor (example: slightly slower scroll):

```bash
wsf set 0.35
```

Enable for your session (**logout/login required**):

```bash
wsf enable
```

Run diagnostics:

```bash
wsf doctor
```

Disable (rollback):

```bash
wsf disable
```

---

## CLI usage

### Build

```bash
meson setup build --prefix="$HOME/.local"
ninja -C build
```

### Install

```bash
meson install -C build
# or
./scripts/install.sh
```

### Common commands

- `wsf get` (or `wsf get --json`)
- `wsf set <factor>` (and/or per‑key factors if supported)
- `wsf enable` / `wsf disable` (**logout/login required**)
- `wsf status`
- `wsf doctor`

---

## GUI (GNOME / libadwaita)

WSF includes a GNOME‑style **GTK4/libadwaita** control app that uses the `wsf` CLI under the hood.

- Run: `wsf-gui`
- Reads values via `wsf get --json`
- Applies changes via `wsf set`

<p align="center">
  <img src="docs/screenshots/gui.png" alt="WSF GUI screenshot" width="860">
</p>

---

## Configuration

WSF stores configuration under:

- `~/.config/wayland-scroll-factor/config`

Typical keys (depending on your version):

- `factor=...` (legacy / shared)
- `scroll_vertical_factor=...`
- `scroll_horizontal_factor=...`
- `pinch_zoom_factor=...`
- `pinch_rotate_factor=...`

You can also override values temporarily using environment variables (see `wsf --help` / docs).

---

## Uninstall / rollback

WSF is designed to be easy to remove.

Disable:

```bash
wsf disable
```

Remove config:

```bash
rm -rf ~/.config/wayland-scroll-factor
rm -f  ~/.config/environment.d/wayland-scroll-factor.conf
```

Remove installed files (user install):

```bash
rm -f  ~/.local/bin/wsf ~/.local/bin/wsf-gui
rm -rf ~/.local/lib/wayland-scroll-factor
```

After disabling/removal: **logout/login**.

---

## Packages

### Arch (AUR-style PKGBUILD)

```bash
cd packaging/aur
makepkg -si
```

This installs system‑wide under `/usr`. For custom library locations, set `WSF_LIB_PATH` before running `wsf enable`.

---

## Compatibility

- **Core (preload/CLI)** requires **libinput ≥ 1.19**
- **GUI** requires **libadwaita ≥ 1.4** (`Adw.ToolbarView` introduced in 1.4)

### Known working

- **Arch Linux (rolling)** + GNOME (Wayland) — primary test target
- **Ubuntu 24.04 LTS** — CLI + GUI compatible
- **Fedora (recent)** — CLI + GUI compatible

### CLI only (GUI too old)

- Ubuntu 22.04 LTS
- Debian 12

---

## Limitations

- Environment changes typically require **logout/login** to affect GNOME Shell.
- WSF intentionally adjusts only a small subset of gesture feel controls.
- Support is focused on GNOME first; broader compositor support is planned.

---

## Contributing

Issues and PRs are welcome. When reporting a problem, please include:

- distro + compositor + session type (Wayland/X11)
- GNOME Shell version (if applicable)
- libinput version
- what you expected vs what happened
- output of `wsf doctor`

---

## License

MIT License — see [`LICENSE`](LICENSE).

---

## Acknowledgements

WSF was inspired by the idea behind [`libinput-config`](https://github.com/lz42/libinput-config).
