project('qrcodegen', 'c')

libqrcodegen = static_library('qrcodegen',
  'c/qrcodegen.c',
  include_directories: include_directories('c')
)
qrcodegen_dep = declare_dependency(
  link_with: libqrcodegen,
  include_directories: include_directories('c')
)
