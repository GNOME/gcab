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
)
test('gcab-self-test', e)
