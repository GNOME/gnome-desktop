AUTOMAKE_OPTIONS = 1.4

INCLUDES = \
	-I.			\
	-I$(srcdir)		\
	-I$(top_srcdir)/intl	\
	-I$(top_builddir)/intl	\
	-I$(top_builddir)/panel	\
	-I$(includedir)		\
	-DGNOME_DESKTOP_SYSCONFDIR=\""$(sysconfdir)"\" \
	-DGNOME_CORE_INTERNAL	\
	$(GNOME_DESKTOP_CFLAGS)

lib_LTLIBRARIES = libgnome-desktop-2.la

noinst_PROGRAMS = test-ditem

libgnome_desktop_2_la_SOURCES = \
	gnome-desktop-item.c	\
	gnome-ditem-edit.c

libgnome_desktopdir = $(includedir)/gnome-desktop-2.0/libgnome
libgnome_desktop_HEADERS = \
	gnome-desktop-item.h

libgnomeui_desktopdir = $(includedir)/gnome-desktop-2.0/libgnomeui
libgnomeui_desktop_HEADERS = \
	gnome-ditem-edit.h

libgnome_desktop_2_la_LIBADD = \
	$(GNOME_DESKTOP_LIBS)

test_ditem_SOURCES = \
	test-ditem.c

test_ditem_LDADD = \
	libgnome-desktop-2.la		\
	$(GNOME_DESKTOP_LIBS)
