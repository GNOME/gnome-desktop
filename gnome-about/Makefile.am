INCLUDES =		 					\
	$(WARN_CFLAGS)						\
	$(DISABLE_DEPRECATED_CFLAGS)				\
	$(GNOME_ABOUT_CFLAGS)					\
	$(DISABLE_DEPRECATED_CFLAGS)				\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale\""  \
	-DGNOME_ICONDIR=\""$(datadir)/pixmaps\""

LDADD = $(GNOME_ABOUT_LIBS)

bin_PROGRAMS = gnome-about

gnome_about_SOURCES =		\
	gnome-about.c		\
	contributors.h

aboutdir = $(datadir)/gnome-about
about_DATA = gnome-64.gif

EXTRA_DIST = $(about_DATA)

SUBDIRS = headers
