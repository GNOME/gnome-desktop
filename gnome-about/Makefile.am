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

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-about.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

aboutdir = $(datadir)/gnome-about
about_DATA = gnome-64.png contributors.list foundation-members.list

EXTRA_DIST = $(about_DATA) gnome-about.in

SUBDIRS = headers

CLEANFILES = gnome-about.desktop gnome-about

check:
	test -s $(top_srcdir)/gnome-about/foundation-members.list

MAINTAINERCLEANFILES = \
	foundation-members.list

-include $(top_srcdir)/git.mk
