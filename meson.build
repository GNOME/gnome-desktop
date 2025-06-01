project('gnome-desktop', 'c',
  meson_version: '>= 0.56.2',
  version: '44.4',
  license: ['GPL-2.0-or-later', 'LGPL-2.1-or-later'],
  default_options: [
    'c_std=gnu99',
  ],
)

# Before making a release that changes the source code, the libversion and
# compat_libversion strings should be modified. (No need to touch these for
# releases with no code changes.)
#
#  * Bump the first component if binary compatibility has been broken; or
#  * Bump the second component if new APIs are added; or
#  * Bump the third component otherwise.
#
# After creating a new stable branch, the next release on the main branch should
# bump the second component to ensure we don't wind up with the same libversion
# for different releases on different branches.
#
# When bumping the first component version, set the second and third components
# to 0. When bumping the second version, set the third one to zero.
#
# A lot easier than libtool, right?
libversion = '2.1.4'
soversion = libversion.split('.')[0]

# Compatibility versions for libgnome-desktop-3
compat_libversion = '20.0.0'
compat_soversion = compat_libversion.split('.')[0]

gdk_pixbuf_req = '>= 2.36.5'
gtk3_req = '>= 3.3.6'
gtk4_req = '>= 4.4.0'
glib_req = '>= 2.53.0'
xrandr_req = '>= 1.3'
schemas_req = '>= 3.27.0'
xext_req = '>= 1.1'

gnome = import('gnome')
i18n = import('i18n')
pkg = import('pkgconfig')

prefix = get_option('prefix')

datadir = prefix / get_option('datadir')
libexecdir = prefix / get_option('libexecdir')

liblocaledir = get_option('prefix') / 'lib/locale'
localedir = datadir / 'locale'
test_metadir = datadir / 'installed-tests' / meson.project_name()
test_execdir = libexecdir / 'installed-tests' / meson.project_name()
versiondir = datadir / 'gnome'

gdk_pixbuf_dep = dependency('gdk-pixbuf-2.0', version: gdk_pixbuf_req)
gtk3_dep = dependency('gtk+-3.0', version: gtk3_req, required: get_option('legacy_library'))
gtk4_dep = dependency('gtk4', version: gtk4_req, required: get_option('build_gtk4'))
glib_dep = dependency('glib-2.0', version: glib_req)
gio_dep = dependency('gio-2.0', version: glib_req)
gio_unix_dep = dependency('gio-unix-2.0', version: glib_req)
schemas_dep = dependency('gsettings-desktop-schemas', version: schemas_req)
fontconfig_dep = dependency('fontconfig')
xkb_config_dep = dependency('xkeyboard-config')
xkbregistry_dep = dependency('xkbregistry', required: false)
iso_codes_dep = dependency('iso-codes')
libsystemd_dep = dependency('libsystemd', required: get_option('systemd'))
udev_dep = dependency('libudev', required: get_option('udev'))

# Check for bubblewrap compatible platform
host_os = host_machine.system()
host_cpu = host_machine.cpu()
supported_os = ['linux']
unsupported_cpus = ['alpha', 'ia64', 'm68k', 'sh4', 'sparc', 'sparc64']
if supported_os.contains(host_os) and not unsupported_cpus.contains(host_cpu)
  seccomp_dep = dependency('libseccomp')
else
  seccomp_dep = dependency('', required: false)
endif

fontconfig_cache_path = fontconfig_dep.get_variable(pkgconfig: 'cachedir')
xkb_base = xkb_config_dep.get_variable(pkgconfig: 'xkb_base')
iso_codes_prefix = iso_codes_dep.get_variable(pkgconfig: 'prefix')

cc = meson.get_compiler('c')

m_dep = cc.find_library('m', required: false)
rt_dep = cc.find_library('rt', required: false)

if not cc.has_function('clock_gettime', dependencies: rt_dep)
  rt_dep = dependency('', required: false)
endif

subdir('libgnome-desktop')
subdir('po')
subdir('tests')

if get_option('desktop_docs')
  subdir('desktop-docs/fdl')
  subdir('desktop-docs/gpl')
  subdir('desktop-docs/lgpl')
endif

if get_option('gtk_doc')
  subdir('docs/reference/gnome-desktop3')
endif

summary({
    'prefix': get_option('prefix'),
    'datadir': datadir,
    'libexecdir': libexecdir,
    'localedir': localedir,
    'versiondir': versiondir,
  },
  section: 'Directories',
)

summary({
    'Use systemd': get_option('systemd').enabled() or get_option('systemd').auto(),
    'Use udev': get_option('udev').enabled() or get_option('udev').auto(),
    'Build GTK4 libraries': get_option('build_gtk4'),
    'Build legacy libgnome-desktop': get_option('legacy_library'),
    'Desktop documentation': get_option('desktop_docs'),
    'API documentation': get_option('gtk_doc'),
    'Debug tools': get_option('debug_tools'),
    'Installed tests': get_option('installed_tests'),
  },
  section: 'Build options',
  bool_yn: true,
)
