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
    'test-mszip.cab',
    'test-ncbytes-overflow.cab',
    'test-none.cab',
    'test-signed.cab',
    install_dir : installed_tests_execdir,
  )
  install_data(
    'src/test.sh',
    'src/test.txt',
    install_dir : join_paths(installed_tests_execdir, 'src'),
  )

  test_conf = configuration_data()
  test_conf.set('installed_tests_dir', abs_installed_tests_execdir)
  test_conf.set('program', 'gcab-self-test')
  test_conf.set('env', '')
  configure_file(
    input : installed_tests_template_tap,
    output : 'gcab-self-test.test',
    install_dir : installed_tests_metadir,
    configuration : test_conf,
  )
endif

e = executable(
  'gcab-self-test',
  sources : [
    'gcab-self-test.c',
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
