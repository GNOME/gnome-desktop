SUBDIRS = \
	 intl \
	 po \
	 macros \
	 gsm \
	 smproxy \
	 pixmaps \
	 desktop-links \
	 help-browser \
	 gnome-terminal \
	 gmenu \
	 gnome-edit \
	 gnome-hint \
	 gnome-about \
	 core-docs \
	 panel \
	 applets \
	 omf-install \
	 idl

EXTRA_DIST = \
	gnome-core.spec.in	\
	gnome-core.spec		\
	COPYING-DOCS		\
	sgmldocs.make		\
	xml-i18n-extract.in	\
	xml-i18n-merge.in	\
	xml-i18n-update.in

## to automatically rebuild aclocal.m4 if any of the macros in `macros/' change
@MAINT@include macros/macros.dep
@MAINT@macros/macros.dep: macros/Makefile.am
@MAINT@	cd macros && $(MAKE) macros.dep
