AUTOMAKE_OPTIONS = 1.4

INCLUDES = \
	-I.			\
	-I$(srcdir)		\
	-I$(top_builddir)/panel	\
	-DGNOME_DESKTOP_SYSCONFDIR=\""$(sysconfdir)"\" \
	-DGNOME_CORE_INTERNAL	\
	$(GNOME_DESKTOP_CFLAGS)

lib_LTLIBRARIES = libgnome-desktop-2.la

noinst_PROGRAMS = test-ditem test-hint

libgnome_desktop_2_la_SOURCES = \
	gnome-desktop-item.c	\
	gnome-ditem-edit.c	\
	gnome-hint.c


libgnome_desktopdir = $(includedir)/gnome-desktop-2.0/libgnome
libgnome_desktop_HEADERS = \
	gnome-desktop-item.h

libgnomeui_desktopdir = $(includedir)/gnome-desktop-2.0/libgnomeui
libgnomeui_desktop_HEADERS = \
	gnome-ditem-edit.h \
	gnome-hint.h

libgnome_desktop_2_la_LIBADD = \
	$(GNOME_DESKTOP_LIBS)

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

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gnome-desktop-2.0.pc

EXTRA_DIST = \
	gnome-desktop-2.0.pc.in
