panel = panel
applets = applets
idl = idl

always_built_SUBDIRS = \
	 gsm smproxy pixmaps desktop-links help-browser \
	 gnome-terminal gmenu gnome-edit gnome-hint gnome-about core-docs

sometimes_built_SUBDIRS = panel applets idl

SUBDIRS = intl po macros \
	 $(always_built_SUBDIRS) $(panel) $(applets) $(idl)

#DIST_SUBDIRS = intl po macros $(always_built_SUBDIRS) \
#	$(sometimes_built_SUBDIRS)

EXTRA_DIST = \
	gnome-core.spec.in	\
	gnome-core.spec		\
	applet-docs.make	\
	app-docs.make

## to automatically rebuild aclocal.m4 if any of the macros in `macros/' change
@MAINT@include macros/macros.dep
@MAINT@macros/macros.dep: macros/Makefile.am
@MAINT@	cd macros && $(MAKE) macros.dep
