ACLOCAL_AMFLAGS = -I m4

SUBDIRS =			\
	po 			\
	libgnome-desktop 	\
	gnome-about 		\
	pixmaps			\
	desktop-docs		\
	docs			\
	man

EXTRA_DIST =			\
	MAINTAINERS		\
	gnome-version.xml.in.in	\
	gnome-version.xml	\
	COPYING-DOCS		\
	HACKING			\
	gnome-doc-utils.make

versiondir=$(datadir)/gnome-about
version_DATA=gnome-version.xml
@INTLTOOL_XML_RULE@

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc --disable-scrollkeeper

CLEANFILES = $(version_DATA)
DISTCLEANFILES = \
  gnome-doc-utils.make
