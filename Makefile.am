if PROGRAM_GMENU
gmenu = gmenu
endif

if ORBIT_INSTALLED
control_center = control-center
endif

always_built_SUBDIRS = panel applets \
	 gsm smproxy pixmaps desktop-links desktop-properties help-browser \
	 gnome-terminal core-docs

sometimes_built_SUBDIRS = gmenu control-center

SUBDIRS = intl po macros $(always_built_SUBDIRS) $(gmenu) $(control_center)

#DIST_SUBDIRS = intl po macros $(always_built_SUBDIRS) \
#	$(sometimes_built_SUBDIRS)

EXTRA_DIST = gnome-core.spec

## to automatically rebuild aclocal.m4 if any of the macros in `macros/' change
@MAINT@include macros/macros.dep
@MAINT@macros/macros.dep: macros/Makefile.am
@MAINT@	cd macros && $(MAKE) macros.dep
