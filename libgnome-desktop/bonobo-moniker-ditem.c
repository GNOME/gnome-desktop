/**
 * bonobo-config-ditem.c: ditem configuration database implementation.
 *
 * Author:
 *   Martin Baulig (baulig@suse.de)
 *
 * Copyright 2001 SuSE Linux AG.
 */
#include <config.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <bonobo/bonobo-arg.h>
#include <bonobo/bonobo-moniker.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo/bonobo-moniker-simple.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-shlib-factory.h>
#include <bonobo-config/bonobo-config-utils.h>

#include "bonobo-config-ditem.h"

#define EX_SET_NOT_FOUND(ev) bonobo_exception_set (ev, ex_Bonobo_Moniker_InterfaceNotFound)

static Bonobo_Unknown
ditem_resolve (BonoboMoniker               *moniker,
	       const Bonobo_ResolveOptions *options,
	       const CORBA_char            *requested_interface,
	       CORBA_Environment           *ev)
{
	Bonobo_Moniker         parent;
	Bonobo_ConfigDatabase  db, pdb = CORBA_OBJECT_NIL;
	const gchar           *name;

	if (strcmp (requested_interface, "IDL:Bonobo/ConfigDatabase:1.0")) {
		EX_SET_NOT_FOUND (ev);
		return CORBA_OBJECT_NIL; 
	}

	parent = bonobo_moniker_get_parent (moniker, ev);
	if (BONOBO_EX (ev))
		return CORBA_OBJECT_NIL;

	name = bonobo_moniker_get_name (moniker);


	if (parent != CORBA_OBJECT_NIL) {

		pdb = Bonobo_Moniker_resolve (parent, options, 
					      "IDL:Bonobo/ConfigDatabase:1.0", 
					      ev);
    
		bonobo_object_release_unref (parent, NULL);
		
		if (BONOBO_EX (ev) || pdb == CORBA_OBJECT_NIL)
			return CORBA_OBJECT_NIL;

	}

	if (!(db = bonobo_config_ditem_new (name))) {
		EX_SET_NOT_FOUND (ev);
		return CORBA_OBJECT_NIL; 
	}

	if (pdb != CORBA_OBJECT_NIL) {
		Bonobo_ConfigDatabase_addDatabase (db, pdb, "",ev);
		
		if (BONOBO_EX (ev)) {
			bonobo_object_release_unref (pdb, NULL);
			bonobo_object_release_unref (db, NULL);
			return CORBA_OBJECT_NIL; 
		}
	}

	return db;
}


static BonoboObject *
bonobo_moniker_ditem_factory (BonoboGenericFactory *this, 
			      const char           *object_id,
			      void                 *closure)
{
	static gboolean initialized = FALSE;

	if (!initialized) {
		initialized = TRUE;		
	}

	if (!strcmp (object_id, "OAFIID:Bonobo_Moniker_ditem")) {

		return BONOBO_OBJECT (bonobo_moniker_simple_new (
		        "ditem:", ditem_resolve));
	} else
		g_warning ("Failing to manufacture a '%s'", object_id);
	
	return NULL;
}

BONOBO_OAF_SHLIB_FACTORY_MULTI ("OAFIID:Bonobo_Moniker_ditem_Factory",
				"bonobo desktop item moniker",
				bonobo_moniker_ditem_factory,
				NULL);
