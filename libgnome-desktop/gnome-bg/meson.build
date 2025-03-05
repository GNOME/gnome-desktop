### gnome-bg

libgnome_bg_sources = [
  'gnome-bg.c',
  'gnome-bg-slide-show.c',
]

libgnome_bg_headers = [
  'gnome-bg.h',
  'gnome-bg-slide-show.h',
]

install_headers(libgnome_bg_headers,
  subdir: 'gnome-desktop-4.0/gnome-bg'
)

libgnome_bg_deps = [
  libgnome_desktop_base_dep,
  gdk_pixbuf_dep,
  gtk4_dep,
]

libgnome_bg_ldflags = []
libgnome_bg_symbols_map = '-Wl,--version-script=@0@'.format(meson.current_source_dir() / 'gnome-bg-symbols.map')
if cc.has_link_argument(libgnome_bg_symbols_map)
  libgnome_bg_ldflags += libgnome_bg_symbols_map
endif

libgnome_bg = library('gnome-bg-4',
  sources: libgnome_bg_sources,
  dependencies: libgnome_bg_deps,
  soversion: soversion,
  version: libversion,
  c_args: libargs,
  link_args: libgnome_bg_ldflags,
  install: true,
  include_directories: [
    include_directories('.'),
    include_directories('..'),
  ],
)

if get_option('introspection')
  libgnome_bg_gir = gnome.generate_gir(libgnome_bg,
    sources: [libgnome_bg_headers, libgnome_bg_sources],
    export_packages: 'gnome-bg-4',
    namespace: 'GnomeBG',
    nsversion: '4.0',
    includes: [libgnome_desktop_base_gir[0], 'GdkPixbuf-2.0', 'Gdk-4.0'],
    extra_args: ['-DGNOME_DESKTOP_USE_UNSTABLE_API', '--quiet', '--warn-all'],
    identifier_prefix: 'Gnome',
    symbol_prefix: 'gnome',
    install: true,
  )
else
  libgnome_bg_gir = ''
endif
libgnome_bg_dep = declare_dependency(
  sources: [
    libgnome_bg_gir,
  ],
  dependencies: libgnome_bg_deps,
  link_with: libgnome_bg,
  include_directories: [
    include_directories('.'),
    include_directories('..'),
  ],
)

pkg.generate(
  libgnome_bg,
  requires: ['gsettings-desktop-schemas'],
  version: meson.project_version(),
  name: 'gnome-bg-4',
  filebase: 'gnome-bg-4',
  description: 'Background image utility library for GNOME components',
  subdirs: 'gnome-desktop-4.0',
)
