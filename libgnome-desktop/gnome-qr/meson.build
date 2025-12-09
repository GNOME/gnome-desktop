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

libgnome_qr = library('gnome-qr-4',
  sources: libgnome_qr_sources,
  dependencies: libgnome_qr_deps,
  soversion: soversion,
  version: libversion,
  c_args: libargs,
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
  requires: ['gsettings-desktop-schemas'],
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
