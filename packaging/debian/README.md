# Debian Packaging

The `debian/` directory here is a template for Debian, Ubuntu, and PPA builds.
It is kept under `packaging/debian/` so the upstream source tree remains clean.

Local test:

```bash
scripts/test-packages.sh deb
```

The test copies the repository into a disposable Debian container, overlays this
`debian/` directory at the source root, and runs `dpkg-buildpackage -us -uc -b`.

Before submitting to Debian mentors or a PPA, review the package with the normal
Debian tooling and policies. This template is a starting point, not a Debian
review approval.
