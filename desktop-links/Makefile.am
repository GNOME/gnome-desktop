appsdir = $(datadir)/apps

Applicationdir = $(appsdir)/Applications
Graphicsdir    = $(appsdir)/Graphics
Systemdir      = $(appsdir)/System
Utilitiesdir   = $(appsdir)/Utilities
Audiodir       = $(appsdir)/Audio
Networkdir     = $(appsdir)/Network
Gamesdir       = $(appsdir)/Games

Application_DATA = 	\
	Emacs.desktop	\
	Netscape.desktop	

Graphics_DATA    = 	\
	GIMP.desktop

Utilities_DATA   = 	\
	UNIX.desktop	\
	UNIX-regular.desktop

## We explicitly define these so that the appropriate directories will
## be made at install time.
System_DATA =
Audio_DATA =
Network_DATA =
Games_DATA =

EXTRA_DIST = $(Application_DATA) $(Graphics_DATA) $(Utilities_DATA) \
	$(System_DATA) $(Audio_DATA) $(Network_DATA) $(Games_DATA)
