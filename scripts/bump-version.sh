#!/usr/bin/env bash
# scripts/bump-version.sh X.Y.Z [--date YYYY-MM-DD] [--name NAME --email EMAIL]
#
# Writes a new version into every file that carries one, taking the release
# notes from the "## [Unreleased]" section of CHANGELOG.md:
#
#   meson.build                                version: 'X.Y.Z'
#   packaging/rpm/wayland-scroll-factor.spec   Version:, Release: back to 1, a %changelog entry
#   packaging/debian/debian/changelog          a new entry (dpkg names the .deb after it)
#   data/...WaylandScrollFactor.metainfo.xml   a new <release>
#   CHANGELOG.md                               "## [Unreleased]" becomes "## [X.Y.Z] - date"
#   README.md                                  "Current release: X.Y.Z."
#
# One command instead of six edits: v1.0.1 (2026-10-30) went out with .deb
# files still called 1.0.0 because the Debian changelog had been forgotten.
# scripts/check-version.sh reads the same places back; CI runs it on every
# push, and the release, COPR and AUR workflows refuse a tag whose files
# disagree with it.
#
# Nothing is committed or tagged here: review `git diff`, commit, tag vX.Y.Z.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

MESON=meson.build
SPEC=packaging/rpm/wayland-scroll-factor.spec
DEBCL=packaging/debian/debian/changelog
META=data/io.github.danielgrasso.WaylandScrollFactor.metainfo.xml
CHANGELOG=CHANGELOG.md
README=README.md
PACKAGE=wayland-scroll-factor

usage() {
  echo "usage: $0 X.Y.Z [--date YYYY-MM-DD] [--name NAME --email EMAIL]" >&2
  exit 2
}

NEW="${1:-}"
[ -n "$NEW" ] || usage
shift
DATE="$(date +%F)"
NAME=""
EMAIL=""
while [ $# -gt 0 ]; do
  case "$1" in
    --date) DATE="${2:-}"; shift 2 ;;
    --name) NAME="${2:-}"; shift 2 ;;
    --email) EMAIL="${2:-}"; shift 2 ;;
    *) usage ;;
  esac
