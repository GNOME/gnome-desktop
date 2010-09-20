SUBDIRS = headers

bin_SCRIPTS = gnome-about

gnome-about: gnome-about.in Makefile
	$(AM_V_GEN)sed							\
		-e s!\@PYTHON\@!@PYTHON@!				\
		-e s!\@GETTEXT_PACKAGE\@!@GETTEXT_PACKAGE@!		\
		-e s!\@LOCALEDIR\@!$(datadir)/locale!			\
		-e s!\@DATADIR\@!$(datadir)/gnome-about!		\
		-e s!\@ICONDIR\@!$(datadir)/pixmaps!			\
		-e s!\@PACKAGE_NAME\@!$(PACKAGE_NAME)!			\
		-e s!\@PACKAGE_VERSION\@!$(PACKAGE_VERSION)!		\
		< $< > $@ &&						\
	chmod a+x $@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-about.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

aboutdir = $(datadir)/gnome-about
about_DATA = gnome-64.png contributors.list foundation-members.list

check:
	test -s $(top_srcdir)/gnome-about/foundation-members.list

EXTRA_DIST = $(about_DATA) gnome-about.in

CLEANFILES = $(desktop_DATA) gnome-about

MAINTAINERCLEANFILES = \
	foundation-members.list

-include $(top_srcdir)/git.mk
