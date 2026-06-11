# Packaging And Distribution

This project currently prioritizes native distro packages over Flatpak. WSF
installs a CLI, a preload library, compositor/session helpers, desktop metadata,
and optional Hyprland session integration. Those pieces need host-level package
integration, so a full Flatpak is not a good fit for the core tool.

## Current Targets

- Arch Linux: AUR stable package `wayland-scroll-factor`.
- Arch Linux: AUR development package `wayland-scroll-factor-git`.
- Fedora: COPR/RPM packaging from `packaging/rpm/`.
- openSUSE: OBS/RPM packaging from `packaging/rpm/`, with distro-specific
  adjustments if OBS requires them.
- Debian/Ubuntu: Debian packaging template from `packaging/debian/`.
- Ubuntu: PPA can be built from the Debian packaging template.

## Publication Status

Currently published:

- AUR stable: `wayland-scroll-factor`.
- AUR development: `wayland-scroll-factor-git`.
- GitHub Actions package artifacts for RPM and Debian builds.

Prepared but not yet published to a distro package repository:

- Fedora COPR.
- openSUSE OBS.
- Ubuntu PPA.
- Debian official repositories.
- Fedora official repositories.
- openSUSE official repositories.

Publishing to COPR, OBS, or a PPA requires project/account credentials that
should be stored as repository secrets before adding automated upload jobs.
Publishing to official distro repositories requires each distro's maintainer or
sponsorship review process.

## Local Package Tests

Use Podman from the repository root:

```bash
scripts/test-packages.sh rpm
scripts/test-packages.sh deb
scripts/test-packages.sh all
```

The tests build packages in disposable containers and do not install anything
on the host.

The same package tests run in GitHub Actions on every push and pull request.
The workflow uploads RPM and Debian build outputs as temporary artifacts, so
you can inspect packages without rebuilding them on your laptop.

Fedora COPR publishing is documented in [`copr.md`](copr.md). The repository
includes a manual GitHub Actions workflow that submits the generated source RPM
to COPR after a `COPR_CONFIG` secret is configured.

## AUR Publishing Worktrees

The live AUR repositories are separate Git repositories. Keep local clones under
the ignored directory:

```text
packaging/aur-worktrees/
```

Tracked AUR templates remain in:

```text
packaging/aur/
packaging/aur-stable/
```

This keeps publication work close to the project without committing nested Git
repositories into the source tree.

## Repository Publication Path

Recommended order:

1. Keep AUR stable and AUR git updated.
2. Publish a COPR repository using the RPM spec.
3. Publish an OBS package using the RPM spec as the starting point.
4. Publish an Ubuntu PPA using the Debian packaging template.
5. Once packaging has real users, start official repository review processes.

Official distro repositories have maintainership and review requirements. Treat
the files under `packaging/` as packaging templates until each distro accepts
them through its own process.

## Release Checklist For Packagers

- Tag a release in Git.
- Update `CHANGELOG.md`.
- Update AppStream release metadata.
- Update AUR stable `pkgver`, checksum, and `.SRCINFO`.
- Run `scripts/test-containers.sh all`.
- Run `scripts/test-packages.sh all`.
- Publish GitHub release artifacts if desired.
