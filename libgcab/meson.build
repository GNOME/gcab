cargs = [
  '-DG_LOG_DOMAIN="gcab"',
]

enum_headers = ['gcab-folder.h', 'gcab-file.h']
enums = gnome.mkenums(
  'gcab-enums',
  sources : enum_headers,
  c_template : 'gcab-enums.c.etemplate',
  h_template : 'gcab-enums.h.etemplate',
  install_header : true,
  install_dir : join_paths(get_option('includedir'), 'libgcab-1.0/libgcab'),
  symbol_prefix : 'gcab',
  identifier_prefix : 'GCab',
)

install_headers(
  'libgcab.h',
  subdir : 'libgcab-1.0',
)

install_headers([
    'gcab-cabinet.h',
    'gcab-file.h',
    'gcab-folder.h',
  ],
  subdir : 'libgcab-1.0/libgcab',
)

mapfile = 'libgcab.syms'
vflag = '-Wl,--version-script,@0@/@1@'.format(meson.current_source_dir(), mapfile)
libgcab = shared_library(
  'gcab-1.0',
  enums,
  sources : [
    'gcab-cabinet.c',
    'gcab-folder.c',
    'gcab-file.c',
    'cabinet.c',
    'decomp.c',
  ],
  version : lt_version,
  darwin_versions: darwin_versions,
  dependencies : [
    gio,
    libz,
  ],
  c_args : cargs,
  include_directories : [
    include_directories('.'),
    include_directories('..'),
  ],
  link_args : cc.get_supported_link_arguments([vflag]),
  link_depends : mapfile,
  install : true
)

gcab_dep = declare_dependency(
  link_with : libgcab,
  include_directories : [
    include_directories('.'),
    include_directories('..'),
  ],
  dependencies : gcab_deps
)

pkgg = import('pkgconfig')
pkgg.generate(
  libraries : libgcab,
  requires : [ 'gio-2.0' ],
  subdirs : 'libgcab-1.0',
  version : meson.project_version(),
  name : 'libgcab',
  filebase : 'libgcab-1.0',
  description : 'Cabinet file library',
)

if get_option('introspection')
  gir = gnome.generate_gir(libgcab,
    sources : [
      enums,
      'gcab-cabinet.c',
      'gcab-cabinet.h',
      'gcab-folder.c',
      'gcab-folder.h',
      'gcab-file.c',
      'gcab-file.h',
    ],
    nsversion : '1.0',
    namespace : 'GCab',
    symbol_prefix : 'gcab',
    header : 'libgcab.h',
    identifier_prefix : 'GCab',
    export_packages : 'libgcab-1.0',
    dependencies : [
      gio,
    ],
    includes : [
      'GLib-2.0',
      'Gio-2.0',
      'GObject-2.0',
    ],
    install : true
  )
  if get_option('vapi')
    gnome.generate_vapi('libgcab-1.0',
      sources: gir[0],
      packages : ['gio-2.0'],
      install : true,
    )
  endif
endif
