wsf_inc = include_directories('../src')

executable(
  'wsf',
  ['wsf.c', '../src/wsf_config.c', '../src/wsf_proc.c'],
  include_directories: wsf_inc,
  dependencies: [dl_dep],
  c_args: [wsf_libdir_define],
  install: true,
  install_dir: join_paths(get_option('prefix'), get_option('bindir'))
)

hyprland_shim_conf = configuration_data()
hyprland_shim_conf.set('WSF_LIBDIR', wsf_libdir)
hyprland_shim_conf.set('WSF_BINDIR', join_paths(get_option('prefix'), get_option('bindir')))

configure_file(
  input: 'wsf-hyprland.in',
  output: 'wsf-hyprland',
  configuration: hyprland_shim_conf,
  install: true,
  install_dir: join_paths(get_option('prefix'), get_option('bindir')),
  install_mode: 'rwxr-xr-x'
)

configure_file(
  input: 'wsf-session-wrapper.in',
  output: 'wsf-session-wrapper',
  configuration: hyprland_shim_conf,
  install: true,
  install_dir: join_paths(get_option('prefix'), get_option('bindir')),
  install_mode: 'rwxr-xr-x'
)

configure_file(
  input: 'wsf-start-hyprland.in',
  output: 'wsf-start-hyprland',
  configuration: hyprland_shim_conf,
  install: true,
  install_dir: join_paths(get_option('prefix'), get_option('bindir')),
  install_mode: 'rwxr-xr-x'
)
