libgnome_qr_deps = [
  glib_dep,
  gio_dep,
  qrcodegen_dep,
  m_dep,
]

libgnome_qr_sources = files([
  'gnome-qr.c',
  'gnome-qr.h',
])

libgnome_qr_headers = files([
  'gnome-qr.h',
])

install_headers(libgnome_qr_headers,
  subdir: 'gnome-desktop-4.0/gnome-qr'
)

gnome_qr_ld_flags = []
gnome_qr_symbol_map = 'gnome-qr-symbols.map'
gnome_qr_symbol_map_arg = '-Wl,--version-script=@0@'.format(
  meson.current_source_dir() / gnome_qr_symbol_map)
if cc.has_link_argument(gnome_qr_symbol_map_arg)
  gnome_qr_ld_flags += gnome_qr_symbol_map_arg
endif

libgnome_qr = library('gnome-qr-4',
  sources: libgnome_qr_sources,
  dependencies: libgnome_qr_deps,
  soversion: qr_soversion,
  version: qr_libversion,
  c_args: libargs,
  link_args: gnome_qr_ld_flags,
  link_depends: files(gnome_qr_symbol_map),
  install: true,
  include_directories: [
    include_directories('.'),
    include_directories('..'),
  ],
)

libgnome_qr_gir = []
if get_option('introspection')
  libgnome_qr_gir += gnome.generate_gir(libgnome_qr,
    sources: [libgnome_qr_headers, libgnome_qr_sources],
    export_packages: 'gnome-qr-4',
    namespace: 'GnomeQR',
    nsversion: '4.0',
    identifier_prefix: 'GnomeQr',
    symbol_prefix: 'gnome_qr',
    includes: ['Gio-2.0', 'GLib-2.0'],
    extra_args: ['--quiet', '--warn-all'],
    fatal_warnings: true,
    install: true,
  )
endif

pkg.generate(
  libgnome_qr,
  requires: ['gio-2.0'],
  version: meson.project_version(),
  name: 'gnome-qr-4',
  filebase: 'gnome-qr-4',
  description: 'QR code utility library for GNOME desktop components',
  subdirs: 'gnome-desktop-4.0',
)

gnome_qr_dep = declare_dependency(
  dependencies: libgnome_qr_deps,
  sources: [
    libgnome_qr_gir,
  ],
  include_directories: [
    include_directories('..'),
  ],
  link_with: libgnome_qr,
)
