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
