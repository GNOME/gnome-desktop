project('gnome-desktop', 'c',
  meson_version: '>= 0.49.0',
  version: '41.1',
  license: ['GPL2+', 'LGPL2+']
)

# Before making a release, the libversion string should be modified.
#
#  * Bump the first component if binary compatibility has been broken; or
#  * Bump the second component if new APIs are added; or
#  * Bump the third component otherwise.
#
# When bumping the first component version, set the second and third components
# to 0. When bumping the second version, set the third one to zero.
#
# A lot easier than libtool, right?
libversion = '19.1.7'
soversion = libversion.split('.')[0]

gdk_pixbuf_req = '>= 2.36.5'
gtk_req = '>= 3.3.6'
glib_req = '>= 2.53.0'
xrandr_req = '>= 1.3'
schemas_req = '>= 3.27.0'
xext_req = '>= 1.1'

gnome = import('gnome')
i18n = import('i18n')
pkg = import('pkgconfig')

prefix = get_option('prefix')

datadir = join_paths(prefix, get_option('datadir'))
libexecdir = join_paths(prefix, get_option('libexecdir'))

liblocaledir = join_paths(get_option('prefix'), 'lib', 'locale')
localedir = join_paths(datadir, 'locale')
test_metadir = join_paths(datadir, 'installed-tests', meson.project_name())
test_execdir = join_paths(libexecdir, 'installed-tests', meson.project_name())
versiondir = join_paths(datadir, 'gnome')

gdk_pixbuf_dep = dependency('gdk-pixbuf-2.0', version: gdk_pixbuf_req)
gtk_dep = dependency('gtk+-3.0', version: gtk_req)
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
fontconfig_cache_path = fontconfig_dep.get_pkgconfig_variable('cachedir')

xkb_base = xkb_config_dep.get_pkgconfig_variable('xkb_base')
iso_codes_prefix = iso_codes_dep.get_pkgconfig_variable('prefix')

cc = meson.get_compiler('c')

m_dep = cc.find_library('m', required: false)
rt_dep = cc.find_library('rt', required: false)

if not cc.has_function('clock_gettime', dependencies: rt_dep)
  rt_dep = dependency('', required: false)
endif

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

config_h = declare_dependency(
  sources: configure_file(
    configuration: conf,
    input: 'config.h.meson',
    output: 'config.h'
  ),
  include_directories: include_directories('.')
)

version_array = meson.project_version().split('.')
version_conf = configuration_data()

version_conf.set('GNOME_PLATFORM', version_array[0])
version_conf.set('GNOME_MINOR', version_array[1])
version_conf.set('GNOME_MICRO', 0)
version_conf.set('GNOME_DISTRIBUTOR', get_option('gnome_distributor'))

if get_option('date_in_gnome_version')
  date = run_command(['date', '+%Y-%m-%d'])
  version_conf.set('GNOME_DATE', date.stdout().strip())
  version_conf.set('GNOME_DATE_COMMENT_START', '')
  version_conf.set('GNOME_DATE_COMMENT_END', '')
else
  version_conf.set('GNOME_DATE', '')
  version_conf.set('GNOME_DATE_COMMENT_START', '<!--')
  version_conf.set('GNOME_DATE_COMMENT_END', '-->')
endif

version_xml = configure_file(
  configuration: version_conf,
  input: 'gnome-version.xml.in',
  output: 'gnome-version.xml'
)
install_data(version_xml, install_dir: versiondir)

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
