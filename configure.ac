m4_define(gnome_platform, 3)
m4_define(gnome_minor, 26)
m4_define(gnome_micro, 2)
m4_define(gnome_sub,)
dnl gnome_sub is an optional sub-version which will not be advertised to the user
dnl It allows to release a 2.30.1.1 which is just a better 2.30.1
dnl Leave empty if not needed

m4_define(gnome_version, [gnome_platform.gnome_minor.gnome_micro]m4_ifval(gnome_sub, .gnome_sub)),

AC_INIT([gnome-desktop], gnome_version,
        [http://bugzilla.gnome.org/enter_bug.cgi?product=gnome-desktop])
AC_CONFIG_SRCDIR([libgnome-desktop])

AM_INIT_AUTOMAKE([1.11.2 foreign no-dist-gzip dist-xz tar-ustar])
AM_MAINTAINER_MODE([enable])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_CONFIG_HEADERS([config.h])

AC_CONFIG_MACRO_DIR([m4])

# Before making a release, the LT_VERSION string should be modified.
# The string is of the form C:R:A.
# - If interfaces have been changed or added, but binary compatibility has
#   been preserved, change to C+1:0:A+1
# - If binary compatibility has been broken (eg removed or changed interfaces)
#   change to C+1:0:0
# - If the interface is the same as the previous version, change to C:R+1:A

LT_VERSION=14:1:2
AC_SUBST(LT_VERSION)

LT_PREREQ([2.2.6])
LT_INIT([dlopen win32-dll disable-static])

IT_PROG_INTLTOOL([0.40.6])

AC_PROG_CC
PKG_PROG_PKG_CONFIG

GNOME_PLATFORM=gnome_platform
GNOME_MINOR=gnome_minor
GNOME_MICRO=gnome_micro
GNOME_DISTRIBUTOR="GNOME.Org"
GNOME_DATE=`date +"%Y-%m-%d"`

AC_ARG_WITH(gnome_distributor, [ --with-gnome-distributor=DISTRIBUTOR Specify name of GNOME distributor])

if test "x$with_gnome_distributor" != x ; then
 GNOME_DISTRIBUTOR=$with_gnome_distributor
fi

AC_ARG_ENABLE([date-in-gnome-version],
 [AS_HELP_STRING([--disable-date-in-gnome-version],[do not put the build date in gnome-version.xml])],
 [],[enable_date_in_gnome_version=yes])
if test "$enable_date_in_gnome_version" = "no"; then
 GNOME_DATE=
 GNOME_DATE_COMMENT_START="<!--"
 GNOME_DATE_COMMENT_END="-->"
else
 GNOME_DATE_COMMENT_START=
 GNOME_DATE_COMMENT_END=
fi

AC_SUBST(GNOME_PLATFORM)
AC_SUBST(GNOME_MINOR)
AC_SUBST(GNOME_MICRO)
AC_SUBST(GNOME_DISTRIBUTOR)
AC_SUBST(GNOME_DATE)
AC_SUBST(GNOME_DATE_COMMENT_START)
AC_SUBST(GNOME_DATE_COMMENT_END)

GNOME_COMPILE_WARNINGS([maximum])
GNOME_MAINTAINER_MODE_DEFINES

AC_ARG_ENABLE(deprecation_flags,
              [AC_HELP_STRING([--enable-deprecation-flags],
                              [use *_DISABLE_DEPRECATED flags @<:@default=no@:>@])],,
              [enable_deprecation_flags=no])

if test "x$enable_deprecation_flags" = "xyes"; then
   DISABLE_DEPRECATED_CFLAGS=$DISABLE_DEPRECATED
   AC_SUBST(DISABLE_DEPRECATED_CFLAGS)
fi

# Should we build shared desktop docs? Useful for people who
# need to build two gnome-desktop using two different versions of GTK+
AC_ARG_ENABLE([desktop-docs],
	[AS_HELP_STRING([--disable-desktop-docs],[do not build desktop-wide help documents])],
	[],[enable_desktop_docs=yes])
AM_CONDITIONAL(DESKTOP_DOCS_ENABLED, test "x$enable_desktop_docs" = "xyes")

# Should we build debug tools? Useful to bundle the gnome-desktop library
# e.g. with a xdg-app runtime
AC_ARG_ENABLE([debug-tools],
        [AS_HELP_STRING([--disable-debug-tools],[do not build debug tools])],
        [],[enable_debug_tools=yes])
AM_CONDITIONAL(DEBUG_TOOLS_ENABLED, test "x$enable_debug_tools" = "xyes")

GLIB_TESTS

dnl If you add a version number here, you *must* add an AC_SUBST line for
dnl it too, or it will never make it into the spec file!

GDK_PIXBUF_REQUIRED=2.36.5
GTK_REQUIRED=3.3.6
GLIB_REQUIRED=2.53.0
XRANDR_REQUIRED=1.3
GSETTINGS_DESKTOP_SCHEMAS_REQUIRED=3.5.91
XEXT_REQUIRED=1.1

AC_SUBST(GTK_REQUIRED)
AC_SUBST(GLIB_REQUIRED)
AC_SUBST(GDK_PIXBUF_REQUIRED)
AC_SUBST(XRANDR_REQUIRED)
AC_SUBST(XEXT_REQUIRED)

AC_CHECK_LIBM
AC_SUBST(LIBM)

AC_CHECK_LIB([rt], [clock_gettime], [LIBRT="-lrt"], [LIBRT=""])
AC_SUBST(LIBRT)

AC_CHECK_FUNCS([openat])

dnl we need x11 for GnomeBG

PKG_CHECK_MODULES(XLIB, x11,
    X11_PACKAGE=x11,
    [X11_PACKAGE=
     AC_PATH_XTRA
     if test "x$no_x" = xyes; then
       AC_MSG_ERROR(["no (requires X development libraries)"])
     else
       XLIB_LIBS="$X_PRE_LIBS $X_LIBS -lX11 $X_EXTRA_LIBS"
       XLIB_CFLAGS=$X_CFLAGS
     fi])
AC_SUBST(X11_PACKAGE)
AC_SUBST(XLIB_CFLAGS)
AC_SUBST(XLIB_LIBS)

dnl ---------------------------------------------------------------------------
dnl - hwdb for pnp IDs (default enabled)
dnl ---------------------------------------------------------------------------
UDEV_PKG=""
AC_ARG_ENABLE(udev, AS_HELP_STRING([--disable-udev],[Disable udev support]), enable_udev=$enableval)
if test x$enable_udev != xno; then
	PKG_CHECK_MODULES(UDEV, libudev, have_udev="yes", have_udev="no")
	if test "x$have_udev" = "xyes"; then
		AC_DEFINE(HAVE_UDEV, 1, [define if udev is available])
		UDEV_PKG="libudev"
	else
		if test x$enable_udev = xyes; then
			AC_MSG_ERROR([udev enabled but not found])
		fi
	fi
else
	have_udev=no
fi

SECCOMP_PKG=""
dnl Check for bubblewrap compatible platform
case $host_os in
  linux*)
    PKG_CHECK_MODULES(LIBSECCOMP, [libseccomp])
    SECCOMP_PKG="libseccomp"
    AC_DEFINE_UNQUOTED(_GNU_SOURCE, 1, [Define to include GNU extensions])
    AC_DEFINE_UNQUOTED(HAVE_BWRAP, 1, [Define to 1 if Bubblewrap support is available])
    AC_DEFINE([ENABLE_SECCOMP], [1], [Define if using seccomp])
    AC_DEFINE_UNQUOTED(INSTALL_PREFIX, "$prefix", [Path to library install prefix])
    ;;
esac

dnl pkg-config dependency checks

PKG_CHECK_MODULES(GNOME_DESKTOP, gdk-pixbuf-2.0 >= $GDK_PIXBUF_REQUIRED
                                 gtk+-3.0 >= $GTK_REQUIRED
                                 glib-2.0 >= $GLIB_REQUIRED
                                 gio-2.0 >= $GLIB_REQUIRED
                                 gsettings-desktop-schemas >= $GSETTINGS_DESKTOP_SCHEMAS_REQUIRED
                                 xkeyboard-config
                                 iso-codes
                                 $UDEV_PKG $SECCOMP_PKG)

XKB_BASE=$($PKG_CONFIG --variable xkb_base xkeyboard-config)
AC_SUBST(XKB_BASE)

ISO_CODES_PREFIX=$($PKG_CONFIG --variable prefix iso-codes)
AC_SUBST(ISO_CODES_PREFIX)

AC_CACHE_CHECK(for timerfd_create(2) system call,
    gnome_cv_timerfd,AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#include <sys/timerfd.h>
#include <unistd.h>
],[
int
main (void)
{
  timerfd_create (CLOCK_MONOTONIC, TFD_CLOEXEC);
  return 0;
}
])],gnome_cv_timerfd=yes,gnome_cv_timerfd=no))
if test x"$gnome_cv_timerfd" = x"yes"; then
  AC_DEFINE(HAVE_TIMERFD, 1, [we have the timerfd_create(2) system call])
