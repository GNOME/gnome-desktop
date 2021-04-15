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

introspection_sources = [
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
  'gnome-languages.c'
]

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

libgnome_desktop_sources = [
  introspection_sources,
  dbus_xrandr_built_sources,
  dbus_idle_built_sources,
  'gnome-datetime-source.h',
  'gnome-datetime-source.c',
  'gnome-rr-private.h',
  'default-input-sources.h',
  'meta-xrandr-shared.h',
  'gnome-desktop-thumbnail-script.c',
  'gnome-desktop-thumbnail-script.h',
  'gnome-gettext-portable.c'
]

libgnome_desktop_headers = [
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
  'gnome-languages.h'
]

install_headers(libgnome_desktop_headers,
  subdir: 'gnome-desktop-3.0/libgnome-desktop'
)

gnome_desktop_deps = [
  config_h,
  gdk_pixbuf_dep,
  gtk_dep,
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

ldflags = []
symbol_map = '-Wl,--version-script=@0@'.format(meson.current_source_dir() / 'symbol.map')
if cc.has_link_argument(symbol_map)
  ldflags += symbol_map
endif

libgnome_desktop = library('gnome-desktop-3',
  libgnome_desktop_sources,
  dependencies: [gnome_desktop_deps, m_dep, rt_dep],
  soversion: soversion,
  version: libversion,
  c_args: libargs,
  link_args: ldflags,
  install: true
)

libgnome_desktop_dep = declare_dependency(
  dependencies: gnome_desktop_deps,
  link_with: libgnome_desktop
)

gnome.generate_gir(libgnome_desktop,
  sources: introspection_sources + libgnome_desktop_headers,
  export_packages: 'gnome-desktop-3.0',
  namespace: 'GnomeDesktop',
  nsversion: '3.0',
  includes: ['GObject-2.0', 'Gtk-3.0', 'GDesktopEnums-3.0'],
  extra_args: '-DGNOME_DESKTOP_USE_UNSTABLE_API',
  identifier_prefix: 'Gnome',
  symbol_prefix: 'gnome',
  install: true
)

desktop_pc = pkg.generate(
  libraries: libgnome_desktop,
  requires: ['gtk+-3.0', 'gsettings-desktop-schemas'],
  version: meson.project_version(),
  name: 'gnome-desktop-3.0',
  filebase: 'gnome-desktop-3.0',
  description: 'Utility library for loading .desktop files',
  subdirs: 'gnome-desktop-3.0'
)

test_programs = [
  'gnome-rr-debug',
  'test-xkb-info',
  'test-pnp-ids',
  'test-wall-clock',
  'test-desktop-thumbnail',
  'test-idle-monitor',
  'test-languages'
]

foreach program : test_programs
  executable(program, program + '.c',
    dependencies: libgnome_desktop_dep,
    install: get_option('debug_tools'),
    install_dir: libexecdir / 'gnome-desktop-debug',
  )
endforeach
