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
  install_dir : 'include/libgcab-1.0/libgcab',
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
  soversion : lt_current,
  version : lt_version,
  dependencies : [
    gio,
    libz,
  ],
  c_args : cargs,
  include_directories : [
    include_directories('.'),
    include_directories('..'),
  ],
  link_args : vflag,
  link_depends : mapfile,
  install : true
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
    identifier_prefix : 'GCab',
    export_packages : 'libgcab',
    dependencies : [
      gio,
    ],
    includes : [
      'Gio-2.0',
      'GObject-2.0',
    ],
    install : true
  )
  gnome.generate_vapi('libgcab-1.0',
    sources: gir[0],
    packages : ['gio-2.0'],
    install : true,
  )
endif
