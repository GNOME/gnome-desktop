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

noinst_LTLIBRARIES = libgnome_desktop-2.la

CORBA_SRCLIST = \
	gnome-desktop-stubs.c	\
	gnome-desktop-skels.c	\
	gnome-desktop-common.c	\
	gnome-desktop.h

# try to make these get cleaned properly
CLEANFILES += $(CORBA_SRCLIST)

$(CORBA_SRCLIST): $(top_srcdir)/idl/gnome-desktop.idl $(ORBIT_IDL)
	$(ORBIT_IDL) -I $(BONOBO_IDLDIR) $(top_srcdir)/idl/gnome-desktop.idl

BUILT_SOURCES = $(CORBA_SRCLIST)

libgnome_desktop_2_la_SOURCES = \
	gnome-ditem.h		\
	gnome-ditem.c		\
	$(CORBA_SRCLIST)

libgnome_desktop_2_la_LIBADD = \
	$(GNOME_DESKTOP_LIBS)
