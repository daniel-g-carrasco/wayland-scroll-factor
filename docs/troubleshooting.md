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
- For pinch issues, check `wsf doctor` for "pinch hooks" symbol availability.
- If using a custom library location, set `WSF_LIB_PATH` before enabling.

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