done
[[ "$NEW" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "error: '$NEW' is not X.Y.Z" >&2; exit 2; }
[[ "$DATE" =~ ^[0-9]{4}-[0-9]{2}-[0-9]{2}$ ]] || { echo "error: '$DATE' is not YYYY-MM-DD" >&2; exit 2; }
date -d "$DATE" >/dev/null 2>&1 || { echo "error: '$DATE' is not a date" >&2; exit 2; }

for f in "$MESON" "$SPEC" "$DEBCL" "$META" "$CHANGELOG" "$README"; do
  [ -f "$f" ] || { echo "error: $f is missing" >&2; exit 1; }
done

CURRENT="$(sed -n "s/^  version: '\([0-9.]*\)',\$/\1/p" "$MESON")"
[ -n "$CURRENT" ] || { echo "error: no \"version: '...'\" line in $MESON" >&2; exit 1; }
[ "$NEW" != "$CURRENT" ] || { echo "error: $MESON already says $CURRENT" >&2; exit 1; }
if grep -q "^## \[$NEW\]" "$CHANGELOG"; then
  echo "error: $CHANGELOG already has a [$NEW] section" >&2
  exit 1
fi

# The release notes: every "- " entry under "## [Unreleased]" up to the next
# heading, continuation lines joined, group headings (### Fixed) dropped.
mapfile -t BULLETS < <(awk '
  /^## \[Unreleased\]/ { on = 1; next }
  on && /^## \[/ { exit }
  on && /^- / { if (b != "") print b; b = substr($0, 3); next }
  on && /^  [^ ]/ && b != "" { b = b " " substr($0, 3); next }
  on && b != "" { print b; b = "" }
  END { if (b != "") print b }
' "$CHANGELOG")
if [ "${#BULLETS[@]}" -eq 0 ]; then
  echo "error: $CHANGELOG has no '## [Unreleased]' section with '- ' entries; write the notes first" >&2
  exit 1
fi

# The maintainer, from the newest Debian entry unless given.
if [ -z "$NAME" ] || [ -z "$EMAIL" ]; then
  SIG="$(grep -m1 '^ -- ' "$DEBCL" || true)"
  [ -n "$NAME" ] || NAME="$(sed -n 's/^ -- \(.*\) <.*/\1/p' <<<"$SIG")"
  [ -n "$EMAIL" ] || EMAIL="$(sed -n 's/^ -- .* <\([^>]*\)>.*/\1/p' <<<"$SIG")"
fi
if [ -z "$NAME" ] || [ -z "$EMAIL" ]; then
  echo "error: no maintainer line in $DEBCL; pass --name and --email" >&2
  exit 1
fi

if [ "$DATE" = "$(date +%F)" ]; then
  STAMP="$(date -R)"
else
  STAMP="$(date -R -d "$DATE 12:00")"
fi
SPECDATE="$(LC_ALL=C date -d "$DATE" '+%a %b %d %Y')"

deb_entry() {
  local b
  printf '%s (%s-1) unstable; urgency=medium\n\n' "$PACKAGE" "$NEW"
  for b in "${BULLETS[@]}"; do
    printf '%s\n' "$b" | fmt -w 68 | sed '1s/^/  * /; 2,$s/^/    /'
  done
  printf '\n -- %s <%s>  %s\n\n' "$NAME" "$EMAIL" "$STAMP"
}

spec_entry() {
  local b
  printf '* %s %s <%s> - %s-1\n' "$SPECDATE" "$NAME" "$EMAIL" "$NEW"
  for b in "${BULLETS[@]}"; do
    printf '%s\n' "$b" | fmt -w 70 | sed '1s/^/- /; 2,$s/^/  /'
  done
  printf '\n'
}

xml_text() {
  sed 's/&/\&amp;/g; s/</\&lt;/g; s/>/\&gt;/g; s/`//g'
}

meta_entry() {
  local b
  printf '    <release version="%s" date="%s">\n      <description>\n' "$NEW" "$DATE"
  for b in "${BULLETS[@]}"; do
    printf '        <p>%s</p>\n' "$(printf '%s' "$b" | xml_text)"
  done
  printf '      </description>\n    </release>'
}

# insert_after FILE REGEX: the block on stdin goes after the first matching line.
insert_after() {
  local file="$1" re="$2" tmp
  grep -q -- "$re" "$file" || { echo "error: no line matching '$re' in $file" >&2; exit 1; }
  tmp="$(mktemp)"
  BLOCK="$(cat)" awk -v re="$re" '
    BEGIN { block = ENVIRON["BLOCK"] }
    { print }
    !done && $0 ~ re { print block; done = 1 }
  ' "$file" > "$tmp" && mv "$tmp" "$file"
}

# replace_once FILE SED_EXPR WHAT: the expression must change the file.
replace_once() {
  local before after
  before="$(md5sum < "$1")"
  sed -i -e "$2" "$1"
  after="$(md5sum < "$1")"
  [ "$before" != "$after" ] || { echo "error: $3 not found in $1" >&2; exit 1; }
}

CUR_RE="${CURRENT//./\\.}"
replace_once "$MESON" "s/^  version: '$CUR_RE',\$/  version: '$NEW',/" "version: '$CURRENT'"
replace_once "$SPEC" "s/^Version:\( *\)$CUR_RE\$/Version:\1$NEW/" "Version: $CURRENT"
sed -i -e 's/^Release:\( *\)[0-9][0-9]*%{?dist}$/Release:\11%{?dist}/' "$SPEC"
spec_entry | insert_after "$SPEC" '^%changelog$'
meta_entry | insert_after "$META" '^  <releases>$'
replace_once "$CHANGELOG" "s/^## \[Unreleased\]\$/## [$NEW] - $DATE/" "## [Unreleased]"
replace_once "$README" "s/Current release: $CUR_RE\./Current release: $NEW./" "Current release: $CURRENT."
tmp="$(mktemp)"
{ deb_entry; cat "$DEBCL"; } > "$tmp" && mv "$tmp" "$DEBCL"

echo "$CURRENT -> $NEW ($DATE), ${#BULLETS[@]} release note(s):"
printf '  - %s\n' "${BULLETS[@]}"
echo
scripts/check-version.sh "$NEW"
echo
echo "Next: review 'git diff', commit (release: v$NEW), push, wait for CI, then 'git tag v$NEW && git push origin v$NEW'."
