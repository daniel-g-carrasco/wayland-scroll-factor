# Release Checklist

WSF is still in testing. Before tagging a public release, run a short validation
pass on each supported backend.

## Build

```bash
meson setup build --prefix="$HOME/.local" --buildtype=release
meson compile -C build
python3 -m py_compile gui/wsf_gui.py
```

## GitHub Actions

Every push and pull request runs:

- source build and metadata validation;
- container smoke tests for Fedora, Ubuntu, Debian, Arch, and openSUSE;
- RPM and Debian package builds.

Use these workflows as the first validation gate. Local container tests remain
useful before a release tag or when debugging a distro-specific failure.

COPR publication is manual:

```text
Actions -> Publish COPR -> Run workflow
```

It requires the `COPR_CONFIG` repository secret described in `docs/copr.md`.

## Container Smoke Tests

Run the build/install smoke matrix when Podman is available:

```bash
scripts/test-containers.sh all
```

These tests cover distro dependency names, Meson build/install, CLI JSON
output, Python GUI syntax, and installed desktop/metainfo validation. They do
not replace real GNOME/Hyprland runtime tests.

The scripts use Podman by default when available and fall back to Docker. To
select an engine explicitly:

```bash
CONTAINER_ENGINE=docker scripts/test-containers.sh fedora
CONTAINER_ENGINE=podman scripts/test-containers.sh fedora
```

## Package Smoke Tests

Run package build tests when Podman is available:

```bash
scripts/test-packages.sh all
```

This builds RPM and Debian packages inside disposable containers. It does not
install packages on the host.

To copy generated packages out of the disposable container:

```bash
WSF_PACKAGE_OUTDIR="$PWD/package-artifacts" scripts/test-packages.sh all
```

## GNOME Wayland

- Install with `./scripts/install.sh`.
- Run `wsf enable`.
- Log out and log back in.
- Confirm `wsf doctor` reports `gnome-shell library mapped: yes`.
- Set extreme values such as `0.05` and `5.0`.
- Verify two-finger vertical and horizontal scroll changes in Files, Settings,
  and a browser.
- Verify pinch zoom/rotate in apps that expose those gestures.
- Confirm mouse wheel scrolling is not scaled.

## Hyprland

- Confirm `hyprctl getoption input:touchpad:scroll_factor` works.
- Run `wsf set 0.35` and verify the live Hyprland scroll factor changes.
- On Hyprland 0.55+ Lua configs, confirm `wsf doctor` reports
  `hyprland scroll apply method: lua-eval` and `wsf apply` changes
  `input:touchpad:scroll_factor` through the eval backend.
- Verify vertical and horizontal GUI sliders stay synchronized, because
  Hyprland exposes one native touchpad scroll factor.
- Launch Hyprland through:

```bash
start-hyprland --path "$(command -v wsf-hyprland)" -- --config ~/.config/hypr/hyprland.conf
```

- Confirm `wsf doctor` reports:

```text
hyprland gesture preload: active
runtime gesture reload: active via Hyprland gestures-only preload
```

- Verify pinch zoom and pinch rotate react to GUI/CLI changes without restarting
  the compositor.
- Confirm child apps do not inherit WSF in `LD_PRELOAD`.

## Versioning

- Use a new minor version when adding or changing compositor backends.
- Use a patch version for compatibility fixes that do not change user-facing
  behavior.
- For the Hyprland backend branch, the next public testing release should be a
  new minor release rather than another `0.2.x` patch.