fi

dnl Documentation support

YELP_HELP_INIT

dnl Language Support

GETTEXT_PACKAGE=gnome-desktop-3.0
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE",
		   [The gettext translation domain])
AC_SUBST(GETTEXT_PACKAGE)

AM_GLIB_GNU_GETTEXT

GOBJECT_INTROSPECTION_CHECK([0.9.7])

# check for gtk-doc
GTK_DOC_CHECK([1.14],[--flavour no-tmpl])

AC_CONFIG_FILES([
Makefile
libgnome-desktop/Makefile
libgnome-desktop/gnome-desktop-3.0.pc
libgnome-desktop/gnome-desktop-3.0-uninstalled.pc
docs/Makefile
docs/reference/Makefile
docs/reference/gnome-desktop3/Makefile
gnome-version.xml.in
po/Makefile.in
desktop-docs/Makefile
desktop-docs/fdl/Makefile
desktop-docs/gpl/Makefile
desktop-docs/lgpl/Makefile
tests/Makefile
])

AC_OUTPUT

dnl ---------------------------------------------------------------------------
dnl - Show summary
dnl ---------------------------------------------------------------------------

echo "
              gnome-desktop $VERSION
              `echo gnome-desktop $VERSION | sed "s/./=/g"`

        prefix:                       ${prefix}
        exec_prefix:                  ${exec_prefix}
        libdir:                       ${libdir}
        bindir:                       ${bindir}
        sbindir:                      ${sbindir}
        sysconfdir:                   ${sysconfdir}
        localstatedir:                ${localstatedir}
        datadir:                      ${datadir}
        source code location:         ${srcdir}
        compiler:                     ${CC}
        cflags:                       ${CFLAGS}
        Maintainer mode:              ${USE_MAINTAINER_MODE}
        Use *_DISABLE_DEPRECATED:     ${enable_deprecation_flags}

        GNOME Distributor:            ${GNOME_DISTRIBUTOR}
        Build desktop-wide docs:      ${enable_desktop_docs}
        Build debug tools:            ${enable_debug_tools}
        Date in gnome-version.xml:    ${enable_date_in_gnome_version}
        Build gtk-doc documentation:  ${enable_gtk_doc}
        Build with udev support:      ${enable_udev}

"
