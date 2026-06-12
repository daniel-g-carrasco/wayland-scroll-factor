shared_library(
  'wsf_preload',
  ['wsf_preload.c', 'wsf_config.c', 'wsf_proc.c'],
  name_prefix: 'lib',
  install: true,
  install_dir: wsf_libdir,
  # Only the six interposed libinput_* wrappers need to be visible; the
  # helper functions are an implementation detail of a library loaded
  # into the compositor, so hide everything not explicitly exported.
  gnu_symbol_visibility: 'hidden',
  # threads_dep: pthread_once in the interposer (links libpthread on
  # toolchains where it is not folded into libc).
  dependencies: [dl_dep, threads_dep]
)
