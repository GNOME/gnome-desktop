#include <gnome.h>

typedef struct {
	const gchar *name;
	const gchar *email;
} author;

/* Please keep this in alphabetical order */
static author authors[] = {
	{ N_("GNOME was brought to you by"), "" },
	{ "", "" },
	{ "Martin Baulig", "<martin@home-of-linux.org" },
	{ "Jacob Berkman", "<jberkman@andrew.cmu.edu>" },
	{ "Jonathan Blandford", "<jrb@redhat.com>" },
	{ "Anders Carlsson", "<andersca@gnu.org>" },	
	{ "GEGL", "" },
	{ "Tim Gerla", "<timg@means.net>" },
	{ "James Henstridge", "<james@daa.com.au>" },
	{ "Miguel de Icaza", "<miguel@gnu.org>" },
	{ "George Lebl", "<jirka@5z.com>" },
	{ "Elliot Lee", "<sopwith@redhat.com>" },
	{ "Ian Main", "<slow@intergate.bc.ca>" },
	{ "Kjartan Maraas", "<kmaraas@online.no>" },
	{ "Federico Mena-Quintero", "<federico@gimp.org>" },
	{ "Jaka Mocnik", "<jaka.mocnik@kiss.uni-lj.si>" },
        { "Tomas Ögren", "<stric@ing.umu.se>" },
	{ "Ian Peters", "<itp@gnu.org" },
	{ "The Squeaky Rubber Gnome", "<squeak>" },
	{ "Owen Taylor", "<otaylor@redhat.com>" },
	{ "Peter Teichman", "<pat4@acpub.duke.edu>" },
	{ "Tom Tromey", "<tromey@cygnus.com>" },
	{ "Michael Zucchi", "<zucchi@zedzone.mmc.com.au>" },
	{ "", "" },
	{ N_("... and many more"), "" },
	{ NULL, NULL }
};
