include $(top_srcdir)/glib-tap.mk

LDADD = $(top_builddir)/libgnome-desktop/libgnome-desktop-3.la $(JSON_LIBS)

AM_CPPFLAGS =							\
	-I$(top_srcdir)						\
	$(GNOME_DESKTOP_CFLAGS)					\
	-DG_LOG_DOMAIN=\"GnomeDesktop\"				\
	$(NULL)

AM_CFLAGS = -g $(WARN_CFLAGS)

TESTS_ENVIRONMENT += G_ENABLE_DIAGNOSTIC=0

test_programs =			\
	wall-clock		\
	gtk-reftest		\
	$(NULL)

EXTRA_DIST +=			\
	C.ref.ui		\
	C.ui			\
	en_US.utf-8.ref.ui	\
	en_US.utf-8.ui		\
	he_IL.utf8.ref.ui	\
	he_IL.utf8.ui

-include $(top_srcdir)/git.mk
