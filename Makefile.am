SUBDIRS =			\
	po 			\
	libgnome-desktop 	\
	gnome-about 		\
	desktop-links		\
	pixmaps			\
	desktop-docs		\
	man

schemadir   = $(GCONF_SCHEMA_FILE_DIR)
schema_DATA = \
	desktop_gnome_icon_theme.schemas

EXTRA_DIST =			\
	gnome-version.xml.in	\
	gnome-version.xml	\
	gnome-desktop.spec.in	\
	gnome-desktop.spec	\
	COPYING-DOCS		\
	HACKING			\
	sgmldocs.make		\
	xmldocs.make		\
	intltool-extract.in	\
	intltool-merge.in	\
	intltool-update.in	\
	$(schema_DATA)

versiondir=$(datadir)/gnome-about
version_DATA=gnome-version.xml


# don't do this if we are building in eg. rpm
install-data-local:
	if test -z "$(DESTDIR)" ; then \
		for p in $(schema_DATA) ; do \
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(srcdir)/$$p; \
		done \
	fi
