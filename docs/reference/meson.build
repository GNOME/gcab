gcab_docs = configure_file(
  input : 'gcab-docs.sgml.in',
  output : 'gcab-docs.sgml',
  configuration : conf,
)

gnome.gtkdoc(
  'gcab',
  src_dir : [
    join_paths(meson.source_root(), 'libgcab'),
    join_paths(meson.build_root(), 'libgcab'),
  ],
  main_sgml : 'gcab-docs.sgml',
  install : true
)
