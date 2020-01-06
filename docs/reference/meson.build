glib_prefix = dependency('glib-2.0').get_pkgconfig_variable('prefix')
glib_docpath = glib_prefix / 'share' / 'gtk-doc' / 'html'

datadir = get_option('prefix') / get_option('datadir')
docpath = datadir / 'gcab' / 'html'

gnome.gtkdoc(
  'gcab',
  src_dir : [
    join_paths(meson.source_root(), 'libgcab'),
    join_paths(meson.build_root(), 'libgcab'),
  ],
  ignore_headers : ['decomp.h', 'cabinet.h', 'gcab-priv.h'],
  main_sgml : 'gcab-docs.sgml',
  fixxref_args: [
    '--html-dir=@0@'.format(docpath),
    '--extra-dir=@0@'.format(glib_docpath / 'glib'),
    '--extra-dir=@0@'.format(glib_docpath / 'gobject'),
    '--extra-dir=@0@'.format(glib_docpath / 'gio'),
  ],
  install : true
)
