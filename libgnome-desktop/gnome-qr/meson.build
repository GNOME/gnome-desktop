libgnome_qr_deps = [
  glib_dep,
  gio_dep,
  qrcodegen_dep,
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

libgnome_qr = library('gnome-qr',
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

pkg.generate(
  libgnome_qr,
  requires: ['gsettings-desktop-schemas'],
  version: meson.project_version(),
  name: 'gnome-qr-4',
  filebase: 'gnome-qr-4',
  description: 'QR code utility library for GNOME desktop components',
  subdirs: 'gnome-desktop-4.0',
)
