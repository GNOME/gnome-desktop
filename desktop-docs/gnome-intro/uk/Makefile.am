# automake does not yet dig .sgml files, so for now the DocBook files
# must be extra_dist.  Also, in the future we should distribute the
# HTML files so that people without DocBook can still use the docs.

# This is ugly and quickly copied from the gtt approach because I need
# to get the help files properly installed and the Red Hat 5.1 freeze
# is today (1998 May 4)
gnome_intro_helpdir = $(datadir)/gnome/help/gnome-intro/es

GNOME_INTRO_HTML_FILES = \
	c93191.html \
	f13.html \
	a-sample-session.html \
	c93230.html \
	f1346.html \
	a-sample-session75.html \
	c93302.html \
	gnome-intro.html \
	c93.html \
	c93372.html \
	c93127.html \
	c93412.html \
	c93175.html \
	docbook.css

GNOME_INTRO_PICTURES = \
	pix/terminal.gif \
	pix/gedit-2files.gif \
	pix/ghex-2files.gif \
	pix/ghex-basic.gif \
	pix/gmc-basic.gif \
	pix/help-browser-initial.gif \
	pix/help-browser-sample.gif \
	pix/help-icon.gif \
	pix/panel-basic.gif \
	pix/panel-menu-select.gif \
	pix/panel-menu.gif \
	pix/panel-icons.gif \
	pix/panel-add-icon.gif \
	pix/gmix.gif \
	pix/gtcd.gif \
	pix/gtop-procs.gif \
	pix/gtcd-track-list.gif \
	pix/gtcd-cddb.gif \
	pix/gulp.gif \
	pix/calendar.gif \
	pix/ee-menu.gif \
	pix/gtop-mem.gif \
	pix/gtop-kill.gif \
	pix/gtt-basic.gif \
	pix/background-properties.gif \
	pix/gtt-new.gif \
	pix/gtt-2proj.gif \
	pix/gcalc.gif \
	pix/freecell.gif \
	pix/mahjongg.gif \
	pix/same-gnome.gif \
	pix/gnomine.gif

# the "portable" `basename' stand-in should work for the files that we
# throw at it
install-data-local: gnome-intro.html
	$(mkinstalldirs) $(DESTDIR)$(gnome_intro_helpdir)
	$(mkinstalldirs) $(DESTDIR)$(gnome_intro_helpdir)/pix
	$(INSTALL_DATA) $(srcdir)/topic.dat $(DESTDIR)$(gnome_intro_helpdir)/topic.dat
	$(INSTALL_DATA) $(srcdir)/gnome-intro.html $(DESTDIR)$(gnome_intro_helpdir)/index.html
	for file in $(GNOME_INTRO_HTML_FILES); do \
	  basefile=`echo $$file | sed -e 's,^.*/,,'`; \
	  $(INSTALL_DATA) $(srcdir)/$$file $(DESTDIR)$(gnome_intro_helpdir)/$$basefile; \
	done
	for file in $(GNOME_INTRO_PICTURES); do \
	  basefile=`echo $$file | sed -e 's,^.*/,,'`; \
	  $(INSTALL_DATA) $(srcdir)/$$file $(DESTDIR)$(gnome_intro_helpdir)/pix/$$basefile; \
	done

GNOME_INTRO_GIFS = pix/panel-basic.gif pix/panel-menu.gif \
	pix/panel-menu-select.gif

EXTRA_DIST = $(GNOME_INTRO_HTML_FILES) \
	$(GNOME_INTRO_GIFS) $(GNOME_INTRO_PICTURES) \
	gnome-intro.html topic.dat # gnome-intro.sgml

manual-html: gnome-intro.html
manual-ps: gnome-intro.ps
manual-rtf: gnome-intro.rtf
manual-all: manual-html manual-ps manual-rtf

%.dvi: %.sgml
	db2dvi $<

%.ps: %.dvi
	dvips -o $@ $<

%/%.html: %.sgml
	-db2html $<

%.rtf: %.sgml
	db2rtf $<

