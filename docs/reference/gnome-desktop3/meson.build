ignored_headers = [
  'gnome-rr-private.h',
  'libgsystem',
  'meta-dbus-idle-monitor.h',
  'meta-dbus-xrandr.h',
  'meta-xrandr-shared.h'
]

gnome.gtkdoc('gnome-desktop3',
  main_sgml: 'gnome-desktop3-docs.sgml',
  ignore_headers: ignored_headers,
  scan_args: '--deprecated-guards="GNOME_DISABLE_DEPRECATED"',
  mkdb_args: ['--sgml-mode', '--output-format=xml'],
  src_dir: join_paths(meson.project_source_root(), 'libgnome-desktop'),
  install: true
)
