appsdir = $(datadir)/apps

Rootdir         = $(appsdir)
Applicationsdir = $(appsdir)/Applications
Graphicsdir     = $(appsdir)/Graphics
Systemdir       = $(appsdir)/System
Utilitiesdir    = $(appsdir)/Utilities
Audiodir        = $(appsdir)/Audio
Networkdir      = $(appsdir)/Network
Gamesdir        = $(appsdir)/Games

Directory_files = \
	Applications.directory \
	Graphics.directory     \
	System.directory       \
	Utilities.directory    \
	Audio.directory        \
	Network.directory      \
	Games.directory

Applications_DATA = 	     \
	Emacs.desktop	     \
	Netscape.desktop

Graphics_DATA    = 	     \
	GIMP.desktop

Utilities_DATA   = 	     \
	UNIX.desktop	     \
	UNIX-regular.desktop \
	Eterm.desktop
	
System_DATA      =           \
	Imlib_config.desktop

Network_DATA     =           \
	Balsa.desktop        \
	Tkrat.desktop

Root_DATA        =           \
	Gmc.desktop

## We explicitly define these so that the appropriate directories will
## be made at install time.
Audio_DATA =
Games_DATA =

EXTRA_DIST = $(Applications_DATA) $(Graphics_DATA) $(Utilities_DATA) \
	$(System_DATA) $(Audio_DATA) $(Network_DATA) $(Games_DATA) \
	$(Directory_files) $(Root_DATA)

install-data-local:
	$(mkinstalldirs) $(Applicationsdir)
	$(mkinstalldirs) $(Graphicsdir)
	$(mkinstalldirs) $(Systemdir)
	$(mkinstalldirs) $(Utilitiesdir)
	$(mkinstalldirs) $(Audiodir)
	$(mkinstalldirs) $(Networkdir)
	$(mkinstalldirs) $(Gamesdir)
	$(INSTALL_DATA) $(srcdir)/Applications.directory $(Applicationsdir)/.directory
	$(INSTALL_DATA) $(srcdir)/Graphics.directory $(Graphicsdir)/.directory
	$(INSTALL_DATA) $(srcdir)/System.directory $(Systemdir)/.directory
	$(INSTALL_DATA) $(srcdir)/Utilities.directory $(Utilitiesdir)/.directory
	$(INSTALL_DATA) $(srcdir)/Audio.directory $(Audiodir)/.directory
	$(INSTALL_DATA) $(srcdir)/Network.directory $(Networkdir)/.directory
	$(INSTALL_DATA) $(srcdir)/Games.directory $(Gamesdir)/.directory
