{
  description = "Tune touchpad scroll and gesture feel on Wayland";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    let
      # Single source of truth: parse `version: '...'` out of meson.build.
      version = builtins.head (builtins.match
        ".*version: '([^']+)'.*"
        (builtins.readFile ./meson.build));

      module = ./packaging/nix/module.nix;

      systemAgnostic = {
        nixosModules = rec {
          default = { pkgs, lib, ... }: {
            imports = [ module ];
            programs.wsf.package = lib.mkDefault
              self.packages.${pkgs.stdenv.hostPlatform.system}.default;
          };
          wayland-scroll-factor = default;
        };

        overlays.default = final: prev: {
          wayland-scroll-factor = final.callPackage ./packaging/nix/package.nix {
            src = self;
            inherit version;
          };
        };
      };

      perSystem = flake-utils.lib.eachSystem [ "x86_64-linux" "aarch64-linux" ] (system:
        let
          pkgs = import nixpkgs { inherit system; };
          wsf = pkgs.callPackage ./packaging/nix/package.nix {
            src = self;
            inherit version;
          };
        in {
          packages.default = wsf;
          packages.wayland-scroll-factor = wsf;

          devShells.default = pkgs.mkShell {
            inputsFrom = [ wsf ];
          };
        });
    in
      systemAgnostic // perSystem;
}
