#include <gnome.h>

typedef struct {
	const gchar *name;
	const gchar *email;
} author;

/* Please keep this in alphabetical order */
static author authors[] = {
	{ N_("GNOME was brought to you by"), "" },
	{ "", "" },
	{ "Jacob Berkman", "<jberkman@andrew.cmu.edu>" },
	{ "Jonathan Blandford", "<jrb@redhat.com>" },
	{ "Anders Carlsson", "<andersca@gnu.org>" },
	{ "George Lebl", "<jirka@5z.com>" },
	{ "Kjartan Maraas", "<kmaraas@online.no>" },
	{ "Federico Mena-Quintero", "<federico@gimp.org>" },
	{ "Jaka Mocnik", "<jaka.mocnik@kiss.uni-lj.si>" },
	{ "The Squeaky Rubber Gnome", "http://www.gnome.org/squeaky-rubber-gnome.shtml" },
        { "Tomas Ögren", "<stric@ing.umu.se>" },
	{ "Peter Teichman", "<pat4@acpub.duke.edu>" },
	{ "", "" },
	{ N_("... and many more"), "" },
	{ NULL, NULL }
};
