/* -*- Mode: C; c-set-style: gnu indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
#include <libbonobo.h>
#include <libgnome/gnome-init.h>
#include <gnome-desktop/GNOME_Desktop.h>
#include <gnome-desktop/gnome-desktop-item.h>
#include <locale.h>
#include <stdlib.h>

#include "bonobo-config-ditem.h"

static void G_GNUC_UNUSED
boot_ditem (Bonobo_ConfigDatabase db)
{
	BonoboArg *arg;
	CORBA_Environment ev;

	arg = bonobo_arg_new (TC_GNOME_Desktop_Entry);
	bonobo_pbclient_set_value (db, "/Desktop Entry", arg, NULL);
	bonobo_arg_release (arg);

	/* For some strange reason, this function causes the config moniker
	 * (bonobo-config-xmldb) to crash after successfully writing the file.
	 * So call this function to initialize the file and the comment it out.
	 */

	CORBA_exception_init (&ev);
	Bonobo_ConfigDatabase_sync (db, &ev);
	CORBA_exception_free (&ev);
}

static void G_GNUC_UNUSED
test_ditem (Bonobo_ConfigDatabase db)
{
	GnomeDesktopItem *ditem;
	GNOME_Desktop_EntryType type;
	const gchar *text;
	GSList *list, *c;

	ditem = gnome_desktop_item_new_from_file ("/tmp/test.desktop",
						  GNOME_DESKTOP_ITEM_LOAD_ONLY_IF_EXISTS);

	text = gnome_desktop_item_get_location (ditem);
	g_print ("LOCATION: |%s|\n", text);

	type = gnome_desktop_item_get_type (ditem);
	g_print ("TYPE: |%d|\n", type);

	text = gnome_desktop_item_get_command (ditem);
	g_print ("COMMAND: |%s|\n", text);

	text = gnome_desktop_item_get_icon_path (ditem);
	g_print ("ICON PATH: |%s|\n", text);

	text = gnome_desktop_item_get_name (ditem, NULL);
	g_print ("NAME: |%s|\n", text);

	text = gnome_desktop_item_get_name (ditem, "de");
	g_print ("NAME (de): |%s|\n", text);

	text = gnome_desktop_item_get_local_name (ditem);
	g_print ("LOCAL NAME: |%s|\n", text);

	text = gnome_desktop_item_get_comment (ditem, NULL);
	g_print ("COMMENT: |%s|\n", text);

	text = gnome_desktop_item_get_comment (ditem, "de");
	g_print ("COMMENT (de): |%s|\n", text);

	text = gnome_desktop_item_get_local_comment (ditem);
	g_print ("LOCAL COMMENT: |%s|\n", text);

	list = gnome_desktop_item_get_attributes (ditem);
	for (c = list; c; c = c->next) {
		const gchar *attr = c->data;

		g_print ("ATTRIBUTE: |%s|\n", attr);
	}

#if 1
	gnome_desktop_item_set_name (ditem, "de", "Neu gesetzt!");

	gnome_desktop_item_save (ditem, NULL);
#endif

	gnome_desktop_item_save (ditem, "/tmp/foo.desktop");
}

#if 0
static void G_GNUC_UNUSED
test_builtin (void)
{
	Bonobo_ConfigDatabase db, parent_db;
	CORBA_Environment ev;
	CORBA_TypeCode type;
	BonoboArg *value;
	gchar *string;

	db = bonobo_config_ditem_new ("/tmp/test.desktop");
	g_assert (db != CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);
	parent_db = bonobo_get_object ("xmldb:/tmp/foo.xmldb", "Bonobo/ConfigDatabase", &ev);
	g_assert (!BONOBO_EX (&ev) && parent_db != CORBA_OBJECT_NIL);
	CORBA_exception_free (&ev);

	CORBA_exception_init (&ev);
	Bonobo_ConfigDatabase_addDatabase (db, parent_db, "", 
					   Bonobo_ConfigDatabase_DEFAULT, &ev);
	g_assert (!BONOBO_EX (&ev));
	CORBA_exception_free (&ev);
}
#endif

