if PROGRAM_GMENU
gmenu = gmenu
endif

SUBDIRS = intl po macros \
	 panel applets \
	 gsm smproxy pixmaps desktop-links desktop-properties help-browser \
	 gnome-terminal core-docs $(gmenu)

## to automatically rebuild aclocal.m4 if any of the macros in `macros/' change
@MAINT@include macros/macros.dep
@MAINT@macros/macros.dep: macros/Makefile.am
@MAINT@	cd macros && $(MAKE) macros.dep
