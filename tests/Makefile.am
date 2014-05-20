include $(top_srcdir)/glib-tap.mk

LDADD = $(top_builddir)/libgnome-desktop/libgnome-desktop-3.la $(JSON_LIBS)

AM_CPPFLAGS =							\
	-I$(top_srcdir)						\
	$(GNOME_DESKTOP_CFLAGS)					\
	-DG_LOG_DOMAIN=\"GnomeDesktop\"				\
	$(NULL)

AM_CFLAGS = -g $(WARN_CFLAGS)

test_programs =			\
	wall-clock		\
	$(NULL)

-include $(top_srcdir)/git.mk
