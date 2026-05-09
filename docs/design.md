# Design

## libinput-config summary

- Technique: global `LD_PRELOAD` via `/etc/ld.so.preload`, injecting a shared
  library into every process that loads libinput.
- Interposition strategy: uses `dlsym(RTLD_NEXT, ...)` to wrap libinput entry
  points. It hooks libinput context creation and event fetching to apply config
  when devices are added, and intercepts pointer axis functions to scale scroll
  values.
- Scroll factor: multiplies values returned by
  `libinput_event_pointer_get_axis_value`,
  `libinput_event_pointer_get_axis_value_discrete`,
  `libinput_event_pointer_get_scroll_value`, and
  `libinput_event_pointer_get_scroll_value_v120`, with optional per-axis
  factors.
- Config: key/value file at `/etc/libinput.conf`, parsed by the injected
  library at runtime.

## Why it is fragile (especially on modern Wayland desktops)

- Global preload is high-risk: `/etc/ld.so.preload` affects every process, can
  break unrelated apps, and is hard to safely roll back.
- Root install + system-wide config makes it hard to keep changes per-user and
  reversible.
- Compositor/libinput internals change across releases; the interposed symbols
  and event flow are not stable API, so updates can silently break behavior.
- Sandboxing (Flatpak/Snap) and hardened environments can block or ignore the
  preload, causing inconsistent results.
- Assumes libinput is used directly by the compositor; any change in how GNOME
  (mutter/gnome-shell) integrates libinput can bypass the hooks.

## Why WSF is safer on GNOME Wayland

- Per-user install and config; no `/etc/ld.so.preload`, easy rollback.
- Guard rail: interposition is active only inside `gnome-shell`.
- Touchpad-only scaling: mouse wheel events are not modified.
- Narrow scope: focuses on scroll + pinch to reduce interaction with unrelated
  libinput behaviors.
- Built-in diagnostics (`wsf doctor`) to verify symbols and environment state.

## Hyprland backend design

- Objective: support Hyprland touchpad scroll tuning without patching Hyprland.
- Mechanism: prefer Hyprland's native runtime setting
  `input:touchpad:scroll_factor` via `hyprctl`.
- Scope: Hyprland exposes one native touchpad scroll factor, so WSF maps both
  vertical and horizontal scroll controls to that single value.
- Pinch zoom/rotate: Hyprland does not expose native client sensitivity
  controls. WSF supports an optional `wsf-hyprland` launcher shim that is used
  through `start-hyprland --path` and activates only gesture hooks inside the
  compositor process.
- Preload safety: WSF does not activate scroll preload hooks inside Hyprland by
  default, avoiding double scaling between native Hyprland scroll handling and
  libinput interposition.
- Persistence: `wsf set` applies live on a running Hyprland session. Hyprland
  integrations should keep static `touchpad.scroll_factor` commented out and
  run `wsf apply` from session startup, preferably through a shell fallback that
  tries `wsf` from `PATH` and then `$HOME/.local/bin/wsf`.
- Distribution integration: WSF installs `wsf-hyprland`, but does not rewrite
  login manager configuration. Public packages or OS images can expose a
  Hyprland session that runs `start-hyprland --path /path/to/wsf-hyprland -- ...`.

Full runtime flow: [`how-it-works.md`](how-it-works.md).

## MVP design (GNOME Wayland)

- Objective: per-user control of two-finger scroll speed on GNOME Wayland.
- Target: `gnome-shell` on Wayland (future extensible to other compositors).
- Mechanism: user-level `LD_PRELOAD` via `~/.config/environment.d`, with
  function interposition on libinput scroll-value getters.
- Scope guard: if the current process is not `gnome-shell`, the library is a
  no-op and returns original values.
- Config: `~/.config/wayland-scroll-factor/config` with `factor=0.35` (example).
- Env overrides:
  - `WSF_FACTOR` overrides the config file for the current session.
  - `WSF_DEBUG=1` enables one-time debug logging.
- Safety: no `/etc/ld.so.preload`, no root changes, per-user install only.
- Uninstall: remove the environment file + config directory; optional remove
  `~/.local/lib/wayland-scroll-factor/` and `~/.local/bin/wsf`.

## V1 audit (current symbol coverage)

- Interposed symbols: `libinput_event_pointer_get_scroll_value` and
  `libinput_event_pointer_get_scroll_value_v120`.
- Scaling applies to both scroll axes via the passed axis argument, using a
  single `factor` value from config/env.
- No axis-source filtering yet, so mouse wheel and touchpad would be treated
  the same by the wrapper.
- No gesture (pinch/rotate) symbols are wrapped in v1.

## Terminology (for config and troubleshooting)

- Tapping/clicking: discrete button-like actions (1/2/3 finger tap or click).
- Scrolling: continuous motion on vertical/horizontal axes (two-finger scroll).
- Gestures: continuous multi-finger actions like pinch (zoom), rotate, swipe, hold.
- Double tap: usually a double-click timing shortcut, not a continuous gesture.
