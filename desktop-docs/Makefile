# automake does not yet dig .sgml files, so for now the DocBook files
# must be extra_dist.  Also, in the future we should distribute the
# HTML files so that people without DocBook can still use the docs.
EXTRA_DIST = gnome-intro.sgml
GNOME_INTRO_GIFS = pix/panel-basic.gif pix/panel-menu.gif \
	pix/panel-menu-select.gif

manual-html: gnome-intro/book01.html
manual-ps: gnome-intro.ps
manual-rtf: gnome-intro.rtf
manual-all: manual-html manual-ps manual-rtf

%.dvi: %.sgml
	db2dvi $<

gnome-intro/book01.html: gnome-intro.sgml
	db2html gnome-intro.sgml
	cp -r pix gnome-intro
gnome-intro.ps: gnome-intro.dvi
	dvips gnome-intro.dvi
gnome-intro.rtf: gnome-intro.sgml
	db2rtf gnome-intro.sgml
