# Install

## Arch Linux / AUR

Stable release:

```
yay -S wayland-scroll-factor
```

Latest Git build:

```
yay -S wayland-scroll-factor-git
```

Without an AUR helper:

```
git clone https://aur.archlinux.org/wayland-scroll-factor.git
cd wayland-scroll-factor
makepkg -si
```

Use `wayland-scroll-factor` for normal installs. Use
`wayland-scroll-factor-git` only if you want the latest `main` branch changes
before the next release.

## Build

```
meson setup build --prefix="$HOME/.local"
ninja -C build
```

## Install (per-user)

```
meson install -C build
```

Or use the helper script:

```
./scripts/install.sh
```

One-shot bootstrap from `main` (installs dependencies, clones the repo, runs install):

```
curl -fsSL https://raw.githubusercontent.com/daniel-g-carrasco/wayland-scroll-factor/main/scripts/bootstrap.sh | bash
```

This installs:
- `~/.local/lib/wayland-scroll-factor/libwsf_preload.so`
- `~/.local/bin/wsf`
- `~/.local/bin/wsf-gui`
- `~/.local/bin/wsf-hyprland`
- `~/.local/bin/wsf-session-wrapper`
- `~/.local/bin/wsf-start-hyprland`
- `~/.local/share/wayland-sessions/wayland-scroll-factor-hyprland.desktop`
- `~/.local/share/applications/io.github.danielgrasso.WaylandScrollFactor.desktop`
- icons under `~/.local/share/icons/hicolor/`

## Build Arch Package From This Repo

From this repo:

```
cd packaging/aur-stable
makepkg -si
```

This installs system-wide under `/usr`.

## Configure and enable

```
./build/tools/wsf set 0.35
./build/tools/wsf enable
```

(If installed, you can also run `~/.local/bin/wsf` instead of `./build/tools/wsf`.)

Then log out and log back in (or reboot).

Notes:
- `wsf enable` tries `systemctl --user daemon-reexec` automatically so the user manager reloads `~/.config/environment.d`.
- If diagnostics show a missing, commented, stale, or unloaded preload setup,
  run `wsf repair`, then log out and log back in. The repair command rewrites
  WSF's per-user `environment.d` file with the exact installed library path and
  preserves unrelated preload entries.
- Once WSF is active in `gnome-shell`, later `wsf set ...` changes are re-read
  by the preload on handled gesture events. They should not require another
  logout, but test with a new gesture after changing values.
- On Hyprland, scroll factor changes apply live through `hyprctl` when a running Hyprland session is detected. Use `wsf apply` to reapply the saved config.
- To persist Hyprland live settings across compositor restarts, keep static `touchpad.scroll_factor` commented out and add the documented `wsf apply` autostart command to your Hyprland startup config.

## Config file

Path: `~/.config/wayland-scroll-factor/config`

Supported keys:

```
factor=0.35
scroll_vertical_factor=0.35
scroll_horizontal_factor=0.35
pinch_zoom_factor=1.00
pinch_rotate_factor=1.00
```

Notes:
- `factor` is the legacy setting; it applies to scroll axes when per-axis keys
  are not set.
- Per-axis keys override `factor`.
- Pinch zoom scaling uses: `1.0 + (scale - 1.0) * pinch_zoom_factor`.

Environment overrides:

```
WSF_FACTOR=0.35
WSF_SCROLL_VERTICAL_FACTOR=0.35
WSF_SCROLL_HORIZONTAL_FACTOR=0.35
WSF_PINCH_ZOOM_FACTOR=1.00
WSF_PINCH_ROTATE_FACTOR=1.00
WSF_LIB_PATH=/custom/path/libwsf_preload.so
WSF_DEBUG=1
```

## Hyprland

WSF uses Hyprland's native `input:touchpad:scroll_factor` setting for live
touchpad scroll tuning:

```
wsf set 0.35
wsf apply
wsf doctor
```

Hyprland currently exposes one touchpad scroll factor for both vertical and
horizontal axes.

For pinch zoom/rotate, launch Hyprland through WSF's shim while keeping
`start-hyprland` in the launch chain:

```
start-hyprland --path "$(command -v wsf-hyprland)" -- --config ~/.config/hypr/hyprland.conf
```

Package installs also add a separate Wayland session:

```
Hyprland (WSF gestures)
```

Select that session in the greeter to start Hyprland with gesture tuning,
without editing login-manager configuration.

The shim only preloads WSF into the compositor process for gesture hooks. Check
activation with:

```
wsf doctor
```

Look for `hyprland gesture preload: active`.

The installer provides `wsf-hyprland`, but it does not edit greetd, SDDM, or
custom session files. Wire the command above into the session launcher used by
your desktop.

For greetd/tuigreet setups that select sessions from the menu, use the installed
session wrapper when WSF is installed system-wide:

```
tuigreet ... --session-wrapper /usr/bin/wsf-session-wrapper
```

This is especially important when `tuigreet --remember-session` is enabled:
tuigreet documents that remembered sessions override `--cmd` on later logins.
`wsf-session-wrapper` leaves non-Hyprland sessions unchanged and injects
`wsf-hyprland` only when the selected command is `Hyprland` or
`start-hyprland`.

Do not hardcode `~/.local/bin/wsf-session-wrapper` in `/etc/greetd/config.toml`.
If that per-user install is removed before the greetd config is reverted, login
can fail because greetd points at a missing wrapper.

For a safer reversible setup, create a wrapper that is independent from WSF:

```
sudo tee /usr/local/bin/wsf-session-wrapper-fallback >/dev/null <<'EOF'
#!/bin/sh
if command -v wsf-session-wrapper >/dev/null 2>&1; then
  exec wsf-session-wrapper "$@"
fi
exec "$@"
EOF
sudo chmod 755 /usr/local/bin/wsf-session-wrapper-fallback
```

Then configure tuigreet with:

```
tuigreet ... --session-wrapper /usr/local/bin/wsf-session-wrapper-fallback
```

If WSF is uninstalled later, the fallback wrapper still exists and launches the
selected session unchanged.

For Hyprland-based OS images, install WSF with the desktop package set and add:

```
exec-once = sh -lc 'if command -v wsf >/dev/null 2>&1; then wsf apply; elif [ -x "$HOME/.local/bin/wsf" ]; then "$HOME/.local/bin/wsf" apply; fi'
```

Keep any static Hyprland `scroll_factor = ...` line commented out so reloads do
not overwrite the WSF value.

## Disable

```
./build/tools/wsf disable
```

Then log out and log back in (or reboot).

`wsf disable` also tries `systemctl --user daemon-reexec` automatically.

## Uninstall

```
./scripts/uninstall.sh
```

Then remove user config if needed:

```
rm -f ~/.config/environment.d/wayland-scroll-factor.conf
rm -rf ~/.config/wayland-scroll-factor/
rm -rf ~/.local/lib/wayland-scroll-factor/
rm -f ~/.local/bin/wsf ~/.local/bin/wsf-gui ~/.local/bin/wsf-hyprland ~/.local/bin/wsf-session-wrapper ~/.local/bin/wsf-start-hyprland
rm -f ~/.local/share/wayland-sessions/wayland-scroll-factor-hyprland.desktop
```

If installed via pacman, remove the package instead:

```
sudo pacman -R wayland-scroll-factor-git
```

This project is a per-user workaround until upstream GNOME provides a scroll
speed control. It avoids `/etc/ld.so.preload` and does not modify system-wide
configuration.

WSF also strips its own `LD_PRELOAD` entry from child processes after load to
reduce inherited-preload issues with sandboxed apps such as snaps.
