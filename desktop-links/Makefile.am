appsdir = $(datadir)/gnome/apps

Rootdir           = $(appsdir)
Applicationsdir   = $(appsdir)/Applications
Developmentdir	  = $(appsdir)/Development
Graphicsdir       = $(appsdir)/Graphics
Systemdir         = $(appsdir)/System
Utilitiesdir      = $(appsdir)/Utilities
Multimediadir     = $(appsdir)/Multimedia
Internetdir       = $(appsdir)/Internet
Gamesdir          = $(appsdir)/Games
Settingsdir       = $(appsdir)/Settings

gnewdir           = $(datadir)/mc/templates

gnew_DATA = GIMP.desktop 

Order_files = \
	Root.order

Settings_Order_files = \
	Settings.order

Directory_files = \
	Root.directory		\
	Applications.directory	\
	Graphics.directory	\
	System.directory	\
	Utilities.directory	\
	Multimedia.directory	\
	Internet.directory	\
	Games.directory		\
	Development.directory   \
	Settings.directory

Multimedia_DATA = \
	XEphem.desktop		\
	XMcd.desktop		\
	xmix.desktop      	\
	realplayer.desktop	\
	GXanim.desktop		\
	XMcd.desktop		\
	realplayer.desktop	\
	xmix.desktop


Utilities_DATA = \
	Xemacs.desktop		\
	gaspell.desktop   	\
	FileRunner.desktop	\
	TkSETI.desktop		\
	rxvt.desktop		\
	lynx.desktop		\
	xbmbrowser.desktop	\
	Vim.desktop		\
	xcolsel.desktop		\
	FileRunner.desktop

Applications_DATA = \
	Emacs.desktop		\
	Vim.desktop		\
	WordPerfect.desktop	\
	Xemacs.desktop		\
	XEphem.desktop		\
	gaspell.desktop

Graphics_DATA = \
	GIMP.desktop		\
	xv.desktop		\
	xbmbrowser.desktop

System_DATA = \
	UNIX.desktop		\
	UNIX-regular.desktop	\
	Eterm.desktop		\
	Kterm.desktop		\
	Vnterm.desktop

Settings_DATA =	\
	Imlib_config.desktop

Internet_DATA = \
	Netscape.desktop	\
	Tkrat.desktop	        \
	TkSETI.desktop		\
	Tkrat.desktop		\
	lynx.desktop

Root_DATA = \
	Gmc.desktop

## We explicitly define these so that the appropriate directories will
## be made at install time.
Multimedia_DATA =
Games_DATA =
Utilities_DATA = 

EXTRA_DIST = $(Applications_DATA) $(Graphics_DATA) $(Utilities_DATA) \
	$(System_DATA) $(Multimedia_DATA) $(Internet_DATA) $(Games_DATA) \
	$(Settings_DATA) \
	$(Directory_files) $(Order_files) $(Root_DATA) $(Settings_Order_files)

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(Applicationsdir)
	$(mkinstalldirs) $(DESTDIR)$(Graphicsdir)
	$(mkinstalldirs) $(DESTDIR)$(Systemdir)
	$(mkinstalldirs) $(DESTDIR)$(Utilitiesdir)
	$(mkinstalldirs) $(DESTDIR)$(Multimediadir)
	$(mkinstalldirs) $(DESTDIR)$(Internetdir)
	$(mkinstalldirs) $(DESTDIR)$(Gamesdir)
	$(mkinstalldirs) $(DESTDIR)$(Settingsdir)
	$(mkinstalldirs) $(DESTDIR)$(Developmentdir)
	$(mkinstalldirs) $(DESTDIR)$(datadir)/control-center/Session/

	$(INSTALL_DATA) $(srcdir)/Root.order $(DESTDIR)$(Rootdir)/.order
	$(INSTALL_DATA) $(srcdir)/Settings.order $(DESTDIR)$(Settingsdir)/.order
	$(INSTALL_DATA) $(srcdir)/Root.directory $(DESTDIR)$(Rootdir)/.directory
	$(INSTALL_DATA) $(srcdir)/Applications.directory $(DESTDIR)$(Applicationsdir)/.directory
	$(INSTALL_DATA) $(srcdir)/Graphics.directory $(DESTDIR)$(Graphicsdir)/.directory
	$(INSTALL_DATA) $(srcdir)/System.directory $(DESTDIR)$(Systemdir)/.directory
	$(INSTALL_DATA) $(srcdir)/Utilities.directory $(DESTDIR)$(Utilitiesdir)/.directory
	$(INSTALL_DATA) $(srcdir)/Multimedia.directory $(DESTDIR)$(Multimediadir)/.directory
	$(INSTALL_DATA) $(srcdir)/Internet.directory $(DESTDIR)$(Internetdir)/.directory
	$(INSTALL_DATA) $(srcdir)/Games.directory $(DESTDIR)$(Gamesdir)/.directory
	$(INSTALL_DATA) $(srcdir)/Settings.directory $(DESTDIR)$(Settingsdir)/.directory
	$(INSTALL_DATA) $(srcdir)/Development.directory $(DESTDIR)$(Developmentdir)/.directory
