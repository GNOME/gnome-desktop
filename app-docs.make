helpdir = $(datadir)/gnome/help/$(app)/$(lang)
help_DATA = \
	index.html	\
	topic.dat	\
	$(figs)

EXTRA_DIST = $(app).sgml $(help_DATA)

all: index.html

index.html: $(app)/index.html
	-cp $(app)/index.html .

# the wierd srcdir trick is because the db2html from the Cygnus RPMs
# cannot handle relative filenames
$(app)/index.html: $(srcdir)/$(app).sgml
	-srcdir=`cd $(srcdir) && pwd`; \
	db2html $$srcdir/$(app).sgml

app-dist-hook: index.html
	-$(mkinstalldirs) $(distdir)/$(app)/stylesheet-images
	-$(mkinstalldirs) $(distdir)/figures
	-cp $(srcdir)/$(app)/*.html $(distdir)/$(app)
	-cp $(srcdir)/$(app)/*.css $(distdir)/$(app)
	-cp $(srcdir)/$(app)/stylesheet-images/*.png \
		$(distdir)/$(app)/stylesheet-images
	-cp $(srcdir)/figures/*.png \
		$(distdir)/figures

install-data-am: index.html
	-$(mkinstalldirs) $(DESTDIR)$(helpdir)/stylesheet-images
	-$(mkinstalldirs) $(DESTDIR)$(helpdir)/figures
	-for file in $(srcdir)/$(app)/*.html $(srcdir)/$(app)/*.css; do \
	  basefile=`echo $$file | sed -e 's,^.*/,,'`; \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(helpdir)/$$basefile; \
	done
	-for file in $(srcdir)/figures/*.png; do \
	  basefile=`echo $$file | sed -e  's,^.*/,,'`; \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(helpdir)/figures/$$basefile; \
	done
	-for file in $(srcdir)/$(app)/stylesheet-images/*.png; do \
	  basefile=`echo $$file | sed -e  's,^.*/,,'`; \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(helpdir)/stylesheet-images/$$basefile; \
	done

$(app).ps: $(srcdir)/$(app).sgml
	-srcdir=`cd $(srcdir) && pwd`; \
	db2ps $$srcdir/$(app).sgml

$(app).rtf: $(srcdir)/$(app).sgml
	-srcdir=`cd $(srcdir) && pwd`; \
	db2ps $$srcdir/$(app).sgml

