installed_tests_metadir = join_paths(get_option('datadir'), 'installed-tests', 'libgcab-1.0')
installed_tests_execdir = join_paths(get_option('libexecdir'), 'installed-tests', 'libgcab-1.0')
installed_tests_enabled = get_option('installed_tests')
installed_tests_template_tap = files('template-tap.test.in')
abs_installed_tests_execdir = join_paths(get_option('prefix'), installed_tests_execdir)

if installed_tests_enabled
  install_data(
    'CVE-2014-9556.cab',
    'CVE-2014-9732.cab',
    'CVE-2015-4470.cab',
    'CVE-2015-4471.cab',
    'privkey.pem',
    'test-extract-bomb.cab',
    'test-mszip-short-cdata.cab',
    'test-mszip.cab',
    'test-multicab-a.cab',
    'test-multicab-b.cab',
    'test-multicab-c.cab',
    'test-lzx-frame-wrap.cab',
    'test-lzx-offset-before-data.cab',
    'test-lzx-offset-overflow.cab',
    'test-lzx-overrun.cab',
    'test-lzx-truncated.cab',
    'test-ncbytes-overflow.cab',
    'test-none-size-mismatch.cab',
    'test-none.cab',
    'test-signed.cab',
    install_dir : installed_tests_execdir,
  )
  install_data(
    'src/test.sh',
    'src/test.txt',
    install_dir : join_paths(installed_tests_execdir, 'src'),
  )

  foreach program : ['gcab-self-test', 'gcab-decomp-test']
    test_conf = configuration_data()
    test_conf.set('installed_tests_dir', abs_installed_tests_execdir)
    test_conf.set('program', program)
    test_conf.set('env', '')
    configure_file(
      input : installed_tests_template_tap,
      output : program + '.test',
      install_dir : installed_tests_metadir,
      configuration : test_conf,
    )
  endforeach
endif

e = executable(
  'gcab-self-test',
  sources : [
    'gcab-self-test.c',
    enums[1],
  ],
  include_directories : [
    include_directories('..'),
    include_directories('../libgcab'),
  ],
  c_args : [
    '-D_XOPEN_SOURCE=500',
  ],
  dependencies : [
    gio,
  ],
  link_with : [
    libgcab,
  ],
  install : installed_tests_enabled,
  install_dir : installed_tests_execdir,
)
env = environment()
env.set('G_TEST_SRCDIR', meson.current_source_dir())
env.set('G_TEST_BUILDDIR', meson.current_build_dir())
test('gcab-self-test', e, env : env)

decomp_test = executable(
  'gcab-decomp-test',
  sources : [
    'decomp-test.c',
    '../libgcab/decomp.c',
  ],
  include_directories : [
    include_directories('..'),
    include_directories('../libgcab'),
  ],
  dependencies : [
    dependency('glib-2.0'),
  ],
  install : installed_tests_enabled,
  install_dir : installed_tests_execdir,
)

test('gcab-decomp-test', decomp_test)
