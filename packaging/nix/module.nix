{ config, lib, pkgs, ... }:

let
  cfg = config.programs.wsf;
in
{
  options.programs.wsf = {
    enable = lib.mkEnableOption "wayland-scroll-factor";

    package = lib.mkOption {
      type = lib.types.package;
      default = pkgs.wayland-scroll-factor;
      description = "The wayland-scroll-factor package to use.";
    };

    gnomePreload = lib.mkOption {
      type = lib.types.bool;
      default = true;
      description = ''
        Enable the GNOME Wayland preload backend declaratively.
        Equivalent to running `wsf enable`. Requires logout/login.
      '';
    };
  };

  config = lib.mkIf cfg.enable {
    environment.systemPackages = [ cfg.package ];
    environment.etc."environment.d/wayland-scroll-factor.conf" = lib.mkIf cfg.gnomePreload {
      text = ''
        LD_PRELOAD=${cfg.package}/lib/wayland-scroll-factor/libwsf_preload.so
      '';
    };
  };
}
