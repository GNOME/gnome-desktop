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
	{ "Anders Carlsson", "<andersca@gnu.org>" },
	{ "George Lebl", "<jirka@5z.com>" },
	{ "Kjartan Maraas", "<kmaraas@online.no>" },
	{ "Federico Mena-Quintero", "<federico@gimp.org>" },
	{ "The Squeaky Rubber Gnome", "http://www.gnome.org/squeaky-rubber-gnome.shtml" },
	{ "", "" },
	{ N_("... and many more"), "" },
	{ NULL, NULL }
};
