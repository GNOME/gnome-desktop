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

desktop_in_files = \
	Emacs.desktop.in	\
	Eterm.desktop.in	\
	FileRunner.desktop.in	\
	gaspell.desktop.in	\
	GIMP.desktop.in		\
	Gmc.desktop.in		\
	GXanim.desktop.in	\
	Imlib_config.desktop.in	\
	Kterm.desktop.in	\
	lynx.desktop.in		\
	Netscape.desktop.in	\
	realplayer.desktop.in	\
	rxvt.desktop.in		\
	Tkrat.desktop.in	\
	TkSETI.desktop.in	\
	UNIX.desktop.in		\
	UNIX-regular.desktop.in	\
	Vim.desktop.in		\
	Vnterm.desktop.in	\
	WordPerfect.desktop.in	\
	xcolsel.desktop.in	\
	Xemacs.desktop.in	\
	XEphem.desktop.in	\
	XMcd.desktop.in		\
	xmix.desktop.in		\
	xv.desktop.in		\
	GIMP.desktop.in		\
	Gmc.desktop.in
gnew_DATA = $(desktop_in_files:.desktop.in=.desktop)
@XML_I18N_MERGE_DESKTOP_RULE@


directory_in_files = \
	Applications.directory.in	\
	Development.directory.in	\
	Games.directory.in		\
	Graphics.directory.in		\
	Internet.directory.in		\
	Multimedia.directory.in		\
	Root.directory.in		\
	Settings.directory.in		\
	System.directory.in		\
	Utilities.directory.in

noinst_DATA = $(directory_in_files:.directory.in=.directory)
@XML_I18N_MERGE_DIRECTORY_RULE@

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
	$(Settings_DATA) $(noinst_DATA) $(directory_in_files) \
	$(Directory_files) $(Order_files) $(Root_DATA) $(Settings_Order_files) \
	$(desktop_in_files) $(gnew_DATA)

install-data-local: $(Directory_files)
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
	$(INSTALL_DATA) Root.directory $(DESTDIR)$(Rootdir)/.directory
	$(INSTALL_DATA) Applications.directory $(DESTDIR)$(Applicationsdir)/.directory
	$(INSTALL_DATA) Graphics.directory $(DESTDIR)$(Graphicsdir)/.directory
	$(INSTALL_DATA) System.directory $(DESTDIR)$(Systemdir)/.directory
	$(INSTALL_DATA) Utilities.directory $(DESTDIR)$(Utilitiesdir)/.directory
	$(INSTALL_DATA) Multimedia.directory $(DESTDIR)$(Multimediadir)/.directory
	$(INSTALL_DATA) Internet.directory $(DESTDIR)$(Internetdir)/.directory
	$(INSTALL_DATA) Games.directory $(DESTDIR)$(Gamesdir)/.directory
	$(INSTALL_DATA) Settings.directory $(DESTDIR)$(Settingsdir)/.directory
	$(INSTALL_DATA) Development.directory $(DESTDIR)$(Developmentdir)/.directory
