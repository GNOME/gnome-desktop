SUBDIRS = libgnome libgnomeui

INCLUDES =							\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale\""  \
	-DGNOME_DESKTOP_SYSCONFDIR=\""$(sysconfdir)"\"		\
	-DGNOME_DESKTOP_PIXMAPDIR=\""$(datadir)/pixmaps"\"	\
	-DGNOME_DESKTOP_ICONDIR=\""$(datadir)/icons"\"		\
	-DGNOME_CORE_INTERNAL					\
	-DDATADIR=\""$(pkgdatadir)"\"				\
	$(WARN_CFLAGS)						\
	$(DISABLE_DEPRECATED)					\
	$(XLIB_CFLAGS)						\
	$(GNOME_DESKTOP_CFLAGS)

lib_LTLIBRARIES = libgnome-desktop-2.la

noinst_PROGRAMS = test-ditem

libgnome_desktop_2_la_SOURCES = \
	gnome-desktop-item.c	\
	gnome-desktop-utils.c	\
	gnome-desktop-thumbnail.c \
	gnome-thumbnail-pixbuf-utils.c \
	gnome-bg.c		\
	display-name.c		\
	gnome-rr.c		\
	gnome-rr-config.c	\
	gnome-rr-labeler.c	\
	edid-parse.c		\
	edid.h

libgnome_desktop_2_la_LIBADD =	\
	$(XLIB_LIBS)		\
	$(GNOME_DESKTOP_LIBS)

libgnome_desktop_2_la_LDFLAGS = \
	-version-info $(LT_VERSION) \
	-no-undefined

test_ditem_SOURCES = \
	test-ditem.c

test_ditem_LDADD = \
	libgnome-desktop-2.la		\
	$(XLIB_LIBS)			\
	$(GNOME_DESKTOP_LIBS)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gnome-desktop-2.0.pc

EXTRA_DIST = \
	gnome-desktop-2.0.pc.in                \
	gnome-desktop-2.0-uninstalled.pc.in
