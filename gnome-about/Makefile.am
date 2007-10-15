INCLUDES =		 					\
	$(WARN_CFLAGS)						\
	$(DISABLE_DEPRECATED_CFLAGS)				\
	$(GNOME_ABOUT_CFLAGS)					\
	$(DISABLE_DEPRECATED)					\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale\""  \
	-DGNOME_ICONDIR=\""$(datadir)/pixmaps\""		\
	-DDATADIR=\"$(datadir)/gnome-about\"

LDADD = $(GNOME_ABOUT_LIBS)

bin_PROGRAMS = gnome-about

gnome_about_SOURCES =		\
	gnome-about.c		\
	contributors.c		\
	contributors.h

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-about.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

aboutdir = $(datadir)/gnome-about
about_DATA = gnome-64.png contributors.list foundation-members.list

EXTRA_DIST = $(about_DATA)

SUBDIRS = headers

CLEANFILES = gnome-about.desktop

check:
	test -s $(top_srcdir)/gnome-about/foundation-members.list
