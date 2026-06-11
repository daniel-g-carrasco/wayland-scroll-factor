# Packaging

This directory contains packaging templates and local publishing notes.

Tracked files:

- `aur/`: Arch AUR `wayland-scroll-factor-git` template.
- `aur-stable/`: Arch AUR `wayland-scroll-factor` stable template.
- `rpm/`: RPM spec for Fedora COPR/openSUSE OBS experiments.
- `debian/`: Debian packaging template for Debian/Ubuntu/PPA experiments.

Ignored local worktrees:

- `aur-worktrees/`: local clones of the live AUR repositories. These are nested
  Git repositories and are intentionally ignored by the main WSF repository.

The first publication target is community packaging:

- Arch AUR: already active.
- Fedora COPR: use `packaging/rpm/wayland-scroll-factor.spec`.
- openSUSE OBS: start from the same RPM spec, then adapt if OBS requires distro
  conditionals.
- Ubuntu PPA / Debian tests: use the `packaging/debian/debian/` template.

Official distro repositories require their own review or sponsorship process.
Do not treat these templates as accepted distro policy until each distro has
reviewed them.
