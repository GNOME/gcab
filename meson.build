project('gcab', 'c',
  version : '1.1',
  license : 'LGPL-2.1+',
  meson_version : '>=0.37.0',
  default_options : ['warning_level=2', 'c_std=c99'],
)

git_version = run_command(['git', 'describe', '--abbrev=4', 'HEAD']).stdout().strip().split('-')

# libtool versioning
lt_current = '0'
lt_revision = '0'
lt_age = '0'
lt_version = '@0@.@1@.@2@'.format(lt_current, lt_age, lt_revision)

# get suported warning flags
test_args = [
  '-fstack-protector-strong',
  '-Waggregate-return',
  '-Wunused',
  '-Warray-bounds',
  '-Wcast-align',
  '-Wclobbered',
  '-Wduplicated-branches',
  '-Wduplicated-cond',
  '-Wempty-body',
  '-Wformat=2',
  '-Wformat-nonliteral',
  '-Wformat-security',
  '-Wformat-signedness',
  '-Wignored-qualifiers',
  '-Wimplicit-function-declaration',
  '-Winit-self',
  '-Wlogical-op',
  '-Wmissing-declarations',
  '-Wmissing-format-attribute',
  '-Wmissing-include-dirs',
  '-Wmissing-noreturn',
  '-Wmissing-parameter-type',
  '-Wmissing-prototypes',
  '-Wnested-externs',
  '-Wno-discarded-qualifiers',
  '-Wno-missing-field-initializers',
  '-Wno-strict-aliasing',
  '-Wno-suggest-attribute=format',
  '-Wno-unused-parameter',
  '-Wnull-dereference',
  '-Wold-style-definition',
  '-Woverride-init',
  '-Wpointer-arith',
  '-Wredundant-decls',
  '-Wreturn-type',
  '-Wshadow',
  '-Wsign-compare',
  '-Wstrict-aliasing',
  '-Wstrict-prototypes',
  '-Wswitch-default',
  '-Wtype-limits',
  '-Wundef',
  '-Wuninitialized',
  '-Wunused-but-set-variable',
  '-Wunused-variable',
  '-Wwrite-strings'
]
cc = meson.get_compiler('c')
foreach arg: test_args
  if cc.has_argument(arg)
    add_project_arguments(arg, language : 'c')
  endif
endforeach

# enable full RELRO where possible
# FIXME: until https://github.com/mesonbuild/meson/issues/1140 is fixed
global_link_args = []
test_link_args = [
  '-Wl,-z,relro',
  '-Wl,-z,now',
]
foreach arg: test_link_args
  if cc.has_argument(arg)
    global_link_args += arg
  endif
endforeach
add_global_link_arguments(
  global_link_args,
  language: 'c'
)

gio = dependency('gio-2.0', version : '>= 2.44.0')
libz = dependency('zlib')

gnome = import('gnome')
i18n = import('i18n')

conf = configuration_data()
conf.set_quoted('GETTEXT_PACKAGE', meson.project_name())
conf.set_quoted('PACKAGE_NAME', meson.project_name())
if git_version.length() == 3
  conf.set_quoted('PACKAGE_STRING', '@0@ @1@.@2@-@3@'.format(meson.project_name(), git_version[0], git_version[1], git_version[2]))
else
  conf.set_quoted('PACKAGE_STRING', '@0@ v@1@'.format(meson.project_name(), meson.project_version()))
endif

conf.set_quoted('LOCALEDIR', join_paths(get_option('prefix'), get_option('localedir')))
conf.set_quoted('PACKAGE_BUGREPORT', 'https://bugzilla.gnome.org/enter_bug.cgi?product=msitools&component=gcab')
conf.set_quoted('TESTDATADIR', join_paths(meson.source_root(), 'tests'))
configure_file(
  output : 'config.h',
  configuration : conf
)

subdir('libgcab')
subdir('po')
subdir('src')

if get_option('docs')
subdir('docs')
endif

if get_option('tests')
subdir('tests')
endif
