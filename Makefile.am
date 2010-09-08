SUBDIRS =			\
	po 			\
	libgnome-desktop 	\
	pixmaps			\
	docs			\
	man

if GNOME_ABOUT_ENABLED
SUBDIRS += gnome-about
endif
if DESKTOP_DOCS_ENABLED
SUBDIRS += desktop-docs
endif

ACLOCAL_AMFLAGS = -I m4

if GNOME_ABOUT_ENABLED
versiondir=$(datadir)/gnome-about
version_DATA=gnome-version.xml
endif
@INTLTOOL_XML_RULE@

EXTRA_DIST =			\
	COPYING-DOCS		\
	HACKING			\
	MAINTAINERS		\
	ChangeLog.pre-git	\
	gnome-version.xml.in.in	\
	gnome-doc-utils.make

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc --disable-scrollkeeper

CLEANFILES = \
	$(version_DATA)

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
