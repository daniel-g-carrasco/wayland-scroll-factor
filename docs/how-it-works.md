# How WSF Works

WSF has one config format and multiple runtime backends. The backend depends on
the compositor.

Config lives at:

```text
~/.config/wayland-scroll-factor/config
```

Typical values:

```ini
scroll_vertical_factor=0.35
scroll_horizontal_factor=0.35
pinch_zoom_factor=1.00
pinch_rotate_factor=1.00
```

The GUI and CLI both write the same config. They do not duplicate input logic.

## Components

- `wsf`: CLI for setting factors, applying live compositor backends, enabling
  GNOME preload, and running diagnostics.
- `wsf-gui`: GTK/libadwaita GUI that calls `wsf`.
- `libwsf_preload.so`: optional preload library that wraps selected libinput
  getter functions.
- `wsf-hyprland`: launcher shim used with `start-hyprland --path` for Hyprland
  gesture tuning.

## GNOME Wayland Flow

GNOME does not expose a native touchpad scroll speed setting. WSF therefore uses
a guarded user-level preload backend.

Activation:

```bash
wsf enable
```

This writes:

```text
~/.config/environment.d/wayland-scroll-factor.conf
```

with:

```ini
LD_PRELOAD=/path/to/libwsf_preload.so
```

After logout/login, `gnome-shell` starts with that preload in its environment.
The preload library then:

1. Detects the process name.
2. Activates only if the process is `gnome-shell`.
3. Resolves libinput symbols with `dlsym(RTLD_NEXT, ...)`.
4. Scales GNOME/Mutter continuous scroll through
   `libinput_event_pointer_get_scroll_value()`. Mutter 50 uses that path for
   finger/continuous scroll and keeps wheel scroll on the v120 path, so mouse
   wheel behavior remains unscaled.
5. Scales pinch zoom/rotate values.
6. Removes WSF from `LD_PRELOAD` in the process environment after loading, so
   child applications do not inherit the preload.

Once loaded, WSF checks the config file again while handling libinput gesture
events. Factor changes should not require another logout, but they are not a
separate compositor command: test with a new gesture after changing a value.
Logout is only required to load or unload the preload itself.

The CLI writes the config through a temporary file and atomic rename, so the
preload never observes a half-written config. The preload also refreshes the
effective factors periodically while handling gestures instead of relying only
on file timestamp changes.

Config values are parsed with the C numeric locale regardless of the process
locale. This matters because GNOME Shell may run with a locale that uses a
comma decimal separator, while the WSF config format intentionally uses ASCII
decimal values such as `0.0500`.

## Hyprland Scroll Flow

Hyprland exposes a native touchpad scroll setting:

```bash
hyprctl keyword input:touchpad:scroll_factor 0.35
```

WSF uses that native backend for scroll. This means:

- no preload is needed for scroll;
- changes apply live;
- vertical and horizontal sliders are synchronized because Hyprland exposes one
  shared touchpad scroll factor for both axes.

When a running Hyprland session is detected:

```bash
wsf set 0.35
```

writes the WSF config and applies:

```bash
hyprctl keyword input:touchpad:scroll_factor 0.35
```

`wsf apply` reapplies the saved config to the running Hyprland session. This is
useful at compositor startup because static Hyprland config reloads can
overwrite runtime settings.

## Hyprland Pinch Zoom/Rotate Flow

Hyprland does not currently expose native general-purpose client settings for
pinch zoom or pinch rotate sensitivity. For those gestures, WSF can use a
targeted preload, but it must be loaded when Hyprland starts.

The recommended public integration is:

```bash
start-hyprland --path "$(command -v wsf-hyprland)" -- --config ~/.config/hypr/hyprland.conf
```

Why this form:

- `start-hyprland` remains the official Hyprland launcher path.
- `wsf-hyprland` is passed as the Hyprland binary path.
- `wsf-hyprland` sets the preload environment and then executes the real
  `Hyprland` binary.
- WSF does not need to patch Hyprland or replace `start-hyprland`.

`wsf-hyprland` sets:

```ini
WSF_TARGETS=Hyprland
WSF_HYPRLAND_GESTURES_ONLY=1
WSF_DEFER_PRUNE_UNTIL_TARGET=1
LD_PRELOAD=/path/to/libwsf_preload.so
```

Then it runs the real `Hyprland`.

Inside the Hyprland process, the preload library:

1. Detects the process name `Hyprland`.
2. Enables gesture hooks.
3. Keeps scroll hooks disabled by default to avoid double scaling, because
   scroll is already handled through Hyprland's native `scroll_factor`.
4. Wraps `libinput_event_gesture_get_scale()` for pinch zoom.
5. Wraps `libinput_event_gesture_get_angle_delta()` for pinch rotate.
6. Removes WSF from `LD_PRELOAD` after the target process has loaded, so child
   applications should not inherit the preload.

Pinch zoom uses this non-destructive mapping:

```text
new_scale = 1.0 + (old_scale - 1.0) * pinch_zoom_factor
```

Pinch rotate uses:

```text
new_angle_delta = old_angle_delta * pinch_rotate_factor
```

Once `wsf doctor` reports the gesture preload as active, changes to
`pinch_zoom_factor` and `pinch_rotate_factor` are picked up live from the config.

## What The Bootstrap Does

The bootstrap/install scripts install files under the selected prefix, normally
`~/.local`:

```text
~/.local/bin/wsf
~/.local/bin/wsf-gui
~/.local/bin/wsf-hyprland
~/.local/lib/wayland-scroll-factor/libwsf_preload.so
```

They do not automatically rewrite login manager configuration. That is
intentional: greetd, SDDM, TTY scripts, and distribution sessions all wire
Hyprland differently.

For general users, WSF documents the command they need:

```bash
start-hyprland --path "$(command -v wsf-hyprland)" -- ...
```

For a distribution or OS image, the package/session can integrate that command
directly.

## Safety Model

- WSF never uses `/etc/ld.so.preload`.
- GNOME preload is per-user via `~/.config/environment.d`.
- Hyprland gesture preload is launch-scoped through `wsf-hyprland`.
- The preload library is a no-op outside configured targets.
- Mouse wheel scroll is not scaled.
- On Hyprland, preload scroll hooks are disabled by default.
- WSF prunes its own `LD_PRELOAD` entry after loading to reduce inherited
  preload problems in child processes.

## Verification

Run:

```bash
wsf doctor
```

Expected Hyprland gesture-ready lines:

```text
hyprland library mapped: yes
hyprland WSF_TARGETS: Hyprland
hyprland WSF_HYPRLAND_GESTURES_ONLY: 1
hyprland gesture preload: active
runtime gesture reload: active via Hyprland gestures-only preload
```

Expected Hyprland launch recommendation:

```text
hyprland WSF launcher: /home/user/.local/bin/wsf-hyprland
start-hyprland: /usr/bin/start-hyprland
hyprland launch recommendation: start-hyprland --path /home/user/.local/bin/wsf-hyprland -- ...
```
