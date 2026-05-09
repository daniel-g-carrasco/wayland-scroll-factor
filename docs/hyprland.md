# Hyprland Backend

WSF has an experimental Hyprland backend for touchpad scroll tuning.

## What WSF Uses

Hyprland already exposes a native touchpad scroll setting:

```bash
hyprctl keyword input:touchpad:scroll_factor 0.35
```

WSF uses that native runtime setting instead of patching Hyprland and instead
of enabling the preload library inside Hyprland by default.

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
- WSF's preload library remains GNOME-focused by default to avoid double
  scaling on Hyprland.

## Persistence

`hyprctl keyword ...` changes the running Hyprland session. To apply WSF at
session start, add this to your Hyprland startup config:

```ini
exec-once = wsf apply
```

For Margine OS integration, prefer launching `wsf apply` as part of the
compositor/session startup path after Hyprland has exported its runtime
environment.

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
  getter functions, but WSF intentionally does not activate its preload hooks in
  Hyprland by default.
