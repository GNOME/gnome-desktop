AUTOMAKE_OPTIONS = 1.4

INCLUDES = \
	-I.			\
	-I$(srcdir)		\
	-I$(top_srcdir)/intl	\
	-I$(top_builddir)/intl	\
	-I$(top_builddir)/panel	\
	-I$(includedir)		\
	-DGNOME_DESKTOP_SYSCONFDIR=\""$(sysconfdir)"\" \
	$(GNOME_DESKTOP_CFLAGS)

noinst_LTLIBRARIES = libgnome-desktop-2.la

noinst_PROGRAMS = test-ditem

CORBA_SRCLIST = \
	GNOME_Desktop-stubs.c	\
	GNOME_Desktop-skels.c	\
	GNOME_Desktop-common.c	\
	GNOME_Desktop.h

# try to make these get cleaned properly
CLEANFILES += $(CORBA_SRCLIST)

$(CORBA_SRCLIST): $(top_srcdir)/idl/GNOME_Desktop.idl $(ORBIT_IDL)
	$(ORBIT_IDL) -I $(BONOBO_IDLDIR) $(top_srcdir)/idl/GNOME_Desktop.idl

BUILT_SOURCES = $(CORBA_SRCLIST)

libgnome_desktop_2_la_SOURCES = \
	gnome-desktop-item.h		\
	gnome-desktop-item.c		\
	bonobo-config-ditem.c		\
	bonobo-config-ditem.h		\
	bonobo-config-ditem-internals.c	\
	bonobo-config-ditem-internals.h	\
	$(CORBA_SRCLIST)

libgnome_desktop_2_la_LIBADD = \
	$(GNOME_DESKTOP_LIBS)

test_ditem_SOURCES = \
	test-ditem.c

test_ditem_LDADD = \
	libgnome-desktop-2.la

