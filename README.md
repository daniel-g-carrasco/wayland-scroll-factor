# Wayland Scroll Factor

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

<p align="center">
  <img src="data/icons/hicolor/512x512/apps/io.github.danielgrasso.WaylandScrollFactor.png"
       alt="Wayland Scroll Factor icon" width="150" height="150">
</p>

<p align="center">
  <b>Tune touchpad scroll and gesture sensitivity on Wayland.</b><br>
  GNOME Wayland support, plus Hyprland scroll and gesture support.<br>
  <i>Current release: 0.3.3. Hyprland support is usable, but still newer than the GNOME backend.</i>
</p>

---

## What WSF Does

Wayland Scroll Factor, or WSF, adjusts touchpad gesture sensitivity when the
desktop does not expose enough controls.

It can tune:

- two-finger vertical and horizontal scroll;
- pinch zoom;
- pinch rotate.

WSF is designed to be reversible. It does not use `/etc/ld.so.preload`, and it
does not patch GNOME, Hyprland, libinput, or your kernel.

## Desktop Support

| Desktop | Status | Notes |
| --- | --- | --- |
| GNOME Wayland | Supported | Uses a guarded preload backend loaded into `gnome-shell`. |
| Hyprland | Supported | Uses Hyprland's native scroll setting and an optional gesture preload session. |
| Hyprland Lua config | Supported | WSF uses `hyprctl eval` when `hyprctl keyword` is not accepted. |
| KDE Plasma / KWin | Not yet | Planned, but not implemented. |
| wlroots compositors | Not yet | Planned, but compositor-specific behavior still needs work. |
| X11 | Not a target | WSF is designed for Wayland compositors. |

## Install

### Arch Linux / AUR

Stable release:

```bash
yay -S wayland-scroll-factor
```

Latest Git build:

```bash
yay -S wayland-scroll-factor-git
```

Use `wayland-scroll-factor` for normal installs. Use
`wayland-scroll-factor-git` only when you want the latest `main` branch before
the next release.

### Fedora Workstation / Fedora Spins

WSF is available from COPR:

```bash
sudo dnf install dnf-plugins-core
sudo dnf copr enable daniel-g-carrasco/wayland-scroll-factor
sudo dnf install wayland-scroll-factor
```

### Fedora Atomic Desktops

For Silverblue, Kinoite, Sway Atomic, Budgie Atomic, bootc-based images, or
derived systems, add the COPR repo file and layer the package:

```bash
fedora_version="$(rpm -E %fedora)"

sudo curl -fsSL \
  -o /etc/yum.repos.d/daniel-g-carrasco-wayland-scroll-factor.repo \
  "https://copr.fedorainfracloud.org/coprs/daniel-g-carrasco/wayland-scroll-factor/repo/fedora-${fedora_version}/daniel-g-carrasco-wayland-scroll-factor-fedora-${fedora_version}.repo"

sudo rpm-ostree refresh-md
sudo rpm-ostree install wayland-scroll-factor
systemctl reboot
```

Atomic users who build their own image can instead include the same COPR repo
and package in the image recipe.

### Debian / Ubuntu

There is not an official apt repository yet. For now, install the `.deb` from
the latest GitHub Release:

```bash
version="0.3.3"
curl -LO "https://github.com/daniel-g-carrasco/wayland-scroll-factor/releases/download/v${version}/wayland-scroll-factor_${version}-1_amd64.deb"
sudo apt install "./wayland-scroll-factor_${version}-1_amd64.deb"
```

This currently targets `amd64`.

### NixOS via Flake

The project is available via a Nix flake from this repository. Add it to your flake.nix file like so

```nix

{
  description = "WSF Flake";
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
    wsf = {
      url = "github:daniel-g-carrasco/wayland-scroll-factor";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };
  outputs = { nixpkgs, wsf, ... }: {
    nixosConfigurations.nixos = nixpkgs.lib.nixosSystem {
      system = "x86_64-linux";
      modules = [
        wsf.nixosModules.default
        { programs.wsf.enable = true; }
        ./configuration.nix # The rest of your configuration
      ];
    };
  };
}

```


### openSUSE, Gentoo, And Other Distros

Use the source install for now. Package names for build dependencies are listed
in [docs/dependencies.md](docs/dependencies.md).

One-shot source install:

```bash
curl -fsSL https://raw.githubusercontent.com/daniel-g-carrasco/wayland-scroll-factor/main/scripts/bootstrap.sh | bash
```

