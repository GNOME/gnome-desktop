libargs = [
  '-DG_LOG_DOMAIN="GnomeDesktop"',
  '-DGDK_DISABLE_DEPRECATION_WARNINGS',
  '-DGTK_DISABLE_DEPRECATION_WARNINGS',
  '-DGNOMELOCALEDIR="@0@"'.format(localedir),
  '-DLIBLOCALEDIR="@0@"'.format(liblocaledir),
  '-DISO_CODES_PREFIX="@0@"'.format(iso_codes_prefix),
  '-DFONTCONFIG_CACHE_PATH="@0@"'.format(fontconfig_cache_path),
  '-DXKB_BASE="@0@"'.format(xkb_base)
]

conf = configuration_data()

conf.set_quoted('GETTEXT_PACKAGE', '@0@-3.0'.format(meson.project_name()))
conf.set_quoted('INSTALL_PREFIX', prefix)

conf.set('ENABLE_SECCOMP', seccomp_dep.found())
conf.set('HAVE_BWRAP', seccomp_dep.found())
conf.set('_GNU_SOURCE', seccomp_dep.found())

conf.set('HAVE_SYSTEMD', libsystemd_dep.found())
conf.set('HAVE_UDEV', udev_dep.found())
conf.set('HAVE_XKBREGISTRY', xkbregistry_dep.found())

conf.set('HAVE_TIMERFD', cc.has_function('timerfd_create'))
conf.set('HAVE_OPENAT', cc.has_function('openat'))
conf.set('HAVE_USELOCALE', cc.has_function('uselocale'))
conf.set('HAVE_XLOCALE', cc.has_header('xlocale.h'))

configure_file(
  output: 'config.h',
  configuration: conf,
)

version_conf = configuration_data()
version_array = meson.project_version().split('.')
version_conf.set('GNOME_PLATFORM', version_array[0])

gnome_desktop_header_file = configure_file(
  input: 'gnome-desktop-version.h.in',
  output: 'gnome-desktop-version.h',
  configuration: version_conf,
)

dbus_xrandr_built_sources = gnome.gdbus_codegen('meta-dbus-xrandr',
  'xrandr.xml',
  namespace: 'MetaDBus',
  interface_prefix: 'org.gnome.Mutter'
)

dbus_idle_built_sources = gnome.gdbus_codegen('meta-dbus-idle-monitor',
  'idle-monitor.xml',
  namespace: 'MetaDBus',
  interface_prefix: 'org.gnome.Mutter',
  object_manager: true
)

base_ldflags = []
base_symbol_map = '-Wl,--version-script=@0@'.format(meson.current_source_dir() / 'base-symbol.map')
if cc.has_link_argument(base_symbol_map)
  base_ldflags += base_symbol_map
endif

### gnome-desktop-base
libgnome_desktop_base_sources = [
  'gnome-desktop-version.c',
  'gnome-desktop-thumbnail.c',
  'gnome-idle-monitor.c',
  'gnome-languages.c',
  'gnome-pnp-ids.c',
  'gnome-systemd.c',
  'gnome-wall-clock.c',
  'gnome-xkb-info.c',
]

libgnome_desktop_base_private_sources = [
  'gnome-datetime-source.c',
  'gnome-desktop-thumbnail-script.c',
  'gnome-gettext-portable.c',
]

libgnome_desktop_base_headers = [
  gnome_desktop_header_file,
  'gnome-desktop-thumbnail.h',
  'gnome-idle-monitor.h',
  'gnome-languages.h',
  'gnome-pnp-ids.h',
  'gnome-systemd.h',
  'gnome-wall-clock.h',
  'gnome-xkb-info.h',
]

if get_option('build_gtk4')
  install_headers(libgnome_desktop_base_headers,
    subdir: 'gnome-desktop-4.0/libgnome-desktop'
  )

  base_deps = [
    gdk_pixbuf_dep,
    glib_dep,
    gio_dep,
    gio_unix_dep,
    libsystemd_dep,
    schemas_dep,
    xkb_config_dep,
    xkbregistry_dep,
    iso_codes_dep,
    udev_dep,
    seccomp_dep,
    m_dep,
    rt_dep,
  ]

  libgnome_desktop_base = library('gnome-desktop-4',
    sources: [
      libgnome_desktop_base_sources,
      libgnome_desktop_base_private_sources,
      dbus_idle_built_sources,
    ],
    dependencies: base_deps,
    soversion: soversion,
    version: libversion,
    c_args: libargs,
    link_args: base_ldflags,
    install: true,
    include_directories: [
      include_directories('.'),
      include_directories('..'),
    ],
  )

  if get_option('introspection')
    libgnome_desktop_base_gir = gnome.generate_gir(libgnome_desktop_base,
      sources: [libgnome_desktop_base_headers, libgnome_desktop_base_sources],
      export_packages: 'gnome-desktop-4',
      namespace: 'GnomeDesktop',
      nsversion: '4.0',
      includes: ['GObject-2.0', 'Gio-2.0', 'GDesktopEnums-3.0', 'GdkPixbuf-2.0'],
      extra_args: ['-DGNOME_DESKTOP_USE_UNSTABLE_API', '--quiet', '--warn-all'],
      identifier_prefix: 'Gnome',
      symbol_prefix: 'gnome',
      install: true,
    )
  else
    libgnome_desktop_base_gir = ''
  endif

  pkg.generate(
    libgnome_desktop_base,
    requires: ['gsettings-desktop-schemas'],
    version: meson.project_version(),
    name: 'gnome-desktop-4',
    filebase: 'gnome-desktop-4',
    description: 'Utility library for GNOME desktop components',
    subdirs: 'gnome-desktop-4.0',
  )

  libgnome_desktop_base_dep = declare_dependency(
    sources: [
      libgnome_desktop_base_gir,
    ],
    dependencies: base_deps,
    link_with: libgnome_desktop_base,
    include_directories: [
      include_directories('.'),
      include_directories('..'),
    ],
  )

  subdir('gnome-bg')
  subdir('gnome-rr')
