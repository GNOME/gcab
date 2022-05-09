configure_file(
  input : 'gcab.1.in',
  output : 'gcab.1',
  configuration : conf,
  install : true,
  install_dir : join_paths(get_option('mandir'), 'man1'),
)

gcab = executable(
  'gcab',
  sources : [
    'gcab.c',
  ],
  include_directories : [
    include_directories('..'),
    include_directories('../libgcab'),
  ],
  dependencies : [
    gio,
  ],
  link_with : [
    libgcab,
  ],
  c_args : cargs,
  install : true,
  install_dir : get_option('bindir')
)
meson.override_find_program('gcab', gcab)

executable(
  'gcab-fuzz',
  sources : [
    'gcab-fuzz.c',
  ],
  include_directories : [
    include_directories('..'),
    include_directories('../libgcab'),
  ],
  dependencies : [
    gio,
  ],
  link_with : [
    libgcab,
  ],
  c_args : cargs,
)

executable(
  'gcab-bzero',
  sources : [
    'gcab-bzero.c',
  ],
  include_directories : [
    include_directories('..'),
    include_directories('../libgcab'),
  ],
  dependencies : [
    gio,
  ],
  link_with : [
    libgcab,
  ],
  c_args : cargs,
)
