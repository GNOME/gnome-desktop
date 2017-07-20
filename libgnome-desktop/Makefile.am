CLEANFILES =
NULL =

EXTRA_DIST =
noinst_LTLIBRARIES =
lib_LTLIBRARIES = libgnome-desktop-3.la

AM_CPPFLAGS =							\
	-I$(top_srcdir)						\
	$(GNOME_DESKTOP_CFLAGS)					\
	$(XLIB_CFLAGS)						\
	-DG_LOG_DOMAIN=\"GnomeDesktop\"				\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale\""	\
	-DISO_CODES_PREFIX=\""$(ISO_CODES_PREFIX)"\"		\
	-DLIBLOCALEDIR=\""$(prefix)/lib/locale"\"		\
	-DPNP_IDS=\""$(PNP_IDS)"\"				\
	-DXKB_BASE=\""$(XKB_BASE)"\"				\
	$(DISABLE_DEPRECATED_CFLAGS)

AM_CFLAGS = $(WARN_CFLAGS)

introspection_sources = 		\
	gnome-desktop-thumbnail.c 	\
	gnome-thumbnail-pixbuf-utils.c 	\
	gnome-bg.c			\
	gnome-bg-slide-show.c		\
	gnome-bg-crossfade.c		\
	gnome-rr.c			\
	gnome-rr-config.c		\
	gnome-rr-output-info.c		\
	gnome-pnp-ids.c			\
	gnome-wall-clock.c		\
	gnome-xkb-info.c		\
	gnome-idle-monitor.c		\
	gnome-languages.c

libgnome_desktop_3_la_SOURCES = 	\
	$(introspection_sources)	\
	$(dbus_xrandr_built_sources)	\
	$(dbus_idle_built_sources)	\
	gnome-datetime-source.h		\
	gnome-datetime-source.c		\
	gnome-rr-private.h		\
	default-input-sources.h		\
	meta-xrandr-shared.h		\
	gnome-desktop-thumbnail-script.c\
	gnome-desktop-thumbnail-script.h

dbus_xrandr_built_sources = meta-dbus-xrandr.c meta-dbus-xrandr.h

$(dbus_xrandr_built_sources) : Makefile.am xrandr.xml
	$(AM_V_GEN)gdbus-codegen							\
		--interface-prefix org.gnome.Mutter					\
		--c-namespace MetaDBus							\
		--generate-c-code meta-dbus-xrandr					\
		$(srcdir)/xrandr.xml

dbus_idle_built_sources = meta-dbus-idle-monitor.c meta-dbus-idle-monitor.h

$(dbus_idle_built_sources) : Makefile.am idle-monitor.xml
	$(AM_V_GEN)gdbus-codegen							\
		--interface-prefix org.gnome.Mutter					\
		--c-namespace MetaDBus							\
		--generate-c-code meta-dbus-idle-monitor				\
		--c-generate-object-manager						\
		$(srcdir)/idle-monitor.xml

BUILT_SOURCES = $(dbus_xrandr_built_sources) $(dbus_idle_built_sources)

libgnome_desktop_3_la_LIBADD =	\
	$(XLIB_LIBS)		\
	$(LIBM)			\
	$(GNOME_DESKTOP_LIBS)	\
	$(LIBRT)		\
	$(NULL)

libgnome_desktop_3_la_LDFLAGS = \
	-version-info $(LT_VERSION) \
	-export-symbols-regex "^gnome_.*" \
	-no-undefined

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gnome-desktop-3.0.pc

libgnome_desktopdir = $(includedir)/gnome-desktop-3.0/libgnome-desktop
libgnome_desktop_HEADERS = \
        gnome-bg.h                      \
        gnome-bg-crossfade.h            \
	gnome-bg-slide-show.h		\
        gnome-desktop-thumbnail.h       \
        gnome-rr.h                      \
        gnome-rr-config.h               \
        gnome-pnp-ids.h			\
        gnome-wall-clock.h		\
	gnome-xkb-info.h		\
	gnome-idle-monitor.h		\
	gnome-languages.h

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir) --warn-all
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

if HAVE_INTROSPECTION

GnomeDesktop-3.0.gir: libgnome-desktop-3.la
GnomeDesktop_3_0_gir_INCLUDES = GObject-2.0 Gtk-3.0 GDesktopEnums-3.0
GnomeDesktop_3_0_gir_PACKAGES = gdk-pixbuf-2.0 glib-2.0 gobject-2.0 gio-2.0 gtk+-3.0 gsettings-desktop-schemas
GnomeDesktop_3_0_gir_EXPORT_PACKAGES = gnome-desktop-3.0
GnomeDesktop_3_0_gir_CFLAGS = -DGNOME_DESKTOP_USE_UNSTABLE_API -I$(top_srcdir)
GnomeDesktop_3_0_gir_LIBS = libgnome-desktop-3.la
GnomeDesktop_3_0_gir_FILES = $(introspection_sources) $(libgnome_desktop_HEADERS)
GnomeDesktop_3_0_gir_SCANNERFLAGS = --identifier-prefix=Gnome --symbol-prefix=gnome
INTROSPECTION_SCANNER_ARGS += $(patsubst %,--c-include='libgnome-desktop/%',$(libgnome_desktop_HEADERS))
INTROSPECTION_GIRS += GnomeDesktop-3.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif

EXTRA_DIST += \
	idle-monitor.xml			\
	xrandr.xml				\
	gnome-desktop-3.0.pc.in			\
	gnome-desktop-3.0-uninstalled.pc.in

gnome_rr_debug_SOURCES =	\
	gnome-rr-debug.c
gnome_rr_debug_LDADD =		\
	$(GNOME_DESKTOP_LIBS)	\
	$(lib_LTLIBRARIES)	\
	$(XLIB_LIBS)

if DEBUG_TOOLS_ENABLED
libexec_PROGRAMS = gnome-rr-debug
endif

noinst_PROGRAMS = test-xkb-info test-pnp-ids test-wall-clock test-desktop-thumbnail test-idle-monitor test-languages
test_xkb_info_LDADD =		\
	$(GNOME_DESKTOP_LIBS)	\
	$(lib_LTLIBRARIES)

test_pnp_ids_LDADD = $(test_xkb_info_LDADD)

test_wall_clock_LDADD = $(test_xkb_info_LDADD)

test_desktop_thumbnail_LDADD = $(test_xkb_info_LDADD)

test_idle_monitor_LDADD = $(test_xkb_info_LDADD)

test_languages_LDADD = $(test_xkb_info_LDADD)

-include $(top_srcdir)/git.mk