int
main (int argc, char **argv)
{
	Bonobo_ConfigDatabase db = NULL;
	Bonobo_ConfigDatabase default_db = NULL;
	Bonobo_KeyList *dirlist, *keylist;
        CORBA_Environment  ev;
	CORBA_TypeCode type;
	CORBA_any *value;
	gchar *string;
	guint i, j;

        CORBA_exception_init (&ev);

	setlocale (LC_ALL, "");

#if 1
	gnome_program_init ("test-ditem", "0.01", &libgnome_module_info,
			    argc, argv, NULL);
#else
	if (!bonobo_init (&argc, argv))
		g_error ("Can not bonobo_init");
#endif

	bonobo_activate ();

	// test_builtin ();

	db = bonobo_config_ditem_new ("/tmp/test.desktop");

#if 0
        CORBA_exception_init (&ev);
	db = bonobo_get_object ("ditem:/tmp/test.desktop", "Bonobo/ConfigDatabase", &ev);
	g_assert (!BONOBO_EX (&ev));
        CORBA_exception_free (&ev);

        CORBA_exception_init (&ev);
	default_db = bonobo_get_object ("xmldb:/tmp/foo.xml", "Bonobo/ConfigDatabase", &ev);
	g_assert (!BONOBO_EX (&ev));
        CORBA_exception_free (&ev);

	g_assert (db != NULL);
	g_assert (default_db != NULL);
#endif

	test_ditem (db);

#if 0
        CORBA_exception_init (&ev);
	Bonobo_ConfigDatabase_addDatabase (db, default_db, "/gnome-ditem/",
					   Bonobo_ConfigDatabase_DEFAULT, &ev);
	g_assert (!BONOBO_EX (&ev));

	dirlist = Bonobo_ConfigDatabase_getDirs (db, "", &ev);
	g_assert (!BONOBO_EX (&ev));

	if (dirlist) {
		for (i = 0; i < dirlist->_length; i++) {
			g_print ("DIR: |%s|\n", dirlist->_buffer [i]);

			keylist = Bonobo_ConfigDatabase_getKeys (db, dirlist->_buffer [i], &ev);
			g_assert (!BONOBO_EX (&ev));

			if (keylist)
				for (j = 0; j < keylist->_length; j++)
					g_print ("KEY (%s): |%s|\n", dirlist->_buffer [i],
						 keylist->_buffer [j]);
	    }
	}

	keylist = Bonobo_ConfigDatabase_getKeys (db, "/Config/Foo", &ev);
	g_assert (!BONOBO_EX (&ev));

	if (keylist)
		for (j = 0; j < keylist->_length; j++)
			g_print ("TEST KEY: |%s|\n", keylist->_buffer [j]);
#endif

        CORBA_exception_init (&ev);
	type = bonobo_pbclient_get_type (db, "/Foo/Test", &ev);
	if (type)
		printf ("type is %d - %s (%s)\n", type->kind, type->name, type->repo_id);

        CORBA_exception_init (&ev);
	value = bonobo_pbclient_get_value (db, "/Foo/Test", TC_CORBA_long, &ev);
	if (value) {
		printf ("got value as long %d\n", BONOBO_ARG_GET_LONG (value));
		BONOBO_ARG_SET_LONG (value, 512);
		bonobo_pbclient_set_value (db, "/Foo/Test", value, &ev);
	}
	bonobo_arg_release (value);
        CORBA_exception_free (&ev);

        CORBA_exception_init (&ev);
	type = bonobo_pbclient_get_type (db, "/Foo/Test", &ev);
	if (type)
		g_message (G_STRLOC ": type is %d - %s (%s)", type->kind, type->name, type->repo_id);
	CORBA_exception_free (&ev);

        CORBA_exception_init (&ev);
	value = bonobo_pbclient_get_value (db, "/Foo/Test", TC_CORBA_long, &ev);
	if (value) {
		g_message (G_STRLOC ": got value as long %d", BONOBO_ARG_GET_LONG (value));
		BONOBO_ARG_SET_LONG (value, 512);
		bonobo_pbclient_set_value (db, "/Foo/Test", value, &ev);
	}
	bonobo_arg_release (value);
        CORBA_exception_free (&ev);

        CORBA_exception_init (&ev);
	type = bonobo_pbclient_get_type (db, "/Desktop Entry/Terminal", &ev);
	if (type)
		g_message (G_STRLOC ": type is %d - %s (%s)", type->kind, type->name, type->repo_id);
	CORBA_exception_free (&ev);

        CORBA_exception_init (&ev);
	type = bonobo_pbclient_get_type (db, "/Desktop Entry/Name", &ev);
	if (type)
		g_message (G_STRLOC ": type is %d - %s (%s)", type->kind, type->name, type->repo_id);
	CORBA_exception_free (&ev);

        CORBA_exception_init (&ev);
	type = bonobo_pbclient_get_type (db, "/Desktop Entry/Name[de]", &ev);
	if (type)
		g_message (G_STRLOC ": type is %d - %s (%s)", type->kind, type->name, type->repo_id);
	CORBA_exception_free (&ev);

        CORBA_exception_init (&ev);
	type = bonobo_pbclient_get_type (db, "/Desktop Entry/URL", &ev);
	if (type)
		g_message (G_STRLOC ": type is %d - %s (%s)", type->kind, type->name, type->repo_id);
	CORBA_exception_free (&ev);

	value = bonobo_pbclient_get_value (db, "/Desktop Entry/Name[de]", TC_GNOME_LocalizedString, &ev);
	g_message (G_STRLOC ": %p", value);
	if (value) {
		GNOME_LocalizedString localized;

		localized = BONOBO_ARG_GET_GENERAL (value, TC_GNOME_LocalizedString, GNOME_LocalizedString, NULL);
		g_message (G_STRLOC ": |%s| - |%s|", localized.locale, localized.text);
	}
	CORBA_exception_free (&ev);

	string = bonobo_pbclient_get_string (db, "/Desktop Entry/URL", NULL);
	g_message (G_STRLOC ": |%s|", string);
	bonobo_pbclient_set_string (db, "/Desktop Entry/URL", "http://www.gnome.org/", NULL);

#if 0
	CORBA_exception_init (&ev);
	Bonobo_ConfigDatabase_sync (db, &ev);
	g_assert (!BONOBO_EX (&ev));
        CORBA_exception_free (&ev);

        CORBA_exception_init (&ev);
	value = bonobo_pbclient_get_value (db, "/Desktop Entry", TC_GNOME_Desktop_Entry, &ev);
	g_message (G_STRLOC ": %p", value);
	if (value)
		printf ("got value as GNOME::DesktopEntry\n");
        CORBA_exception_free (&ev);
#endif

	exit (0);
}
