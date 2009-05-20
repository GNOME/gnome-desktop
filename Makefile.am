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
	ChangeLog.pre-git	\
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

MAINTAINERCLEANFILES = \
	$(srcdir)/aclocal.m4 \
	$(srcdir)/config.guess \
	$(srcdir)/config.h.in \
	$(srcdir)/config.sub \
	$(srcdir)/depcomp \
	$(srcdir)/install-sh \
	$(srcdir)/ltmain.sh \
	$(srcdir)/missing \
	$(srcdir)/mkinstalldirs \
	`find "$(srcdir)" -type f -name Makefile.in -print` \
	$(srcdir)/configure \
	$(srcdir)/gtk-doc.make \
	$(srcdir)/m4/gnome-doc-utils.m4 \
	$(srcdir)/m4/gtk-doc.m4 \
	$(srcdir)/m4/intltool.m4

-include $(top_srcdir)/git.mk
