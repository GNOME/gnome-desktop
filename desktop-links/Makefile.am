NULL=

Desktopdir	  = $(datadir)/applications
Directorydir      = $(datadir)/gnome/vfolders
Orderdir          = $(datadir)/gnome/vfolders

Order_DATA = \
	Root.order \
	Settings.order

Directory_in_files = \
	Root.directory		\
	Accessories.directory   \
	Applications.directory	\
	Graphics.directory	\
	Home.directory		\
	System.directory	\
	Multimedia.directory	\
	Internet.directory	\
	Games.directory		\
	Development.directory   \
	Office.directory	\
	Settings.directory

Directory_DATA = $(Directory_in_files:.directory.in=.directory)
@INTLTOOL_DIRECTORY_RULE@

Desktop_in_files = \
	Xemacs.desktop		\
	rxvt.desktop		\
	Emacs.desktop		\
	GIMP.desktop		\
	xterm.desktop		\
	Netscape.desktop

Desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = $(Desktop_in_files) $(Directory_DATA) $(Order_DATA)