Manual source install:

```bash
git clone https://github.com/daniel-g-carrasco/wayland-scroll-factor.git
cd wayland-scroll-factor
meson setup build --prefix="$HOME/.local"
meson compile -C build
meson install -C build
```

The source installer writes under `~/.local`. It does not edit system login
manager configuration.

## Basic Use

Set the main scroll factor:

```bash
wsf set 0.35
```

Set individual factors:

```bash
wsf set \
  --scroll-vertical 0.35 \
  --scroll-horizontal 0.35 \
  --pinch-zoom 1.00 \
  --pinch-rotate 1.00
```

Inspect the current configuration:

```bash
wsf get
wsf status
wsf doctor
```

Open the GUI:

```bash
wsf-gui
```

<p align="center">
  <img src="docs/screenshots/gui.png" alt="WSF GUI screenshot" width="760">
</p>

## GNOME Wayland

GNOME support uses a per-user preload backend.

Enable it:

```bash
wsf set 0.35
wsf enable
```

Then log out and log back in.

After WSF is loaded into `gnome-shell`, later factor changes are re-read during
new handled gesture events. Changing factors should not require another logout,
but enabling or disabling the preload still does.

If diagnostics show a stale or missing preload setup:

```bash
wsf repair
```

Disable it:

```bash
wsf disable
```

Then log out and log back in.

## Hyprland

Hyprland scroll tuning is live:

```bash
wsf set 0.35
wsf apply
```

Hyprland currently exposes one native touchpad scroll factor shared by vertical
and horizontal scrolling. For that reason, WSF keeps those two scroll values in
sync when running under Hyprland.

For pinch zoom and pinch rotate, launch the dedicated session installed by WSF:

```text
Hyprland (WSF gestures)
```

Select it from your greeter. This starts Hyprland through WSF's
`start-hyprland`-compatible launcher without rewriting `greetd`, SDDM, GDM, or
other login-manager configuration.

Check activation:

```bash
wsf doctor
```

Look for:

```text
hyprland gesture preload: active
```

Hyprland reloads can overwrite runtime scroll settings with values from the
static config. If WSF should manage scroll speed, remove or comment out static
`scroll_factor` entries from your Hyprland config, or intentionally keep them
in sync with WSF.

For Hyprland 0.55+ Lua configs, WSF packages install a generic helper:

```lua
dofile("/usr/share/wayland-scroll-factor/hyprland/wsf.lua")
```

For per-user source installs:

```lua
dofile(os.getenv("HOME") .. "/.local/share/wayland-scroll-factor/hyprland/wsf.lua")
```

The helper reapplies saved WSF scroll settings after Lua config reloads.

More details are in [docs/hyprland.md](docs/hyprland.md).

## Configuration

WSF stores user settings here:

```text
~/.config/wayland-scroll-factor/config
```

Supported keys:

```ini
scroll_vertical_factor=0.35
scroll_horizontal_factor=0.35
pinch_zoom_factor=1.00
pinch_rotate_factor=1.00
```

The legacy `factor=...` key is still accepted for scroll settings.

## Uninstall

Package installs should be removed with the package manager used to install
them.

Arch:

```bash
yay -Rns wayland-scroll-factor
```

Fedora:

```bash
sudo dnf remove wayland-scroll-factor
```

Fedora Atomic:

```bash
sudo rpm-ostree uninstall wayland-scroll-factor
systemctl reboot
```

Source install:

```bash
~/wayland-scroll-factor/scripts/uninstall.sh
```

Per-user state can also be removed manually:

```bash
rm -f ~/.config/environment.d/wayland-scroll-factor.conf
rm -rf ~/.config/wayland-scroll-factor
```

## Documentation

- [Install details](docs/install.md)
- [Dependencies](docs/dependencies.md)
- [Hyprland backend](docs/hyprland.md)
- [How WSF works](docs/how-it-works.md)
- [Troubleshooting](docs/troubleshooting.md)
- [Testing](docs/testing.md)
- [Packaging](docs/packaging.md)
- [Fedora COPR publishing](docs/copr.md)

## Project Status

WSF is a small open-source tool built around practical desktop behavior. The
GNOME backend is the oldest and most proven path. Hyprland support works today,
including Lua scroll application and optional pinch gesture tuning, but it is
still a newer backend and may need compositor-specific fixes as Hyprland
evolves.
