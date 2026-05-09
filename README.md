# Wayland Scroll Factor (WSF)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

<p align="center">
  <img src="data/icons/hicolor/512x512/apps/io.github.danielgrasso.WaylandScrollFactor.png"
       alt="WSF icon" width="160" height="160">
</p>

<p align="center">
  <b>Tune touchpad scroll and gesture feel on Wayland</b><br>
  GNOME Wayland preload backend, plus experimental Hyprland scroll and gesture support.<br>
  <i>Status: testing</i>
</p>

---

## What It Does

WSF adjusts touchpad sensitivity when the compositor does not expose enough user-facing controls.

- Two-finger scroll factor.
- Pinch zoom factor.
- Pinch rotate factor.
- GNOME Wayland support through a guarded per-user preload library.
- Hyprland scroll support through native `hyprctl`.
- Experimental Hyprland pinch zoom/rotate support through a `start-hyprland`
  compatible launcher shim.

WSF is per-user and reversible. It does **not** use `/etc/ld.so.preload`.

---

## Install

Testing branch install:

```bash
curl -fsSL https://raw.githubusercontent.com/daniel-g-carrasco/wayland-scroll-factor/feature/hyprland-backend/scripts/bootstrap.sh | WSF_REF=feature/hyprland-backend bash
```

Manual build:

```bash
git clone https://github.com/daniel-g-carrasco/wayland-scroll-factor.git
cd wayland-scroll-factor
git checkout feature/hyprland-backend
meson setup build --prefix="$HOME/.local"
ninja -C build
meson install -C build
```

The installer places files under `~/.local`.

It installs `wsf`, `wsf-gui`, `wsf-hyprland`, and
`libwsf_preload.so`. It does not automatically modify your login manager.

---

## Quick Start

Set a factor:

```bash
wsf set 0.35
```

Check status:

```bash
wsf status
wsf doctor
```

Run the GUI:

```bash
wsf-gui
```

<p align="center">
  <img src="docs/screenshots/gui.png" alt="WSF GUI screenshot" width="760">
</p>

---

## GNOME Wayland

GNOME uses the preload backend.

```bash
wsf set 0.35
wsf enable
```

Then log out and log back in.

Disable:

```bash
wsf disable
```

Then log out and log back in.

Once the preload is already loaded in `gnome-shell`, later `wsf set ...` changes should apply live.

---

## Hyprland

Hyprland uses the native backend.

WSF applies:

```bash
hyprctl keyword input:touchpad:scroll_factor <factor>
```

So scroll changes apply immediately:

```bash
wsf set 0.35
hyprctl getoption input:touchpad:scroll_factor
```

No logout/login is required for Hyprland scroll changes.

Hyprland currently exposes one native touchpad scroll factor shared by vertical and horizontal scrolling. For that reason, WSF keeps the vertical and horizontal scroll controls synchronized on Hyprland.

Hyprland does not currently expose native client pinch zoom/rotate sensitivity
settings. WSF can tune those controls by starting Hyprland through the installed
`wsf-hyprland` shim.

```bash
start-hyprland --path "$(command -v wsf-hyprland)" -- --config ~/.config/hypr/hyprland.conf
```

This keeps Hyprland inside its recommended `start-hyprland` launch path while
letting WSF preload only the compositor process for gesture hooks. Once
`wsf doctor` reports `hyprland gesture preload: active`, pinch factor changes
are picked up live from the WSF config.

WSF does not modify login manager configuration automatically. If you use a
display manager or a custom session script, point that session command at the
`start-hyprland --path ...` form above.

### Hyprland Persistence

Hyprland runtime settings can be overwritten by static config during reload/login. If WSF should manage scroll speed, remove or comment out any static `touchpad.scroll_factor` / `input:touchpad:scroll_factor` value from your Hyprland config, or keep it intentionally in sync with WSF.

Add a startup command in your Hyprland autostart config:

```ini
exec-once = sh -lc 'if command -v wsf >/dev/null 2>&1; then wsf apply; elif [ -x "$HOME/.local/bin/wsf" ]; then "$HOME/.local/bin/wsf" apply; fi'
```

This reapplies the saved WSF value when Hyprland starts.

More details: [`docs/hyprland.md`](docs/hyprland.md)

Architecture details: [`docs/how-it-works.md`](docs/how-it-works.md)

---

## Commands

```bash
wsf get
wsf get --json
wsf set <factor>
wsf set --scroll-vertical <factor>
wsf set --scroll-horizontal <factor>
wsf set --pinch-zoom <factor>
wsf set --pinch-rotate <factor>
wsf apply
wsf enable
wsf disable
wsf status
wsf status --json
wsf doctor
wsf doctor --json
wsf-hyprland
```

Config file:

```text
~/.config/wayland-scroll-factor/config
```

Example:

```ini
factor=0.35
scroll_vertical_factor=0.35
scroll_horizontal_factor=0.35
pinch_zoom_factor=1.00
pinch_rotate_factor=1.00
```

---

## Uninstall

```bash
wsf disable
rm -rf ~/.config/wayland-scroll-factor
rm -f ~/.config/environment.d/wayland-scroll-factor.conf
rm -f ~/.local/bin/wsf ~/.local/bin/wsf-gui ~/.local/bin/wsf-hyprland
rm -rf ~/.local/lib/wayland-scroll-factor
rm -f ~/.local/share/applications/io.github.danielgrasso.WaylandScrollFactor.desktop
rm -f ~/.local/share/metainfo/io.github.danielgrasso.WaylandScrollFactor.metainfo.xml
```

After disabling GNOME preload, log out and log back in.

---

## Compatibility

Known working:

- Arch Linux rolling + GNOME Wayland.
- Arch Linux rolling + Hyprland native scroll backend.
- Ubuntu 24.04 LTS.
- Recent Fedora.

Requirements:

- `meson`, `ninja`, C compiler for building from source.
- `libinput`.
- Python 3, GTK4, libadwaita for the GUI.
- `hyprctl` for Hyprland native scroll support.

---

## Limitations

- GNOME enable/disable requires logout/login because `gnome-shell` must load or unload the preload library.
- Hyprland has one native touchpad scroll factor, not independent vertical/horizontal factors.
- Hyprland pinch zoom/rotate support is experimental and requires launching
  Hyprland with the targeted gestures-only preload.
- WSF is a workaround until compositors expose better upstream controls.

---

## Docs

- [`docs/install.md`](docs/install.md)
- [`docs/how-it-works.md`](docs/how-it-works.md)
- [`docs/troubleshooting.md`](docs/troubleshooting.md)
- [`docs/hyprland.md`](docs/hyprland.md)
- [`docs/design.md`](docs/design.md)
- [`CHANGELOG.md`](CHANGELOG.md)

---

## Acknowledgements

WSF was inspired by the idea behind [`libinput-config`](https://github.com/lz42/libinput-config), but uses a narrower and safer architecture: no global preload, per-user rollback, GNOME process guard, Hyprland native backend, and built-in diagnostics.

## License

MIT. See [`LICENSE`](LICENSE).
