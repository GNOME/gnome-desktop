INCLUDES = 			\
	-I$(includedir)		\
	-I$(top_srcdir)/intl	\
	-I$(top_builddir)/intl	\
	$(GNOME_ABOUT_CFLAGS)	\
	-DGNOMELOCALEDIR=\""$(datadir)/locale\""

LDADD = \
	$(GNOME_ABOUT_LIBS)	\
	$(INTLLIBS)

bin_PROGRAMS = gnome-about

gnome_about_SOURCES =		\
	gnome-about.c		\
	contributors.h

authorsdir = $(datadir)/gnome-about
authors_DATA = contributors.dat

EXTRA_DIST = logo.xpm $(authors_DATA)
