# automake does not yet dig .sgml files, so for now the DocBook files
# must be extra_dist.  Also, in the future we should distribute the
# HTML files so that people without DocBook can still use the docs.

# This is ugly and quickly copied from the gtt approach because I need
# to get the help files properly installed and the Red Hat 5.1 freeze
# is today (1998 May 4)
gnome_intro_helpdir = $(datadir)/gnome/help/gnome-intro/C

GNOME_INTRO_HTML_FILES = \
	gnome-intro/book01.html gnome-intro/c0102.html gnome-intro/c0202.html \
	gnome-intro/c0205.html gnome-intro/c0208.html gnome-intro/f0102.html \
	gnome-intro/c01.html gnome-intro/c02.html gnome-intro/c0203.html \
	gnome-intro/c0206.html gnome-intro/f01.html gnome-intro/c0101.html \
	gnome-intro/c0201.html gnome-intro/c0204.html gnome-intro/c0207.html \
	gnome-intro/f0101.html

GNOME_INTRO_PICTURES = pix/terminal.gif \
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

install-data-local:
	$(mkinstalldirs) $(gnome_intro_helpdir)
	$(mkinstalldirs) $(gnome_intro_helpdir)/pix
	$(INSTALL_DATA) $(srcdir)/topic.dat $(gnome_intro_helpdir)/topic.dat
	$(INSTALL_DATA) $(srcdir)/gnome-intro.html $(gnome_intro_helpdir)/index.html
	$(INSTALL_DATA) $(GNOME_INTRO_HTML_FILES) $(gnome_intro_helpdir)
	$(INSTALL_DATA) $(GNOME_INTRO_PICTURES) $(gnome_intro_helpdir)/pix

INTRO_EXTRA_DIST = gnome-intro.sgml
GNOME_INTRO_GIFS = pix/panel-basic.gif pix/panel-menu.gif \
	pix/panel-menu-select.gif


manual-html: gnome-intro/book01.html
	cp gnome-intro/book01.html gnome-intro.html
manual-ps: gnome-intro.ps
manual-rtf: gnome-intro.rtf
manual-all: manual-html manual-ps manual-rtf

%.dvi: %.sgml
	db2dvi $<

%.ps: %.dvi
	dvips -o $@ $<

%/book01.html: %.sgml
	db2html $<

%.rtf: %.sgml
	db2rtf $<

gnome-intro.html: gnome-intro/book01.html
	cp $< $@

# now a big ugly kludge: to handle the CVS directory that developers
# will have in the gnome-intro directory.  eventually I'll handle this
# by calling it gnome-intro-prebuilt or something
gnome-intro/book01.html: gnome-intro.sgml
	db2html gnome-intro.sgml
	mkdir gnome-intro/pix
	cp pix/*.gif gnome-intro/pix
	if test -d gnome-intro.junk/CVS; then \
		echo "CVS directory got moved -- moving it back"; \
		mv gnome-intro.junk/CVS $(srcdir)/gnome-intro/CVS; \
	else echo "CVS directory did not get moved -- all is OK"; fi
#gnome-intro.ps: gnome-intro.dvi
#	dvips gnome-intro.dvi
#gnome-intro.rtf: gnome-intro.sgml
#	db2rtf gnome-intro.sgml
