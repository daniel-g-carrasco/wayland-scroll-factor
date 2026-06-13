# AUR Publishing

WSF is published to the Arch User Repository as two packages:

- **`wayland-scroll-factor`** — the stable package, built from a release
  tarball. Source: `packaging/aur-stable/PKGBUILD`.
- **`wayland-scroll-factor-git`** — the VCS package, built from `main` (its
  `pkgver()` derives the version at build time). Source: `packaging/aur/PKGBUILD`.

Publishing is automated by `.github/workflows/aur.yml` — the AUR counterpart of
the COPR workflow (see [copr.md](copr.md)), but it runs **automatically on every
clean semver tag** rather than on a manual dispatch.

## How the automation works

- On `git push` of a tag `vX.Y.Z` (no pre-release suffix), the workflow:
  1. downloads the release tarball for that tag and computes its `sha256`,
  2. pins `pkgver`/`pkgrel`/`sha256sums` in `packaging/aur-stable/PKGBUILD`,
  3. regenerates `.SRCINFO` with `makepkg --printsrcinfo`,
  4. pushes the stable package to the AUR,
  5. refreshes `wayland-scroll-factor-git` from `packaging/aur/PKGBUILD` (a
     no-op push when nothing changed).
- Pre-release tags (`v1.2.3-rc1`, `…-beta`, …) are **skipped**.
- `workflow_dispatch` (with a `tag` input) is available to bootstrap or
  re-publish an existing tag if a push failed. It does not change the normal
  automatic-on-tag flow.

Tagging strategy: cut releases with clean `vMAJOR.MINOR.PATCH` tags (the same
tags `release.yml` and the COPR build consume). The version itself lives in
`meson.build`; bump it together with the RPM spec and the Debian changelog.

## One-time setup

1. Create an AUR account and register an SSH **public** key on it
   (`https://aur.archlinux.org` → My Account → SSH Public Key). The account
   must be the maintainer or a co-maintainer of **both** packages.

2. Add the matching **private** key as a repository secret named
   `AUR_SSH_KEY` (Settings → Secrets and variables → Actions). Use a key
   dedicated to CI; it only needs AUR push access.

3. The packages must already exist on the AUR (first submission is manual, or
   the workflow will initialise a new repo on first push if the name is free
   and the account is authorised).

After that, every `vX.Y.Z` tag updates the AUR automatically.
