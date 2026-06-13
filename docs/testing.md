# Testing

WSF has two different kinds of tests:

- container smoke tests for build, install, metadata, and CLI behavior;
- real Wayland session tests for compositor input behavior.

Containers are useful, but they cannot prove that libinput events are modified
inside `gnome-shell` or `Hyprland`, and they cannot prove how a real Flatpak or
Electron window consumes Wayland pointer-axis events.

## Iterating on the GUI (distrobox)

To work on the GUI without rebasing an atomic host or spinning up a VM:

```bash
scripts/dev-distrobox.sh create   # one-time: Ubuntu box + deps + first build
scripts/dev-distrobox.sh build    # rebuild + reinstall after editing code
scripts/dev-distrobox.sh gui      # launch wsf-gui on your session
```

distrobox shares `$HOME` and the Wayland socket, so the window opens on your
host session. This exercises the GUI app (window, About dialog, sliders,
toasts) and the CLI it drives — but **not** real scroll-injection behavior,
which needs the compositor's own `gnome-shell` to load the preload (use a
separate VM with a full GNOME session for that).

## Container Smoke Tests

Use Podman from the repository root:

```bash
scripts/test-containers.sh fedora
scripts/test-containers.sh ubuntu
scripts/test-containers.sh debian
scripts/test-containers.sh arch
scripts/test-containers.sh opensuse
```

Run the whole matrix:

```bash
scripts/test-containers.sh all
```

The script installs dependencies inside disposable containers, builds WSF,
installs it into the container, checks CLI JSON output, checks Python GUI
syntax, and validates installed desktop/metainfo files when the distro provides
the validation tools.

These tests do not install anything on the host system.

## Local Build Checks

If Meson and Ninja are installed locally:

```bash
meson setup build --prefix="$HOME/.local" --buildtype=release
meson compile -C build
python3 -m py_compile gui/wsf_gui.py
```

Optional metadata checks:

```bash
desktop-file-validate build/data/io.github.danielgrasso.WaylandScrollFactor.desktop
appstreamcli validate --no-net data/io.github.danielgrasso.WaylandScrollFactor.metainfo.xml
```

The Hyprland session file under `build/data/wayland-scroll-factor-hyprland.desktop`
is installed to `share/wayland-sessions`. It intentionally contains
session-specific keys such as `DesktopNames`, so `desktop-file-validate` may
report application-desktop errors on it. Treat that file as a greeter/session
entry, not as a normal launcher.

## GNOME Runtime Checks

GNOME behavior must be tested in a real GNOME Wayland session.

```bash
wsf enable
# log out and back in
wsf doctor
```

Expected:

```text
gnome-shell library mapped: yes
runtime config reload: active
```

Then test obvious values:

```bash
wsf set 0.10
# start a new two-finger scroll gesture
wsf set 3.00
# start a new two-finger scroll gesture
```

Compare GNOME Files, Settings, and a browser. Mouse wheel scrolling should not
be scaled.

## Flatpak And Electron Checks

If GNOME apps react to WSF but a Flatpak app does not, first separate these
cases:

- a GTK Flatpak app;
- an Electron Flatpak app;
- an app running through XWayland.

Use an obvious value:

```bash
wsf set 0.10
wsf doctor
```

Test at least one GTK Flatpak, for example Calculator or Text Editor from
Flathub, then test the affected app.

Check whether an app is running through XWayland:

```bash
xlsclients | grep -Ei 'discord|vesktop|app-name' || echo "not listed as X11/XWayland"
```

`xprop WM_CLASS` is another quick check: click the app window. Native Wayland
windows usually do not answer through `xprop`; XWayland windows do.

Some Electron Flatpaks need explicit Wayland testing. For example:

```bash
flatpak run --nosocket=x11 --socket=wayland dev.vencord.Vesktop
flatpak run --nosocket=x11 --socket=wayland com.discordapp.Discord
```

If GTK Flatpaks react but Electron apps do not, document the app name, Flatpak
ID, Wayland/XWayland status, Electron version if known, and `wsf doctor`
output. That points to app/toolkit scroll handling rather than WSF failing to
load in the compositor.

For deeper compositor tracing, enable trace logs before login:

```bash
mkdir -p ~/.config/environment.d
cat > ~/.config/environment.d/wsf-debug.conf <<'EOF'
WSF_DEBUG=1
WSF_TRACE=1
EOF
systemctl --user daemon-reexec
# log out and back in
journalctl --user -b _COMM=gnome-shell | grep -E 'wsf: (init|trace|reloaded)'
```

Remove trace mode afterwards:

```bash
rm -f ~/.config/environment.d/wsf-debug.conf
systemctl --user daemon-reexec
```

## Hyprland Runtime Checks

Hyprland scroll should apply live:

```bash
wsf set 0.35
hyprctl getoption input:touchpad:scroll_factor
```

For Hyprland 0.55+ Lua configs, `wsf doctor` should report:

```text
hyprland scroll apply method: lua-eval
```

Pinch zoom/rotate requires launching through the WSF gesture session or wrapper.
Expected:

```text
hyprland gesture preload: active
runtime gesture reload: active via Hyprland gestures-only preload
```
