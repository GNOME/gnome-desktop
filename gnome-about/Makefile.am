SUBDIRS = logos

INCLUDES =		 					\
	$(WARN_CFLAGS)						\
	$(GNOME_ABOUT_CFLAGS)					\
	$(DISABLE_DEPRECATED_CFLAGS)				\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale\""  \
	-DGNOME_ICONDIR=\""$(datadir)/pixmaps\""

# Flags to make sure we're not using any deprecated functions
DISABLE_DEPRECATED_CFLAGS =					\
	-DG_DISABLE_DEPRECATED					\
	-DGDK_DISABLE_DEPRECATED				\
	-DGTK_DISABLE_DEPRECATED				\
	-DGDK_PIXBUF_DISABLE_DEPRECATED				\
	-DGNOME_DISABLE_DEPRECATED

LDADD = \
	$(GNOME_ABOUT_LIBS)

bin_PROGRAMS = gnome-about

gnome_about_SOURCES =		\
	gnome-about.c		\
	contributors.h
