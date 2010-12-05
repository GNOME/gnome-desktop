lib_LTLIBRARIES = libgnome-desktop-3.la

AM_CPPFLAGS =							\
	-I$(top_srcdir)						\
	$(GNOME_DESKTOP_CFLAGS)					\
	$(XLIB_CFLAGS)						\
	-DG_LOG_DOMAIN=\"GnomeDesktop\"				\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale\""	\
	-DPNP_IDS=\""$(PNP_IDS)"\"				\
	$(DISABLE_DEPRECATED_CFLAGS)

AM_CFLAGS = $(WARN_CFLAGS)

libgnome_desktop_3_la_SOURCES = \
	gnome-desktop-utils.c	\
	gnome-desktop-thumbnail.c \
	gnome-thumbnail-pixbuf-utils.c \
	gnome-bg.c		\
	gnome-bg-crossfade.c	\
	display-name.c		\
	gnome-rr.c		\
	gnome-rr-config.c	\
	gnome-rr-labeler.c	\
	gnome-rr-private.h	\
	gnome-tz-monitor.c	\
	edid-parse.c		\
	edid.h			\
	private.h

libgnome_desktop_3_la_LIBADD =	\
	$(XLIB_LIBS)		\
	$(GNOME_DESKTOP_LIBS)

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
        gnome-desktop-utils.h           \
        gnome-desktop-thumbnail.h       \
        gnome-rr.h                      \
        gnome-rr-config.h               \
        gnome-rr-labeler.h		\
        gnome-tz-monitor.h

if USE_INTERNAL_PNP_IDS
pnpdatadir = $(datadir)/libgnome-desktop-3.0
pnpdata_DATA = pnp.ids
endif
pnpdata_DATA_dist = pnp.ids

check:
	test -s $(top_srcdir)/libgnome-desktop/pnp.ids

EXTRA_DIST = \
	gnome-desktop-3.0.pc.in			\
	gnome-desktop-3.0-uninstalled.pc.in	\
	$(pnpdata_DATA_dist)

MAINTAINERCLEANFILES = \
	pnp.ids

-include $(top_srcdir)/git.mk
