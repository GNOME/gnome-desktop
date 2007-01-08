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
	$(GNOME_DESKTOP_CFLAGS)

lib_LTLIBRARIES = libgnome-desktop-2.la

noinst_PROGRAMS = test-ditem test-hint test-ditem-edit

libgnome_desktop_2_la_SOURCES = \
	gnome-desktop-item.c	\
	gnome-ditem-edit.c	\
	gnome-hint.c

libgnome_desktop_2_la_LIBADD = \
	$(GNOME_DESKTOP_LIBS)

libgnome_desktop_2_la_LDFLAGS = \
	-version-info $(LT_VERSION) \
	-no-undefined

test_ditem_SOURCES = \
	test-ditem.c

test_ditem_LDADD = \
	libgnome-desktop-2.la		\
	$(GNOME_DESKTOP_LIBS)

test_hint_SOURCES = \
        test-hint.c

test_hint_LDADD = \
        libgnome-desktop-2.la           \
        $(GNOME_DESKTOP_LIBS)

test_ditem_edit_SOURCES = test-ditem-edit.c

test_ditem_edit_LDADD =		\
	libgnome-desktop-2.la	\
	$(GNOME_DESKTOP_LIBS)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gnome-desktop-2.0.pc

EXTRA_DIST = \
	gnome-desktop-2.0.pc.in                \
	gnome-desktop-2.0-uninstalled.pc.in
