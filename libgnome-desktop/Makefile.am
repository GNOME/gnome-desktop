AUTOMAKE_OPTIONS = 1.4

localedir = $(datadir)/gnome

INCLUDES = \
	-I.			\
	-I$(srcdir)		\
	-I$(top_srcdir)/intl	\
	-I$(top_builddir)/intl	\
	-I$(top_builddir)/panel	\
	-I$(includedir)		\
	-DGNOME_DESKTOP_SYSCONFDIR=\""$(sysconfdir)"\" \
	-DLOCALE_ALIAS_PATH=\""$(localedir)"\" \
	$(GNOME_DESKTOP_CFLAGS)

noinst_LTLIBRARIES = libgnome-desktop-2.la

noinst_PROGRAMS = test-ditem

bin_PROGRAMS = bonobo-moniker-ditem

CORBA_SRCLIST = \
	GNOME_Desktop-stubs.c	\
	GNOME_Desktop-skels.c	\
	GNOME_Desktop-common.c	\
	GNOME_Desktop.h

Bonobo_Moniker_ditem.server.in: Bonobo_Moniker_ditem.server.in.in
	sed -e "s|\@MONIKER_BINDIR\@|$(bindir)|" $< > $@

serverdir = $(libdir)/bonobo/servers
server_DATA = Bonobo_Moniker_ditem.server

@XML_I18N_MERGE_SERVER_RULE@

# try to make these get cleaned properly
CLEANFILES += $(CORBA_SRCLIST)

$(CORBA_SRCLIST): $(top_srcdir)/idl/GNOME_Desktop.idl $(ORBIT_IDL)
	$(ORBIT_IDL) -I $(BONOBO_IDLDIR) $(top_srcdir)/idl/GNOME_Desktop.idl

BUILT_SOURCES = $(CORBA_SRCLIST)

bonobo_moniker_ditem_SOURCES = \
	$(CORBA_SRCLIST)		\
	bonobo-config-ditem.c		\
	bonobo-config-ditem.h		\
	bonobo-moniker-ditem.c

bonobo_moniker_ditem_LDADD = \
	$(GNOME_DESKTOP_LIBS)

libgnome_desktop_2_la_SOURCES = \
	gnome-desktop-item.h		\
	gnome-desktop-item.c		\
	gnome-desktop-i18n.c		\
	gnome-desktop-i18n.h		\
	$(CORBA_SRCLIST)

libgnome_desktop_2_la_LIBADD = \
	$(GNOME_DESKTOP_LIBS)

test_ditem_SOURCES = \
	bonobo-config-ditem.c		\
	bonobo-config-ditem.h		\
	test-ditem.c

test_ditem_LDADD = \
	libgnome-desktop-2.la

locale_DATA = \
	locale.alias

