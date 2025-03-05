libgnome_rr_deps = [
  libgnome_desktop_base_dep,
  gtk4_dep,
]

libgnome_rr_sources = [
  'gnome-rr-config.c',
  'gnome-rr-output-info.c',
  'gnome-rr-screen.c',
]

libgnome_rr_headers = [
  'gnome-rr.h',
  'gnome-rr-config.h',
  'gnome-rr-output-info.h',
  'gnome-rr-screen.h',
  'gnome-rr-types.h',
]

install_headers(libgnome_rr_headers,
  subdir: 'gnome-desktop-4.0/gnome-rr'
)

libgnome_rr_ldflags = []
libgnome_rr_symbols_map = '-Wl,--version-script=@0@'.format(meson.current_source_dir() / 'gnome-rr-symbols.map')
if cc.has_link_argument(libgnome_rr_symbols_map)
  libgnome_rr_ldflags += libgnome_rr_symbols_map
endif

libgnome_rr = library('gnome-rr-4',
  sources: [
    libgnome_rr_sources,
    dbus_xrandr_built_sources,
  ],
  dependencies: libgnome_rr_deps,
  soversion: soversion,
  version: libversion,
  c_args: libargs,
  link_args: libgnome_rr_ldflags,
  install: true,
  include_directories: [
    include_directories('.'),
    include_directories('..'),
  ],
)

if get_option('introspection') 
  libgnome_rr_gir = gnome.generate_gir(libgnome_rr,
    sources: [libgnome_rr_headers, libgnome_rr_sources],
    export_packages: 'gnome-rr-4',
    namespace: 'GnomeRR',
    nsversion: '4.0',
    includes: [libgnome_desktop_base_gir[0], 'Gdk-4.0'],
    extra_args: ['-DGNOME_DESKTOP_USE_UNSTABLE_API', '--quiet', '--warn-all'],
    identifier_prefix: 'GnomeRR',
    symbol_prefix: 'gnome_rr',
    install: true,
  )
else
  libgnome_rr_gir = ''
endif


libgnome_rr_dep = declare_dependency(
  sources: [
    dbus_xrandr_built_sources,
    libgnome_rr_gir,
  ],
  dependencies: libgnome_rr_deps,
  link_with: libgnome_rr,
  include_directories: [
    include_directories('.'),
    include_directories('..'),
  ],
)

pkg.generate(
  libgnome_rr,
  requires: ['gsettings-desktop-schemas'],
  version: meson.project_version(),
  name: 'gnome-rr-4',
  filebase: 'gnome-rr-4',
  description: 'Display information utility library for GNOME desktop components',
  subdirs: 'gnome-desktop-4.0',
)

