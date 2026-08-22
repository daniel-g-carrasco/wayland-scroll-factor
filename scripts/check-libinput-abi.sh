#!/usr/bin/env bash
# Check that every libinput symbol WSF interposes still exists, with the
# signature we assume, in the libinput this system ships.
#
# WHY THIS EXISTS
#
# WSF works by interposing six libinput entry points inside gnome-shell.
# That is a contract with two parties, and neither of them is us:
#
#   1. libinput must still EXPORT the symbol. If it disappears or is
#      renamed, our definition stops shadowing anything and we silently
#      become a no-op: scroll goes back to normal and nothing errors.
#   2. the compositor must still CALL it. mutter could keep libinput
#      happy while routing scroll through some other path, and again we
#      would be a silent no-op.
#
# This script covers (1) mechanically. Point (2) cannot be checked from
# here: it is a grep over the compositor's source, and the answer for
# mutter is recorded in docs/compatibility.md.
#
# The symbol list is READ FROM THE SOURCE rather than typed here, so it
# cannot drift from what the preload actually defines.
#
#   usage: scripts/check-libinput-abi.sh [/path/to/libinput.so]
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="${ROOT}/src/wsf_preload.c"

[[ -r "$SRC" ]] || {
  echo "cannot read ${SRC}" >&2
  exit 1
}

mapfile -t SYMBOLS < <(
  grep -oE 'WSF_EXPORT [a-z]+ libinput_[a-z0-9_]+' "$SRC" \
    | grep -oE 'libinput_[a-z0-9_]+' | sort -u
)

((${#SYMBOLS[@]} > 0)) || {
  echo "found no WSF_EXPORT libinput_* symbols in ${SRC}" >&2
  echo "(the extraction pattern and the source have diverged)" >&2
  exit 1
}

LIB="${1:-}"
if [[ -z "$LIB" ]]; then
  # ldconfig knows where the runtime linker will actually find it, which
  # is the copy gnome-shell will load.
  LIB="$(ldconfig -p 2>/dev/null | awk '/libinput\.so\.[0-9]/ {print $NF; exit}' || true)"
fi
[[ -n "$LIB" && -r "$LIB" ]] || {
  echo "libinput not found. Install it, or pass the path as an argument." >&2
  exit 1
}

command -v nm >/dev/null 2>&1 || {
  echo "nm not found (binutils)." >&2
  exit 1
}

echo "libinput:  ${LIB}"
if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists libinput; then
  echo "version:   $(pkg-config --modversion libinput)"
fi
echo "symbols:   ${#SYMBOLS[@]} interposed by src/wsf_preload.c"
echo

# libinput versions its symbols, so nm prints them as
# `libinput_event_pointer_get_scroll_value@@LIBINPUT_1.19`. Keep the tag:
# it says which ABI version introduced the symbol, and a symbol that
# moved to a new version node is worth noticing too.
exported="$(nm -D --defined-only "$LIB" 2>/dev/null | awk '$2 == "T" {print $NF}')"
missing=0
for sym in "${SYMBOLS[@]}"; do
  entry="$(grep -m1 -E "^${sym}(@@.*)?$" <<<"$exported" || true)"
  if [[ -n "$entry" ]]; then
    ver="${entry#*@@}"
    [[ "$ver" == "$entry" ]] && ver="unversioned"
    printf '  ok       %-46s %s\n' "$sym" "$ver"
  else
    printf '  MISSING  %s\n' "$sym"
    missing=$((missing + 1))
  fi
done

echo
if ((missing > 0)); then
  echo "${missing} interposed symbol(s) are gone from this libinput."
  echo "WSF would load and do nothing on this system: the preload can only"
  echo "shadow a symbol that exists. Check libinput's release notes before"
  echo "shipping against it."
  exit 1
fi
echo "All interposed symbols present. The preload still has something to shadow."
