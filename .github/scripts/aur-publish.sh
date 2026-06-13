#!/usr/bin/env bash
# Publish the stable + -git packages to the AUR. Runs as the unprivileged
# 'builder' user INSIDE an archlinux container (makepkg refuses root), with
# ~/.ssh/aur = the AUR deploy key already in place. Invoked by
# .github/workflows/aur.yml.
#
# - stable (wayland-scroll-factor): pkgver/pkgrel/sha256 are pinned from the
#   release tarball of the triggering vX.Y.Z tag, then .SRCINFO is regenerated
#   and pushed.
# - -git (wayland-scroll-factor-git): published as-is (its pkgver() derives the
#   version at build time on the user's machine); kept current so a PKGBUILD
#   change reaches the AUR too. A no-op push when nothing changed.
#
# All writes happen inside the AUR clone under $HOME (builder-owned); the
# checked-out repo at $REPO_ROOT is only ever read, so the runner's post-job
# git cleanup keeps working on it.
#
# The AUR packages must already exist and the deploy key's account must be a
# (co-)maintainer. TAG is provided by the workflow (e.g. v0.3.5).
set -euo pipefail

TAG="${TAG:?TAG (vX.Y.Z) is required}"
VER="${TAG#v}"
REPO_ROOT="${REPO_ROOT:-/work}"
WORK="${WORK:-$HOME/aur-work}"
PROJECT_URL="https://github.com/daniel-g-carrasco/wayland-scroll-factor"

git config --global user.name "wsf release bot"
git config --global user.email "ai@danielgrasso.com"

mkdir -p "$WORK"
cd "$WORK"

# Publish one AUR package: clone (or init if it does not exist yet), copy the
# packaging files in, optionally pin the version from the release tarball,
# regenerate .SRCINFO, and commit + push only when something actually changed.
publish() {
	local aur_name="$1" src_dir="$2" pin_version="${3:-}"
	local remote="ssh://aur@aur.archlinux.org/${aur_name}.git"

	echo "==> ${aur_name}"
	rm -rf "$aur_name"
	if ! git clone "$remote" "$aur_name" 2>/dev/null; then
		echo "    repo not on AUR yet — initialising a new one"
		mkdir -p "$aur_name"
		git -C "$aur_name" init -q
		git -C "$aur_name" remote add origin "$remote"
	fi

	cp "$REPO_ROOT/$src_dir/PKGBUILD" "$aur_name/PKGBUILD"
	if [ -f "$REPO_ROOT/$src_dir/wayland-scroll-factor.install" ]; then
		cp "$REPO_ROOT/$src_dir/wayland-scroll-factor.install" "$aur_name/"
	fi

	# Pin the stable package's version + checksum from the release tarball.
	# Done on the clone's copy so $REPO_ROOT stays read-only.
	if [ -n "$pin_version" ]; then
		local url sha
		url="${PROJECT_URL}/archive/refs/tags/v${pin_version}.tar.gz"
		echo "    hashing release tarball: ${url}"
		sha="$(curl -fsSL "$url" | sha256sum | cut -d' ' -f1)"
		[ -n "$sha" ] || { echo "failed to hash release tarball" >&2; return 1; }
		sed -i \
			-e "s/^pkgver=.*/pkgver=${pin_version}/" \
			-e "s/^pkgrel=.*/pkgrel=1/" \
			-e "s/^sha256sums=.*/sha256sums=('${sha}')/" \
			"$aur_name/PKGBUILD"
	fi

	( cd "$aur_name" && makepkg --printsrcinfo > .SRCINFO )

	git -C "$aur_name" add -A
	if git -C "$aur_name" diff --cached --quiet; then
		echo "    no changes — nothing to push"
		return 0
	fi
	git -C "$aur_name" commit -q -m "Update to ${VER}"
	git -C "$aur_name" push -u origin HEAD:master
	echo "    pushed ${aur_name} @ ${VER}"
}

# stable: pinned to the tag's release tarball.
publish "wayland-scroll-factor" "packaging/aur-stable" "$VER"

# -git: published as-is (version is computed at build time).
publish "wayland-scroll-factor-git" "packaging/aur"

echo "AUR publish complete for ${TAG}"
