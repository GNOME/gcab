gidocgen_dep = dependency('gi-docgen',
  version: '>= 2021.1',
  fallback: ['gi-docgen', 'dummy_dep'],
  native: true,
)

gidocgen = find_program('gi-docgen')

toml_conf = configuration_data()
toml_conf.set('version', meson.project_version())

gcab_toml = configure_file(
  input: 'GCab-1.0.toml',
  output: 'GCab-1.0.toml',
  configuration: toml_conf,
)

custom_target('gcab-doc',
  input: [gcab_toml, gir[0]],
  output: 'GCab-1.0',
  command: [
    gidocgen,
    'generate',
    '--quiet',
    '--no-namespace-dir',
    '--config=@INPUT0@',
    '--output-dir=@OUTPUT@',
    '--content-dir=@0@'.format(meson.current_source_dir()),
    '@INPUT1@',
  ],
  build_by_default: true,
  install: true,
  install_dir: get_option('datadir') / 'doc',
)
