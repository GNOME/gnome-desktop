project('gnome-desktop', 'c',
  meson_version: '>= 0.47.0',
  version: '3.30.0',
  license: ['GPL2+', 'LGPL2+']
)

# Before making a release, the soversion string should be modified.
# The string is of the form C.A.R.
# - If interfaces have been changed or added, but binary compatibility has
#   been preserved, change to C+1.A+1.0
# - If binary compatibility has been broken (eg removed or changed interfaces)
#   change to C+1.0.0
# - If the interface is the same as the previous version, change to C.A.R+1

soversion = '17.0.2'

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
xkb_config_dep = dependency('xkeyboard-config')
iso_codes_dep = dependency('iso-codes')
x_dep = dependency('x11')

udev_dep = dependency('libudev', required: get_option('udev'))

# Check for bubblewrap compatible platform
host_os = host_machine.system()
host_cpu = host_machine.cpu()
supported_os = ['linux']
unsupported_cpus = ['alpha', 'ia64', 'm68k', 'sh4', 'sparc64']
if supported_os.contains(host_os) and not unsupported_cpus.contains(host_cpu)
  seccomp_dep = dependency('libseccomp')
else
  seccomp_dep = dependency('', required: false)
endif

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

conf.set10('ENABLE_SECCOMP', seccomp_dep.found())
conf.set10('HAVE_BWRAP', seccomp_dep.found())
conf.set10('_GNU_SOURCE', seccomp_dep.found())

conf.set10('HAVE_UDEV', udev_dep.found())

conf.set10('HAVE_TIMERFD', cc.has_function('timerfd_create'))
conf.set10('HAVE_OPENAT', cc.has_function('openat'))

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
version_conf.set('GNOME_MICRO', version_array[2])
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