endif

### Legacy ###
if get_option('legacy_library')
  introspection_sources = [
    'gnome-desktop-version.c',
    'gnome-desktop-thumbnail.c',
    'gnome-bg.c',
    'gnome-bg-slide-show.c',
    'gnome-bg-crossfade.c',
    'gnome-rr.c',
    'gnome-rr-config.c',
    'gnome-rr-output-info.c',
    'gnome-systemd.c',
    'gnome-pnp-ids.c',
    'gnome-wall-clock.c',
    'gnome-xkb-info.c',
    'gnome-idle-monitor.c',
    'gnome-languages.c',
  ]

  libgnome_desktop_sources = [
    introspection_sources,
    dbus_xrandr_built_sources,
    dbus_idle_built_sources,
    'gnome-datetime-source.c',
    'gnome-desktop-thumbnail-script.c',
    'gnome-gettext-portable.c',
  ]

  libgnome_desktop_headers = [
    gnome_desktop_header_file,
    'gnome-bg.h',
    'gnome-bg-crossfade.h',
    'gnome-bg-slide-show.h',
    'gnome-desktop-thumbnail.h',
    'gnome-rr.h',
    'gnome-rr-config.h',
    'gnome-systemd.h',
    'gnome-pnp-ids.h',
    'gnome-wall-clock.h',
    'gnome-xkb-info.h',
    'gnome-idle-monitor.h',
    'gnome-languages.h',
  ]

  install_headers(libgnome_desktop_headers,
    subdir: 'gnome-desktop-3.0/libgnome-desktop'
  )

  gnome_desktop_deps = [
    gdk_pixbuf_dep,
    gtk3_dep,
    glib_dep,
    gio_dep,
    gio_unix_dep,
    libsystemd_dep,
    schemas_dep,
    xkb_config_dep,
    xkbregistry_dep,
    iso_codes_dep,
    udev_dep,
    seccomp_dep
  ]

  compat_ldflags = []
  compat_symbol_map = '-Wl,--version-script=@0@'.format(meson.current_source_dir() / 'symbol.map')
  if cc.has_link_argument(compat_symbol_map)
    compat_ldflags += compat_symbol_map
  endif

  libgnome_desktop = library('gnome-desktop-3',
    libgnome_desktop_sources,
    dependencies: [gnome_desktop_deps, m_dep, rt_dep],
    soversion: compat_soversion,
    version: compat_libversion,
    c_args: libargs,
    link_args: compat_ldflags,
    install: true,
    include_directories: [
      include_directories('.'),
      include_directories('..'),
    ],
  )

  libgnome_desktop_dep = declare_dependency(
    dependencies: gnome_desktop_deps,
    link_with: libgnome_desktop,
    include_directories: [
      include_directories('.'),
      include_directories('..'),
    ],
  )

  if get_option('introspection')
    gnome.generate_gir(libgnome_desktop,
      sources: introspection_sources + libgnome_desktop_headers,
      export_packages: 'gnome-desktop-3.0',
      namespace: 'GnomeDesktop',
      nsversion: '3.0',
      includes: ['GObject-2.0', 'Gtk-3.0', 'GDesktopEnums-3.0'],
      extra_args: ['-DGNOME_DESKTOP_USE_UNSTABLE_API', '--quiet', '--warn-all'],
      identifier_prefix: 'Gnome',
      symbol_prefix: 'gnome',
      install: true
    )
  endif

  pkg.generate(
    libraries: libgnome_desktop,
    requires: ['gtk+-3.0', 'gsettings-desktop-schemas'],
    version: meson.project_version(),
    name: 'gnome-desktop-3.0',
    filebase: 'gnome-desktop-3.0',
    description: 'Utility library for loading .desktop files',
    subdirs: 'gnome-desktop-3.0'
  )

  ### Tests
  test_programs = [
    'gnome-rr-debug',
    'test-xkb-info',
    'test-pnp-ids',
    'test-wall-clock',
    'test-desktop-thumbnail',
    'test-idle-monitor',
    'test-languages'
  ]

  foreach program: test_programs
    executable(program, program + '.c',
      dependencies: libgnome_desktop_dep,
      install: get_option('debug_tools'),
      install_dir: libexecdir / 'gnome-desktop-debug',
    )
  endforeach
endif
