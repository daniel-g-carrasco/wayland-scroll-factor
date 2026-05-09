# Hyprland Backend

WSF has an experimental Hyprland backend for touchpad scroll tuning.

## What WSF Uses

Hyprland already exposes a native touchpad scroll setting:

```bash
hyprctl keyword input:touchpad:scroll_factor 0.35
```

WSF uses that native runtime setting for scroll instead of patching Hyprland.
The preload library is not loaded inside Hyprland by default.

For pinch zoom/rotate, WSF uses a separate launch-time path through
`wsf-hyprland`. This is intentionally separate from scroll because Hyprland
already handles scroll natively, while pinch zoom/rotate do not have equivalent
native settings.

## Current Behavior

- `wsf set <factor>` writes the WSF config and applies
  `input:touchpad:scroll_factor` live when a running Hyprland session is
  detected.
- `wsf set --scroll-vertical <factor>` applies the same native Hyprland setting.
- `wsf set --scroll-horizontal <factor>` also applies the same native Hyprland
  setting because Hyprland does not currently expose separate vertical and
  horizontal touchpad scroll factors.
- `wsf apply` reapplies the saved WSF config to supported live compositor
  backends. On Hyprland it uses `scroll_vertical_factor` as the canonical value
  if vertical and horizontal differ.
- `wsf status` and `wsf doctor` report the current Hyprland
  `input:touchpad:scroll_factor` value.

## Limits

- Hyprland native scroll tuning is one touchpad factor for both axes.
- Hyprland's native touchpad scroll factor range is treated as `0.0-2.0`; WSF
  clamps higher values when applying through `hyprctl`.
- Pinch zoom and pinch rotate sensitivity are not exposed as general native
  Hyprland settings for client applications.
- WSF can tune pinch zoom/rotate through the installed `wsf-hyprland` shim,
  launched via `start-hyprland --path`.
- WSF's scroll preload hooks stay disabled on Hyprland by default to avoid
  double scaling, because scroll is already handled by Hyprland's native
  `scroll_factor`.

## Experimental Pinch Support

To test pinch zoom/rotate, launch Hyprland with WSF's shim while keeping
Hyprland's recommended launcher in the path:

```bash
start-hyprland --path "$(command -v wsf-hyprland)" -- --config ~/.config/hypr/hyprland.conf
```

`wsf-hyprland` finds `libwsf_preload.so`, sets the Hyprland gesture-only
environment, and then executes the real `Hyprland` binary. This does not use
`/etc/ld.so.preload`; it only affects the launched compositor process. WSF
removes itself from `LD_PRELOAD` after loading, so child applications should
not inherit the preload setting.

After restarting Hyprland, check:

```bash
wsf doctor
```

Look for:

```text
hyprland gesture preload: active
runtime gesture reload: active via Hyprland gestures-only preload
```

For login managers, set the session command to the equivalent of the
`start-hyprland --path ... -- ...` command above. WSF does not modify login
manager configuration automatically.

For greetd/tuigreet, `--cmd` is only a default command. If `--remember-session`
is enabled, tuigreet can relaunch the remembered session and bypass `--cmd`.
Use the installed wrapper for selected/remembered sessions:

```bash
tuigreet ... --session-wrapper "$(command -v wsf-session-wrapper)"
```

The wrapper receives the selected session command. It leaves unrelated sessions
unchanged, wraps `Hyprland` with `wsf-hyprland`, and injects
`--path wsf-hyprland` when the selected session command is `start-hyprland`.

Example with a session wrapper:

```bash
exec start-hyprland --path "$(command -v wsf-hyprland)" -- --config "$HOME/.config/hypr/hyprland.conf"
```

If your system already uses `start-hyprland`, keep it and add only the `--path`
argument. Avoid launching `Hyprland` directly unless your distro/session already
does that, because modern Hyprland recommends `start-hyprland`.

## Persistence

`hyprctl keyword ...` changes the running Hyprland session. To make WSF the
source of truth, keep any static Hyprland `touchpad.scroll_factor` line
commented out and apply WSF at session start.

Recommended input config pattern:

```ini
touchpad {
    natural_scroll = true

    # Touchpad scroll speed is managed by Wayland Scroll Factor (WSF).
    # Hyprland exposes one native `scroll_factor` shared by vertical and
    # horizontal axes. Keep this commented so reload/login does not overwrite
    # the value chosen in WSF/WSF GUI.
    #
    # scroll_factor = 0.3

    tap-to-click = true
    clickfinger_behavior = true
}
```

Recommended startup config pattern:

```ini
exec-once = sh -lc 'if command -v wsf >/dev/null 2>&1; then wsf apply; elif [ -x "$HOME/.local/bin/wsf" ]; then "$HOME/.local/bin/wsf" apply; fi'
```

For system images or preconfigured desktops, prefer launching `wsf apply` as
part of the compositor/session startup path after Hyprland has exported its
runtime environment.

If you also want pinch zoom/rotate, the system image should launch Hyprland
through:

```bash
start-hyprland --path /path/to/wsf-hyprland -- ...
```

and still keep the `wsf apply` autostart for native scroll persistence.

For greeters that support session wrappers, `wsf-session-wrapper` is usually
safer than replacing the whole session command because it preserves session
selection and leaves non-Hyprland sessions untouched.

## Source Audit

Audited on 2026-05-09 against:

- Local Hyprland `0.54.3`
- Upstream Hyprland master commit `11bd00c`
- Upstream Aquamarine commit `813c1e8`

Findings:

- Hyprland applies `input:touchpad:scroll_factor` to pointer axis events coming
  from finger scrolling.
- The same Hyprland factor is used for both vertical and horizontal scroll axes.
- Hyprland does not provide a comparable general-purpose pinch zoom/rotate
  sensitivity setting for client gestures.
- Aquamarine still reads libinput scroll and pinch values through libinput
  getter functions, and the `wsf-hyprland` shim can preload WSF only for
  Hyprland gesture hooks.

See also: [`how-it-works.md`](how-it-works.md).
