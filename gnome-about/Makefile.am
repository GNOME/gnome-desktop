INCLUDES = 			\
	-I$(includedir)		\
	-I$(top_srcdir)/intl	\
	-I$(top_builddir)/intl	\
	$(GNOME_INCLUDEDIR)	\
	-DGNOMELOCALEDIR=\""$(datadir)/locale\""

LDADD = \
	$(GNOME_LIBDIR)		\
	$(GNOMEUI_LIBS)		\
	$(PIXBUF_LIBS)		\
	$(CANVAS_PIXBUF_LIBS)	\
	$(INTLLIBS)

bin_PROGRAMS = gnome-about

gnome_about_SOURCES =		\
	gnome-about.c		\
	authors.h

authorsdir = $(datadir)/gnome-about
authors_DATA = authors.dat

EXTRA_DIST = logo.xpm $(authors_DATA)
