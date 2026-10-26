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

if get_option('libfuzzer')
  fuzzing_engine = get_option('fuzzing-engine')
  if fuzzing_engine == ''
    if not cc.has_argument('-fsanitize=fuzzer')
      error('libFuzzer is not supported by this compiler; use -Dfuzzing-engine with an OSS-Fuzz engine')
    endif
    fuzzing_engine = '-fsanitize=fuzzer'
  endif

  executable(
    'gcab-fuzz-libfuzzer',
    sources : [
      'gcab-fuzz-libfuzzer.c',
    ],
    include_directories : [
      include_directories('..'),
      include_directories('../libgcab'),
    ],
    dependencies : [
      gio,
      libz,
    ],
    link_with : [
      libgcab_fuzz,
    ],
    c_args : cargs + fuzzing_cargs,
    link_args : fuzzing_engine.split(),
  )
endif

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

if get_option('tests')
  cli_test_env = environment()
  cli_test_env.set('GCAB', gcab.full_path())
  test('gcab-cli',
    find_program('sh'),
    args : [files('../tests/test-cli.sh')],
    env : cli_test_env,
    depends : gcab,
  )
endif
