desktop_conf = configuration_data()
desktop_conf.set('bindir', join_paths(get_option('prefix'), get_option('bindir')))

desktop_file = configure_file(
  input: 'io.github.danielgrasso.WaylandScrollFactor.desktop.in',
  output: 'io.github.danielgrasso.WaylandScrollFactor.desktop',
  configuration: desktop_conf
)

install_data(
  desktop_file,
  install_dir: join_paths(get_option('prefix'), get_option('datadir'), 'applications')
)

hyprland_session_file = configure_file(
  input: 'wayland-scroll-factor-hyprland.desktop.in',
  output: 'wayland-scroll-factor-hyprland.desktop',
  configuration: desktop_conf
)

install_data(
  hyprland_session_file,
  install_dir: join_paths(get_option('prefix'), get_option('datadir'), 'wayland-sessions')
)

install_data(
  'io.github.danielgrasso.WaylandScrollFactor.metainfo.xml',
  install_dir: join_paths(get_option('prefix'), get_option('datadir'), 'metainfo')
)

foreach size : ['128x128', '256x256', '512x512']
  install_data(
    'icons/hicolor/' + size + '/apps/io.github.danielgrasso.WaylandScrollFactor.png',
    install_dir: join_paths(get_option('prefix'), get_option('datadir'), 'icons', 'hicolor', size, 'apps')
  )
endforeach
