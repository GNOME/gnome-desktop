SUBDIRS =			\
	po 			\
	libgnome-desktop 	\
	gnome-about 		\
	desktop-links		\
	pixmaps			\
	desktop-docs		\
	man

EXTRA_DIST =			\
	gnome-version.xml.in.in	\
	gnome-version.xml	\
	gnome-desktop.spec.in	\
	gnome-desktop.spec	\
	COPYING-DOCS		\
	HACKING			\
	xmldocs.make		\
	intltool-extract.in	\
	intltool-merge.in	\
	intltool-update.in

versiondir=$(datadir)/gnome-about
version_DATA=gnome-version.xml
@INTLTOOL_XML_RULE@

# ignore scrollkeeper files, which will legitimately exist after uninstall.
distuninstallcheck_listfiles = find . -type f -print | grep -v scrollkeeper

CLEANFILES = $(version_DATA)
DISTCLEANFILES = \
  intltool-extract \
  intltool-merge \
  intltool-update
