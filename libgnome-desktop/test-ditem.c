#include <libbonobo.h>

#include "bonobo-config-ditem.h"

int
main (int argc, char **argv)
{
	Bonobo_ConfigDatabase db = NULL;
        CORBA_Environment  ev;
	CORBA_any *value;

        CORBA_exception_init (&ev);

	if (bonobo_init (&argc, argv) == FALSE)
		g_error ("Cannot init bonobo");

	db = bonobo_config_ditem_new ("/tmp/t.desktop");

	g_assert (db != NULL);

	value = bonobo_pbclient_get_value (db, "/test", TC_long, &ev);

	if (value) {
		printf ("got value %d\n", BONOBO_ARG_GET_LONG (value));
	}
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
