project(
  'wayland-scroll-factor',
  'c',
  version: '0.2.2',
  default_options: [
    'warning_level=2',
    'c_std=c11'
  ]
)

cc = meson.get_compiler('c')
dl_dep = cc.find_library('dl', required: true)
wsf_libdir = join_paths(get_option('prefix'), get_option('libdir'), 'wayland-scroll-factor')
wsf_libdir_define = '-DWSF_LIBDIR="' + wsf_libdir + '"'

subdir('src')
subdir('tools')
subdir('gui')
subdir('data')
