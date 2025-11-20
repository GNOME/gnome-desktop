libgnome_qr_gtk_deps = [
  gnome_qr_dep,
  gtk4_dep,
]

libgnome_qr_gtk_sources = files([
  'gnome-qr-widget.c',
  'gnome-qr-widget.h',
])

libgnome_qr_gtk_headers = files([
  'gnome-qr-widget.h',
])

install_headers(libgnome_qr_gtk_headers,
  subdir: 'gnome-desktop-4.0/gnome-qr-gtk'
)

libgnome_qr_gtk = library('gnome-qr-gtk-4',
  sources: libgnome_qr_gtk_sources,
  dependencies: libgnome_qr_gtk_deps,
  soversion: qr_gtk_soversion,
  version: qr_gtk_libversion,
  c_args: libargs,
  install: true,
  include_directories: [
    include_directories('.'),
    include_directories('..'),
  ],
)

libgnome_qr_gtk_gir = []
if get_option('introspection')
  libgnome_qr_gtk_gir += gnome.generate_gir(libgnome_qr_gtk,
    sources: [libgnome_qr_gtk_headers, libgnome_qr_gtk_sources],
    export_packages: 'gnome-qr-gtk-4',
    namespace: 'GnomeQRGtk',
    nsversion: '4.0',
    identifier_prefix: 'GnomeQr',
    symbol_prefix: 'gnome_qr',
    includes: [libgnome_qr_gir[0], 'Gtk-4.0'],
    extra_args: ['--quiet', '--warn-all'],
    fatal_warnings: true,
    install: true,
  )
endif

pkg.generate(
  libgnome_qr_gtk,
  requires: ['gnome-qr-4', 'gtk4'],
  version: meson.project_version(),
  name: 'gnome-qr-gtk-4',
  filebase: 'gnome-qr-gtk-4',
  description: 'QR code GTK widget library for GNOME desktop components',
  subdirs: 'gnome-desktop-4.0',
)

gnome_qr_gtk_dep = declare_dependency(
  sources: [
    libgnome_qr_gtk_gir,
  ],
  dependencies: libgnome_qr_gtk_deps,
  include_directories: [
    include_directories('..'),
  ],
  link_with: libgnome_qr_gtk,
)
