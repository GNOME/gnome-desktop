INCLUDES = 			\
	-I$(includedir)		\
	-I$(top_srcdir)/intl	\
	-I$(top_builddir)/intl	\
	$(GNOME_INCLUDEDIR)	\
	$(PIXBUF_CFLAGS)	\
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
	contributors.h

man_MANS = gnome-about.1

authorsdir = $(datadir)/gnome-about
authors_DATA = contributors.dat

EXTRA_DIST = logo.xpm $(authors_DATA) $(man_MANS)
