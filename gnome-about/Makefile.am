INCLUDES = 			\
	-I$(includedir)		\
	-I$(top_srcdir)/intl	\
	-I$(top_builddir)/intl	\
	$(GNOME_INCLUDEDIR)	\
	-DGNOMELOCALEDIR=\""$(datadir)/locale\""

LDADD = \
	$(GNOME_LIBDIR)		\
	$(GNOMEUI_LIBS)		\
	$(INTLLIBS)

bin_PROGRAMS = gnome-about

gnome_about_SOURCES =		\
	gnome-about.c	\
	authors.h

EXTRA_DIST = logo.xpm