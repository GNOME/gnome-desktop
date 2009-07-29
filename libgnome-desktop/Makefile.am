SUBDIRS = libgnome libgnomeui

INCLUDES =							\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale\""  \
	-DPNP_IDS=\""$(PNP_IDS)"\"				\
	$(WARN_CFLAGS)						\
	$(DISABLE_DEPRECATED)					\
	$(XLIB_CFLAGS)						\
	$(GNOME_DESKTOP_CFLAGS)

lib_LTLIBRARIES = libgnome-desktop-2.la

noinst_PROGRAMS = test-ditem

libgnome_desktop_2_la_SOURCES = \
	gnome-desktop-item.c	\
	gnome-desktop-utils.c	\
	gnome-desktop-thumbnail.c \
	gnome-thumbnail-pixbuf-utils.c \
	gnome-bg.c		\
	gnome-bg-crossfade.c	\
	display-name.c		\
	gnome-rr.c		\
	gnome-rr-config.c	\
	gnome-rr-labeler.c	\
	gnome-rr-private.h	\
	edid-parse.c		\
	edid.h			\
	private.h

libgnome_desktop_2_la_LIBADD =	\
	$(XLIB_LIBS)		\
	$(GNOME_DESKTOP_LIBS)

libgnome_desktop_2_la_LDFLAGS = \
	-version-info $(LT_VERSION) \
	-no-undefined

test_ditem_SOURCES = \
	test-ditem.c

test_ditem_LDADD = \
	libgnome-desktop-2.la		\
	$(XLIB_LIBS)			\
	$(GNOME_DESKTOP_LIBS)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gnome-desktop-2.0.pc

pnpdata_DATA_dist = pnp.ids
if USE_INTERNAL_PNP_IDS
pnpdatadir = $(datadir)/libgnome-desktop
pnpdata_DATA = pnp.ids
endif

check:
	test -s $(top_srcdir)/libgnome-desktop/pnp.ids

EXTRA_DIST = \
	gnome-desktop-2.0.pc.in                \
	gnome-desktop-2.0-uninstalled.pc.in    \
	$(pnpdata_DATA_dist)

MAINTAINERCLEANFILES = \
	pnp.ids

-include $(top_srcdir)/git.mk
