# Desktop Compatibility

WSF does not extend GNOME. It interposes six libinput functions inside the
`gnome-shell` process, so a new GNOME release cannot break WSF's API, because
WSF does not use one. What it can break is quieter and worse: WSF keeps
loading, reports itself enabled, and silently stops doing anything.

That failure has exactly two causes, and they are worth naming separately
because they are checked in different places.

**1. libinput stops exporting a symbol.** A preload can only shadow a function
that exists. If libinput renames or drops one, our definition shadows nothing.

**2. The compositor stops calling it.** libinput can keep the symbol while
mutter routes scroll through some other path. Everything still links, and
nothing happens.

Neither shows up as an error. Both show up as "scroll speed went back to
normal and I do not know why".

## Checking part 1: does libinput still export them?

```bash
scripts/check-libinput-abi.sh            # against this system's libinput
scripts/check-libinput-abi.sh /path/to/libinput.so.10
```

The symbol list is read out of `src/wsf_preload.c` rather than typed into the
script, so it cannot drift from what the preload actually defines. Output
includes each symbol's ABI version tag, because a symbol that moved to a new
version node is also worth a look.

This runs automatically in the Rawhide test lane:

```bash
scripts/test-containers.sh rawhide
```

Rawhide carries the next GNOME months before it reaches a laptop, which makes
it the early-warning lane: it prints the GNOME version it is testing against,
checks the ABI, then builds and installs WSF on that toolchain.

## Checking part 2: does the compositor still call them?

There is no clever way to do this. It is a grep over mutter's source, in the
one file that turns libinput events into Clutter events:

```bash
for f in libinput_event_pointer_get_scroll_value \
         libinput_event_pointer_get_scroll_value_v120 \
         libinput_event_gesture_get_scale \
         libinput_event_gesture_get_angle_delta; do
  printf '%-48s ' "$f"
  curl -s "https://gitlab.gnome.org/GNOME/mutter/-/raw/51.beta/src/backends/native/meta-seat-impl.c" \
    | grep -c "$f"
done
```

Compare the counts against the previous stable branch (`gnome-50`, `gnome-49`).
Equal counts mean the path we sit on is unchanged. A drop to zero is the
finding: the symbol survived, the caller did not.

Use a release tag or a branch that exists. A 404 from GitLab greps as zero and
looks exactly like a removal.

## Results

| GNOME | Checked | libinput exports | mutter calls | Build |
| --- | --- | --- | --- | --- |
| 51 beta | 2026-08-22 | 6/6, libinput 1.31.3 (Rawhide) | 4/4 at tag `51.beta`, same counts as 49 and 50 | ok on Rawhide |

The two symbols mutter does not call, `libinput_event_pointer_get_axis_value`
and `..._discrete`, are the pre-1.19 scroll API. mutter has not called them in
any of 49, 50 or 51; WSF keeps interposing them for older compositors.

## What none of this proves

That a two-finger swipe on a real touchpad actually scrolls slower. Containers
have no compositor and no touchpad, and a VM has no touchpad either. The
mechanical checks answer "can this still work"; only a GNOME 51 session on real
hardware answers "does it". See [testing.md](testing.md) for the session tests.
