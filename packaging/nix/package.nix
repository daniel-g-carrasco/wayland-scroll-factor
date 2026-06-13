{ lib
, stdenv
, meson
, ninja
, pkg-config
, wrapGAppsHook4
, gobject-introspection
, python3
, gtk4
, libadwaita
, libinput
, glib
, hyprland
, withHyprland ? false
, src
, version
}:

let
  # Used both to satisfy meson's find_program('python3') and so that
  # patchShebangs rewrites wsf-gui's `#!/usr/bin/env python3` to an
  # interpreter that can actually `import gi`.
  pythonEnv = python3.withPackages (ps: [ ps.pygobject3 ]);
in
stdenv.mkDerivation {
  pname = "wayland-scroll-factor";
  inherit version src;

  postPatch = ''
    substituteInPlace gui/wsf_gui.py \
      --replace-fail '#!/usr/bin/env python3' '#!${pythonEnv}/bin/python3'
  '';

  nativeBuildInputs = [
    meson
    ninja
    pkg-config
    wrapGAppsHook4
    gobject-introspection
    pythonEnv
  ];

  buildInputs = [
    gtk4
    libadwaita
    libinput
    glib
  ];

  # wrapGAppsHook4 already wraps everything in $out/bin during fixup, so
  # extra runtime PATH entries go through gappsWrapperArgs instead of a
  # second wrapProgram layer. Hyprland is opt-in: on a GNOME system it would
  # otherwise drag the entire Hyprland closure in just for hyprctl.
  preFixup = lib.optionalString withHyprland ''
    gappsWrapperArgs+=(--prefix PATH : ${lib.makeBinPath [ hyprland ]})
  '';

  meta = with lib; {
    description = "Tune touchpad scroll and gesture feel on Wayland";
    homepage = "https://github.com/daniel-g-carrasco/wayland-scroll-factor";
    license = licenses.mit;
    platforms = platforms.linux;
    mainProgram = "wsf";
  };
}
