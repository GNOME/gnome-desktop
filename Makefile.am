if ORBIT_INSTALLED
control_center = control-center
capplets = capplets
panel = panel
applets = applets
endif

always_built_SUBDIRS = \
	 gsm smproxy pixmaps desktop-links desktop-properties help-browser \
	 gnome-terminal core-docs gmenu

sometimes_built_SUBDIRS = panel applets control-center capplets

SUBDIRS = intl po macros \
	 $(always_built_SUBDIRS) $(panel) $(applets) $(control_center) $(capplets)

#DIST_SUBDIRS = intl po macros $(always_built_SUBDIRS) \
#	$(sometimes_built_SUBDIRS)

EXTRA_DIST = gnome-core.spec

## to automatically rebuild aclocal.m4 if any of the macros in `macros/' change
@MAINT@include macros/macros.dep
@MAINT@macros/macros.dep: macros/Makefile.am
@MAINT@	cd macros && $(MAKE) macros.dep
