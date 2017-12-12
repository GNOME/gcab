configure_file(
  input : 'gcab.1.in',
  output : 'gcab.1',
  configuration : conf,
  install : true,
  install_dir : join_paths(get_option('mandir'), 'man1'),
)

executable(
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