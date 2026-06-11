# Troubleshooting

## Manual checklist

- `./build/tools/wsf set 0.35`
- `./build/tools/wsf enable`
- `./build/tools/wsf repair` if `status` or `doctor` reports a stale preload setup
- Log out and log back in (GNOME Wayland does not support "Alt+F2 r").
- `./build/tools/wsf status` and `./build/tools/wsf doctor`
- Test vertical + horizontal scrolling in GNOME apps (Files, Settings) and in a browser.
- Test pinch zoom in a photo viewer or maps app.

## If scroll speed does not change

- Ensure the environment file exists:
  `~/.config/environment.d/wayland-scroll-factor.conf`
- Ensure the library exists:
  `~/.local/lib/wayland-scroll-factor/libwsf_preload.so`
- For system installs, the library should exist at:
  `/usr/lib/wayland-scroll-factor/libwsf_preload.so`
- Confirm `gnome-shell` is the compositor (Wayland session).
- Verify the guard rail is not too strict: only `gnome-shell` is targeted.
- Ensure you logged out and logged back in after enabling/disabling.
- If `wsf doctor` says `environment.d` is present but `systemd --user` did not pick it up, run:
  `wsf repair`
  Then log out and log back in.
- If `wsf doctor` says `systemd --user` is ready but `gnome-shell` has not loaded WSF yet, log out and log back in again. If that still fails on your distro, reboot once.
- If `LD_PRELOAD` points to an old WSF location, `wsf repair` removes stale WSF
  entries and writes the exact installed `libwsf_preload.so` path.
- For pinch issues, check `wsf doctor` for "pinch hooks" symbol availability.
- If using a custom library location, set `WSF_LIB_PATH` before enabling.
- If a snap app fails to launch, update to a recent WSF version and rerun `wsf doctor`. Current builds remove WSF from child `LD_PRELOAD` after load to avoid inherited-preload problems.

## App-specific scroll differences

If GNOME apps such as Files or Settings change speed correctly, WSF is active.
If a specific app still scrolls too quickly, the remaining difference is likely
inside that app or toolkit rather than in WSF's compositor hook.

This has been reported with Electron-based apps such as Discord/Vesktop. Test
with an intentionally obvious value such as `wsf set 0.20`, then compare:

- a native GNOME app;
- a GTK Flatpak app;
- the affected app.

If GNOME and GTK apps change but the affected app does not, include the app
name, install method, Wayland/XWayland status, and `wsf doctor` output in the
issue.

## Hyprland

- Run `wsf status` and confirm it reports `hyprland: running`.
- Run `hyprctl getoption input:touchpad:scroll_factor` to inspect the live compositor value.
- Run `wsf set 0.35` or `wsf apply`; both should update the live Hyprland scroll factor without logout.
- On Hyprland 0.55+ Lua configs, `wsf status` / `wsf doctor` should report
  `hyprland scroll apply method: lua-eval`. WSF falls back from
  `hyprctl keyword` to `hyprctl eval` automatically.
- If vertical and horizontal factors differ, remember Hyprland currently has one native touchpad scroll factor for both axes.
- If the value changes but does not persist after restarting or reloading
  Hyprland, comment out any static `touchpad.scroll_factor` line and add the
  documented `wsf apply` autostart command. For Lua configs, load
  `/usr/share/wayland-scroll-factor/hyprland/wsf.lua` from `hyprland.lua`.
- Pinch zoom/rotate tuning requires launching Hyprland through `wsf-hyprland`
  with `start-hyprland --path`.
- For pinch zoom/rotate, `wsf doctor` should report
  `hyprland gesture preload: active`.
- Package installs provide a `Hyprland (WSF gestures)` session. Select it in
  the greeter if you want pinch zoom/rotate tuning without editing greetd or
  another login manager manually.
- If it is inactive, confirm `wsf-hyprland` is installed and your login/session
  command uses `start-hyprland --path "$(command -v wsf-hyprland)" -- ...`.
- With tuigreet, `--remember-session` overrides `--cmd` after a session has been
  remembered. Use `--session-wrapper /usr/bin/wsf-session-wrapper` for package
  installs, or a persistent fallback wrapper in `/usr/local/bin`.
- Do not leave `/etc/greetd/config.toml` pointing to
  `~/.local/bin/wsf-session-wrapper`. If that per-user install is removed,
  greetd can fail to start the selected session.
- If Hyprland shows a warning that it was launched directly, your session is
  bypassing `start-hyprland`. Change the session command to use
  `start-hyprland --path wsf-hyprland` instead of calling `Hyprland` directly.
- If `start-hyprland` appears in the process tree but the gesture preload is
  inactive, run `wsf doctor` and check the `hyprland launch recommendation`
  line.

## Debug mode

Set `WSF_DEBUG=1` before logging in to see a one-time init log from the
preload library. On GNOME Wayland it appears in the user journal.

Example:

```
printf 'WSF_DEBUG=1\n' >> ~/.config/environment.d/wayland-scroll-factor.conf
# log out / log back in
journalctl --user -b -g "wsf:"
```

For scroll-hook debugging, `WSF_TRACE=1` logs a small sampled set of scroll
events, including axis, original value, factor, scaled result, event type, and
axis source:

```
printf 'WSF_TRACE=1\n' >> ~/.config/environment.d/wayland-scroll-factor.conf
# log out / log back in
journalctl --user -b _COMM=gnome-shell | grep -E 'wsf: (init|trace|reloaded)'
```

Remove it after debugging:

```
sed -i '/^WSF_TRACE=/d' ~/.config/environment.d/wayland-scroll-factor.conf
```

WSF config files always use `.` as decimal separator. Current builds parse
those values independently of the process locale, so GNOME sessions using a
comma-decimal locale should still read values such as `0.0500` correctly.

If `libinput --version` is missing, install `libinput-tools` on Arch for
additional diagnostics.
