#include <gnome.h>

typedef struct {
	const gchar *name;
	const gchar *email;
} author;

static author authors[] = {
	{ N_("GNOME was brought to you by"), "" },
	{ "", "" },
	{ "Anders Carlsson", "<andersca@gnu.org>" },
	{ "Jacob Berkman", "<jberkman@andrew.cmu.edu>" },
	{ "Kjartan Maraas", "<kmaraas@online.no>" },
	{ "", "" },
	{ N_("...and many more"), "" },
	{NULL, NULL}
};
