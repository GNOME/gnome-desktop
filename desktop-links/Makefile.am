Directorydir = $(datadir)/gnome/vfolders

Directory_in_files =			\
	Root.directory.in		\
	Accessories.directory.in	\
	Applications.directory.in	\
	Graphics.directory.in		\
	System.directory.in		\
	Multimedia.directory.in		\
	Internet.directory.in		\
	Games.directory.in		\
	Development.directory.in	\
	Office.directory.in		\
	Settings.directory.in		\
	Other.directory.in

Directory_DATA = $(Directory_in_files:.directory.in=.directory)
@INTLTOOL_DIRECTORY_RULE@

EXTRA_DIST = $(Directory_in_files)

CLEANFILES = $(Directory_DATA)
