project('gcab', 'c',
  version : '1.6',
  license : 'LGPL-2.1+',
  meson_version : '>=0.50.0',
  default_options : ['warning_level=2', 'c_std=c99'],
)

git_version = []
git = find_program('git', required: false)
if git.found()
  git_version = run_command(git, 'describe', '--abbrev=4', '--dirty', check: false).stdout().strip().split('-')
endif

# libtool versioning
lt_current = 3
lt_revision = 0
lt_age = 3
lt_version = '@0@.@1@.@2@'.format(lt_current - lt_age, lt_age, lt_revision)
darwin_versions = [lt_current + 1, '@0@.@1@.0'.format(lt_current + 1, lt_revision)]

# get suported warning flags
test_args = [
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

if not meson.is_cross_build()
  add_project_arguments('-fstack-protector-strong', language : 'c')
endif

# enable full RELRO where possible
# FIXME: until https://github.com/mesonbuild/meson/issues/1140 is fixed
global_link_args = []
test_link_args = [
  '-Wl,-z,relro',
  '-Wl,-z,now',
]
foreach arg: test_link_args
  if cc.has_link_argument(arg)
    global_link_args += arg
  endif
endforeach
add_project_link_arguments(
  global_link_args,
  language: 'c'
)

gio = dependency('gio-2.0', version : '>= 2.62.0')
libz = dependency('zlib')

gcab_deps = [
  gio,
  libz,
]

gnome = import('gnome')
i18n = import('i18n')

if git_version.length() >= 3
  package_string = '@0@ @1@.@2@-@3@'.format(meson.project_name(), git_version[0], git_version[1], git_version[2])
  if git_version.length() == 4
    package_string = '@0@-dirty'.format(package_string)
  endif
else
  package_string = '@0@ v@1@'.format(meson.project_name(), meson.project_version())
endif

conf = configuration_data()
conf.set_quoted('GETTEXT_PACKAGE', meson.project_name())
conf.set_quoted('PACKAGE_NAME', meson.project_name())
conf.set_quoted('PACKAGE_STRING', package_string)
conf.set_quoted('LOCALEDIR', join_paths(get_option('prefix'), get_option('localedir')))
conf.set_quoted('PACKAGE_BUGREPORT', 'https://gitlab.gnome.org/GNOME/gcab/issues/new')
configure_file(
  output : 'config.h',
  configuration : conf
)

subdir('libgcab')
subdir('src')

if get_option('nls')
subdir('po')
endif

if get_option('docs')
subdir('docs')
endif

if get_option('tests')
subdir('tests')
endif
