# automake does not yet dig .sgml files, so for now the DocBook files
# must be extra_dist.  Also, in the future we should distribute the
# HTML files so that people without DocBook can still use the docs.

# This is ugly and quickly copied from the gtt approach because I need
# to get the help files properly installed and the Red Hat 5.1 freeze
# is today (1998 May 4)
gnome_intro_helpdir = $(datadir)/gnome/help/gnome-intro/C

GNOME_INTRO_HTML_FILES = \
	$(srcdir)/gnome-intro/c93191.html \
	$(srcdir)/gnome-intro/f13.html \
	$(srcdir)/gnome-intro/a-sample-session.html \
	$(srcdir)/gnome-intro/c93230.html \
	$(srcdir)/gnome-intro/f1346.html \
	$(srcdir)/gnome-intro/a-sample-session75.html \
	$(srcdir)/gnome-intro/c93302.html \
	$(srcdir)/gnome-intro/gnome-intro.html \
	$(srcdir)/gnome-intro/c93.html \
	$(srcdir)/gnome-intro/c93372.html \
	$(srcdir)/gnome-intro/c93127.html \
	$(srcdir)/gnome-intro/c93412.html \
	$(srcdir)/gnome-intro/c93175.html \
	$(srcdir)/gnome-intro/docbook.css*

GNOME_INTRO_PICTURES = $(srcdir)/pix/terminal.gif \
	$(srcdir)/pix/gedit-2files.gif \
	$(srcdir)/pix/ghex-2files.gif \
	$(srcdir)/pix/ghex-basic.gif \
	$(srcdir)/pix/gmc-basic.gif \
	$(srcdir)/pix/help-browser-initial.gif \
	$(srcdir)/pix/help-browser-sample.gif \
	$(srcdir)/pix/help-icon.gif \
	$(srcdir)/pix/panel-basic.gif \
	$(srcdir)/pix/panel-menu-select.gif \
	$(srcdir)/pix/panel-menu.gif \
	$(srcdir)/pix/panel-icons.gif \
	$(srcdir)/pix/panel-add-icon.gif \
	$(srcdir)/pix/gmix.gif \
	$(srcdir)/pix/gtcd.gif \
	$(srcdir)/pix/gtop-procs.gif \
	$(srcdir)/pix/gtcd-track-list.gif \
	$(srcdir)/pix/gtcd-cddb.gif \
	$(srcdir)/pix/gulp.gif \
	$(srcdir)/pix/calendar.gif \
	$(srcdir)/pix/ee-menu.gif \
	$(srcdir)/pix/gtop-mem.gif \
	$(srcdir)/pix/gtop-kill.gif \
	$(srcdir)/pix/gtt-basic.gif \
	$(srcdir)/pix/background-properties.gif \
	$(srcdir)/pix/gtt-new.gif \
	$(srcdir)/pix/gtt-2proj.gif \
	$(srcdir)/pix/gcalc.gif \
	$(srcdir)/pix/freecell.gif \
	$(srcdir)/pix/mahjongg.gif \
	$(srcdir)/pix/same-gnome.gif \
	$(srcdir)/pix/gnomine.gif

install-data-local: gnome-intro.html
	$(mkinstalldirs) $(gnome_intro_helpdir)
	$(mkinstalldirs) $(gnome_intro_helpdir)/pix
	$(INSTALL_DATA) $(srcdir)/topic.dat $(gnome_intro_helpdir)/topic.dat
	$(INSTALL_DATA) $(srcdir)/gnome-intro.html $(gnome_intro_helpdir)/index.html
	$(INSTALL_DATA) $(GNOME_INTRO_HTML_FILES) $(gnome_intro_helpdir)
	$(INSTALL_DATA) $(GNOME_INTRO_PICTURES) $(gnome_intro_helpdir)/pix

INTRO_EXTRA_DIST = $(srcdir)/gnome-intro.sgml
GNOME_INTRO_GIFS = $(srcdir)/pix/panel-basic.gif $(srcdir)/pix/panel-menu.gif \
	$(srcdir)/pix/panel-menu-select.gif

EXTRA_DIST = $(INTRO_EXTRA_DIST) $(GNOME_INTRO_HTML_FILES) \
	$(GNOME_INTRO_GIFS) $(GNOME_INTRO_PICTURES) $(srcdir)/topic.dat



manual-html: gnome-intro/gnome-intro.html
	cp gnome-intro/gnome-intro.html gnome-intro.html
manual-ps: gnome-intro.ps
manual-rtf: gnome-intro.rtf
manual-all: manual-html manual-ps manual-rtf

%.dvi: %.sgml
	db2dvi $<

%.ps: %.dvi
	dvips -o $@ $<

%/%.html: %.sgml
	db2html $<

%.rtf: %.sgml
	db2rtf $<

gnome-intro.html: gnome-intro/gnome-intro.html
	cp $< $@

# now a big ugly kludge: to handle the CVS directory that developers
# will have in the gnome-intro directory.  eventually I'll handle this
# by calling it gnome-intro-prebuilt or something
gnome-intro/gnome-intro.html: gnome-intro.sgml
	db2html gnome-intro.sgml
	-mkdir gnome-intro/pix
	cp pix/*.gif gnome-intro/pix
	if test -d gnome-intro.junk/CVS; then \
		echo "CVS directory got moved -- moving it back"; \
		mv gnome-intro.junk/CVS $(srcdir)/gnome-intro/CVS; \
	else echo "CVS directory did not get moved -- all is OK"; fi
