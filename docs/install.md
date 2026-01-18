# Install

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

One-shot bootstrap (installs deps, clones repo, runs install):

```
curl -fsSL https://raw.githubusercontent.com/daniel-g-carrasco/wayland-scroll-factor/main/scripts/bootstrap.sh | bash
```

This installs:
- `~/.local/lib/wayland-scroll-factor/libwsf_preload.so`
- `~/.local/bin/wsf`
- `~/.local/bin/wsf-gui`
- `~/.local/share/applications/io.github.danielgrasso.WaylandScrollFactor.desktop`
- icons under `~/.local/share/icons/hicolor/`

## Install (Arch package)

From this repo:

```
cd packaging/aur
makepkg -si
```

This installs system-wide under `/usr`.

## Configure and enable

```
./build/tools/wsf set 0.35
./build/tools/wsf enable
```

Then log out and log back in (or reboot).

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

## Disable

```
./build/tools/wsf disable
```

Then log out and log back in (or reboot).

## Uninstall

```
./scripts/uninstall.sh
```

Then remove user config if needed:

```
rm -f ~/.config/environment.d/wayland-scroll-factor.conf
rm -rf ~/.config/wayland-scroll-factor/
rm -rf ~/.local/lib/wayland-scroll-factor/
rm -f ~/.local/bin/wsf
```

If installed via pacman, remove the package instead:

```
sudo pacman -R wayland-scroll-factor-git
```

This project is a per-user workaround until upstream GNOME provides a scroll
speed control. It avoids `/etc/ld.so.preload` and does not modify system-wide
configuration.
