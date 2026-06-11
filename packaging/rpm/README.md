# RPM Packaging

`wayland-scroll-factor.spec` is intended as a starting point for Fedora COPR and
openSUSE OBS.

Local test:

```bash
scripts/test-packages.sh rpm
```

The test builds from the current repository checkout inside a disposable Fedora
container. It does not install anything on the host.

For COPR, create a source RPM from a release tarball and submit that SRPM. For
openSUSE OBS, import the spec and release tarball, then adjust package names if
OBS reports distro-specific dependency differences.
