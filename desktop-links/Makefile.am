Directorydir = $(datadir)/gnome/vfolders

Directory_in_files =		\
	Root.directory		\
	Accessories.directory   \
	Applications.directory	\
	Graphics.directory	\
	System.directory	\
	Multimedia.directory	\
	Internet.directory	\
	Games.directory		\
	Development.directory   \
	Office.directory	\
	Settings.directory	\
	Other.directory

Directory_DATA = $(Directory_in_files:.directory.in=.directory)
@INTLTOOL_DIRECTORY_RULE@

EXTRA_DIST = $(Directory_DATA) $(Directory_in_files)
