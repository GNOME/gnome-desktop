/* -*- Mode: C; c-set-style: gnu indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
#include <libbonobo.h>

#include "bonobo-config-ditem.h"

int
main (int argc, char **argv)
{
	Bonobo_ConfigDatabase db = NULL;
	Bonobo_KeyList *dirlist, *keylist;
        CORBA_Environment  ev;
	CORBA_any *value;
	guint i, j;

        CORBA_exception_init (&ev);

	if (bonobo_init (&argc, argv) == FALSE)
		g_error ("Cannot init bonobo");

#if 1
	db = bonobo_config_ditem_new ("~/work/test.desktop");
#else
        CORBA_exception_init (&ev);
	db = bonobo_get_object ("xmldb:~/work/foo.xml", "Bonobo/ConfigDatabase", &ev);
	g_assert (!BONOBO_EX (&ev));
        CORBA_exception_free (&ev);
#endif

	g_assert (db != NULL);

        CORBA_exception_init (&ev);

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

        CORBA_exception_init (&ev);
	value = bonobo_pbclient_get_value (db, "/Config/scrollbacklines", TC_long, &ev);
	if (value)
		printf ("got value as long %d\n", BONOBO_ARG_GET_LONG (value));
        CORBA_exception_free (&ev);

        CORBA_exception_init (&ev);
	value = bonobo_pbclient_get_value (db, "/Config/scrollbacklines", TC_string, &ev);
	if (value)
		printf ("got value as string %s\n", BONOBO_ARG_GET_STRING (value));
        CORBA_exception_free (&ev);

	exit (0);

        CORBA_exception_init (&ev);

	value = bonobo_pbclient_get_value (db, "/storagetype",
					   TC_Bonobo_StorageType, &ev);
	if (value) {
		printf ("got value\n");
	}
        CORBA_exception_init (&ev);

	value = bonobo_arg_new (TC_long);
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

	value = bonobo_arg_new (TC_string);
	BONOBO_ARG_SET_STRING (value, "a simple test");
	Bonobo_ConfigDatabase_setValue (db, "a/b/c/d", value, &ev);
	g_assert (!BONOBO_EX (&ev));

	Bonobo_ConfigDatabase_removeDir (db, "/", &ev);
	g_assert (!BONOBO_EX (&ev));

	value = bonobo_arg_new (TC_long);
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
