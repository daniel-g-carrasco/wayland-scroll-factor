project(
  'wayland-scroll-factor',
  'c',
  version: '0.3.4',
  default_options: [
    'warning_level=2',
    'c_std=c11',
    # This project ships a library injected into gnome-shell via
    # LD_PRELOAD, so build hardening is cheap insurance worth enabling
    # by default (distro packaging usually adds these; do it ourselves
    # too). b_pie + RELRO/BIND_NOW are set below once they're known to
    # be supported by the active compiler/linker.
    'b_pie=true'
  ]
)

cc = meson.get_compiler('c')
dl_dep = cc.find_library('dl', required: true)
threads_dep = dependency('threads')

# Hardening flags, filtered to what the compiler actually accepts so the
# build still works on minimal toolchains.
hardening_c_args = cc.get_supported_arguments(
  '-fstack-protector-strong',
  '-D_FORTIFY_SOURCE=2',
  '-fstack-clash-protection'
)
hardening_link_args = cc.get_supported_link_arguments(
  '-Wl,-z,relro',
  '-Wl,-z,now'
)
add_project_arguments(hardening_c_args, language: 'c')
add_project_link_arguments(hardening_link_args, language: 'c')

wsf_libdir = join_paths(get_option('prefix'), get_option('libdir'), 'wayland-scroll-factor')
wsf_libdir_define = '-DWSF_LIBDIR="' + wsf_libdir + '"'

subdir('src')
subdir('tools')
subdir('gui')
subdir('data')
