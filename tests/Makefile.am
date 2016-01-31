include $(top_srcdir)/glib-tap.mk

LDADD = $(GNOME_DESKTOP_LIBS) $(top_builddir)/libgnome-desktop/libgnome-desktop-3.la

AM_CPPFLAGS =							\
	-I$(top_srcdir)						\
	$(GNOME_DESKTOP_CFLAGS)					\
	-DG_LOG_DOMAIN=\"GnomeDesktop\"				\
	-DINSTALLED_TEST_DIR=\"$(installed_testdir)\"		\
	$(NULL)

AM_CFLAGS = -g $(WARN_CFLAGS)

TESTS_ENVIRONMENT += G_ENABLE_DIAGNOSTIC=0

installed_test_programs =	\
	wall-clock		\
	wallclock-reftest	\
	$(NULL)

dist_installed_test_data =	\
	C.ref.ui		\
	C.ui			\
	en_US.utf-8.ref.ui	\
	en_US.utf-8.ui		\
	he_IL.utf8.ref.ui	\
	he_IL.utf8.ui

-include $(top_srcdir)/git.mk
