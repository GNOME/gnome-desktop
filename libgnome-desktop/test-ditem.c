/* -*- Mode: C; c-set-style: gnu indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
#include <libbonobo.h>
#include <libgnome/Gnome.h>
#include <libgnome/gnome-ditem.h>
#include <locale.h>

#include "bonobo-config-ditem.h"

static void G_GNUC_UNUSED
boot_ditem (Bonobo_ConfigDatabase db)
{
	BonoboArg *arg;
	CORBA_Environment ev;

	arg = bonobo_arg_new (TC_GNOME_DesktopEntry);
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
	GNOME_DesktopEntryType type;
	const gchar *text;
	GSList *list, *c;

	ditem = gnome_desktop_item_new_from_file ("/home/martin/work/test.desktop",
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

#if 0
	gnome_desktop_item_set_name (ditem, "de", "Neu gesetzt!");

	gnome_desktop_item_save (ditem, NULL);
#endif

	// gnome_desktop_item_save (ditem, "~/work/foo.desktop");
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

	setlocale (LC_ALL, "");

	if (bonobo_init (&argc, argv) == FALSE)
		g_error ("Cannot init bonobo");

	bonobo_activate ();

	// db = bonobo_config_ditem_new ("~/work/test.desktop");

        CORBA_exception_init (&ev);
	db = bonobo_get_object ("ditem:~/work/test.desktop", "Bonobo/ConfigDatabase", &ev);
	g_assert (!BONOBO_EX (&ev));
        CORBA_exception_free (&ev);

        CORBA_exception_init (&ev);
	default_db = bonobo_get_object ("xmldb:~/work/foo.xml", "Bonobo/ConfigDatabase", &ev);
	g_assert (!BONOBO_EX (&ev));
        CORBA_exception_free (&ev);

	g_assert (db != NULL);
	g_assert (default_db != NULL);

	test_ditem (db);

        CORBA_exception_init (&ev);
	Bonobo_ConfigDatabase_addDatabase (db, default_db, "/gnome-ditem/", &ev);
	g_assert (!BONOBO_EX (&ev));

#if 0
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
	Bonobo_ConfigDatabase_sync (db, &ev);
        CORBA_exception_free (&ev);

        CORBA_exception_init (&ev);
	value = bonobo_pbclient_get_value (db, "/Desktop Entry", TC_GNOME_DesktopEntry, &ev);
	g_message (G_STRLOC ": %p", value);
	if (value)
		printf ("got value as string (%s)\n", BONOBO_ARG_GET_STRING (value));
        CORBA_exception_free (&ev);

        CORBA_exception_init (&ev);
	value = bonobo_pbclient_get_value (db, "/Desktop Entry",
					   TC_GNOME_LocalizedStringList, &ev);
	g_message (G_STRLOC ": %p", value);
	if (value)
		printf ("got value as string (%s)\n", BONOBO_ARG_GET_STRING (value));
        CORBA_exception_free (&ev);

	exit (0);

        CORBA_exception_init (&ev);

	value = bonobo_pbclient_get_value (db, "/storagetype",
					   TC_Bonobo_StorageType, &ev);
	if (value) {
		printf ("got value\n");
	}
        CORBA_exception_init (&ev);

	value = bonobo_arg_new (TC_CORBA_long);
	BONOBO_ARG_SET_LONG (value, 5);
	
	Bonobo_ConfigDatabase_setValue (db, "/test", value, &ev);
	g_assert (!BONOBO_EX (&ev));

	Bonobo_ConfigDatabase_setValue (db, "/test/level2", value, &ev);
	g_assert (!BONOBO_EX (&ev));

	Bonobo_ConfigDatabase_setValue (db, "/test/level21", value, &ev);
	g_assert (!BONOBO_EX (&ev));

	Bonobo_ConfigDatabase_setValue (db, "/test/level3/level3", value, &ev);
	g_assert (!BONOBO_EX (&ev));

	Bonobo_ConfigDatabase_setValue (db, "/bonobo/test1", value, &ev);
	g_assert (!BONOBO_EX (&ev));

	value = bonobo_arg_new (TC_Bonobo_StorageType);
	
	Bonobo_ConfigDatabase_setValue (db, "/storagetype", value, &ev);
	g_assert (!BONOBO_EX (&ev));

	value = bonobo_arg_new (TC_CORBA_string);
	BONOBO_ARG_SET_STRING (value, "a simple test");
	Bonobo_ConfigDatabase_setValue (db, "a/b/c/d", value, &ev);
	g_assert (!BONOBO_EX (&ev));

	Bonobo_ConfigDatabase_removeDir (db, "/", &ev);
	g_assert (!BONOBO_EX (&ev));

	value = bonobo_arg_new (TC_CORBA_long);
	BONOBO_ARG_SET_LONG (value, 5);
	
	Bonobo_ConfigDatabase_setValue (db, "/test", value, &ev);
	g_assert (!BONOBO_EX (&ev));

	Bonobo_ConfigDatabase_setValue (db, "/test/level2", value, &ev);
	g_assert (!BONOBO_EX (&ev));

	Bonobo_ConfigDatabase_setValue (db, "/test/level21", value, &ev);
	g_assert (!BONOBO_EX (&ev));

	Bonobo_ConfigDatabase_setValue (db, "/test/level3/level3", value, &ev);
	g_assert (!BONOBO_EX (&ev));


	value = bonobo_arg_new (TC_Bonobo_StorageType);
	Bonobo_ConfigDatabase_setValue (db, "/storagetype", value, &ev);
	g_assert (!BONOBO_EX (&ev));

	Bonobo_ConfigDatabase_sync (db, &ev);
	g_assert (!BONOBO_EX (&ev));


	/*
	{
		BonoboConfigBag     *config_bag;
		Bonobo_Unknown       bag, v;
		char *name;

		config_bag = bonobo_config_bag_new (db, "/bonobo");

		g_assert (config_bag);

		bag = BONOBO_OBJREF (config_bag);

		prop =Bonobo_PropertyBag_getPropertyByName (bag, "test1", &ev);
		g_assert (!BONOBO_EX (&ev));
	    
	
		printf("TEST3\n");
		name = Bonobo_Property_getName (prop, &ev);
		printf("TEST3e %s\n", name);
		g_assert (!BONOBO_EX (&ev));


	}
	*/
		

	exit (0);
}
