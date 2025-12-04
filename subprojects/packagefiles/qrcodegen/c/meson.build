libqrcodegen = static_library('qrcodegen',
  'qrcodegen.c',
)

qrcodegen_dep = declare_dependency(
  link_with: libqrcodegen,
  include_directories: include_directories('.')
)

test('qrcodegen',
  executable('qrcodegen-test',
    sources: [
      'qrcodegen.c',
      'qrcodegen-test.c',
    ],
    c_args: ['-DQRCODEGEN_TEST'],
  ),
)
