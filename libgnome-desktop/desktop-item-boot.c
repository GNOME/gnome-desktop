/* -*- Mode: C; c-set-style: gnu indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
#include <libbonobo.h>
#include <libgnome/Gnome.h>

static void
boot_ditem (Bonobo_ConfigDatabase db)
{
	BonoboArg *arg;
	CORBA_Environment ev;

	arg = bonobo_arg_new (TC_GNOME_DesktopEntry);
	bonobo_pbclient_set_value (db, "/Desktop Entry", arg, NULL);
	bonobo_arg_release (arg);

	arg = bonobo_arg_new (TC_CORBA_long);
	bonobo_pbclient_set_value (db, "/Desktop Entry/X-GNOME-Long", arg, NULL);
	bonobo_arg_release (arg);

	/* For some strange reason, this function causes the config moniker
	 * (bonobo-config-xmldb) to crash after successfully writing the file.
	 * So call this function to initialize the file and the comment it out.
	 */

	CORBA_exception_init (&ev);
	Bonobo_ConfigDatabase_sync (db, &ev);
	g_assert (!BONOBO_EX (&ev));
	CORBA_exception_free (&ev);
}

int
main (int argc, char **argv)
{
	Bonobo_ConfigDatabase db = NULL;
	Bonobo_ConfigDatabase default_db = NULL;
	Bonobo_KeyList *dirlist, *keylist;
        CORBA_Environment  ev;
	CORBA_TypeCode type;
	CORBA_any *value;
	guint i, j;

        CORBA_exception_init (&ev);

	if (bonobo_init (&argc, argv) == FALSE)
		g_error ("Cannot init bonobo");

	bonobo_activate ();


        CORBA_exception_init (&ev);
	default_db = bonobo_get_object ("xmldb:" LIBGNOME_SYSCONFDIR "/gnome-2.0/gnome-desktop.xmldb",
					"Bonobo/ConfigDatabase", &ev);
	g_assert (!BONOBO_EX (&ev));
        CORBA_exception_free (&ev);

	g_assert (default_db != NULL);

	boot_ditem (default_db);

	exit (0);
}
