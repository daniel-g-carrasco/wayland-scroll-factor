# Troubleshooting

## Manual checklist

- `./build/tools/wsf set 0.35`
- `./build/tools/wsf enable`
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
  `systemctl --user daemon-reexec`
  Then log out and log back in.
- If `wsf doctor` says `systemd --user` is ready but `gnome-shell` has not loaded WSF yet, log out and log back in again. If that still fails on your distro, reboot once.
- For pinch issues, check `wsf doctor` for "pinch hooks" symbol availability.
- If using a custom library location, set `WSF_LIB_PATH` before enabling.
- If a snap app fails to launch, update to a recent WSF version and rerun `wsf doctor`. Current builds remove WSF from child `LD_PRELOAD` after load to avoid inherited-preload problems.

## Hyprland

- Run `wsf status` and confirm it reports `hyprland: running`.
- Run `hyprctl getoption input:touchpad:scroll_factor` to inspect the live compositor value.
- Run `wsf set 0.35` or `wsf apply`; both should update the live Hyprland scroll factor without logout.
- If vertical and horizontal factors differ, remember Hyprland currently has one native touchpad scroll factor for both axes.
- If the value changes but does not persist after restarting Hyprland, comment out any static `touchpad.scroll_factor` line and add the documented `wsf apply` autostart command to your Hyprland startup config.
- Pinch zoom/rotate tuning requires launching Hyprland through `wsf-hyprland`
  with `start-hyprland --path`.
- For pinch zoom/rotate, `wsf doctor` should report
  `hyprland gesture preload: active`.
- If it is inactive, confirm `wsf-hyprland` is installed and your login/session
  command uses `start-hyprland --path "$(command -v wsf-hyprland)" -- ...`.
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

If `libinput --version` is missing, install `libinput-tools` on Arch for
additional diagnostics.
