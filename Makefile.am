SUBDIRS = intl po panel gsm pixmaps desktop-links desktop-properties \
	help-browser macros

## to automatically rebuild aclocal.m4 if any of the macros in `macros/' change
@MAINT@include macros/macros.dep
@MAINT@macros/macros.dep: macros/Makefile.am
@MAINT@ cd macros && $(MAKE) macros.dep
