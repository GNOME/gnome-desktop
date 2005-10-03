SUBDIRS =			\
	po 			\
	libgnome-desktop 	\
	gnome-about 		\
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
	intltool-extract.in	\
	intltool-merge.in	\
	intltool-update.in	\
	gnome-doc-utils.make

versiondir=$(datadir)/gnome-about
version_DATA=gnome-version.xml
@INTLTOOL_XML_RULE@

DISTCHECK_CONFIGURE_FLAGS = --disable-scrollkeeper

CLEANFILES = $(version_DATA)
DISTCLEANFILES = \
  intltool-extract \
  intltool-merge \
  intltool-update \
  gnome-doc-utils.make
