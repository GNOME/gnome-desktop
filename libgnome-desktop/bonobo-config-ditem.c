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
#include <bonobo/bonobo-arg.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo-config/bonobo-config-utils.h>

#include "bonobo-config-ditem.h"

static GObjectClass *parent_class = NULL;

#define CLASS(o) BONOBO_CONFIG_DITEM_CLASS (G_OBJECT_GET_CLASS (o))

#define PARENT_TYPE BONOBO_CONFIG_DATABASE_TYPE
#define FLUSH_INTERVAL 30 /* 30 seconds */

static void
real_sync (BonoboConfigDatabase *db, 
	   CORBA_Environment    *ev)
{
	BonoboConfigDItem *ditem G_GNUC_UNUSED = BONOBO_CONFIG_DITEM (db);

	if (!db->writeable)
		return;

	return;
}

static gboolean G_GNUC_UNUSED
timeout_cb (gpointer data)
{
	BonoboConfigDItem *ditem = BONOBO_CONFIG_DITEM (data);
	CORBA_Environment ev;

	CORBA_exception_init(&ev);

	real_sync (BONOBO_CONFIG_DATABASE (data), &ev);
	
	CORBA_exception_free (&ev);

	ditem->time_id = 0;

	/* remove the timeout */
	return FALSE;
}

static void G_GNUC_UNUSED
notify_listeners (BonoboConfigDItem *ditem, 
		  const char        *key, 
		  const CORBA_any   *value)
{
	CORBA_Environment ev;
	char *dir_name;
	char *leaf_name;
	char *ename;

	if (!key)
		return;

	CORBA_exception_init(&ev);

	ename = g_strconcat ("Bonobo/Property:change:", key, NULL);

	bonobo_event_source_notify_listeners(ditem->es, ename, value, &ev);

	g_free (ename);
	
	if (!(dir_name = bonobo_config_dir_name (key)))
		dir_name = g_strdup ("");

	if (!(leaf_name = bonobo_config_leaf_name (key)))
		leaf_name = g_strdup ("");
	
	ename = g_strconcat ("Bonobo/ConfigDatabase:change", dir_name, ":", 
			     leaf_name, NULL);

	bonobo_event_source_notify_listeners (ditem->es, ename, value, &ev);
						   
	CORBA_exception_free (&ev);

	g_free (ename);
	g_free (dir_name);
	g_free (leaf_name);
}

static void
bonobo_config_ditem_finalize (GObject *object)
{
	BonoboConfigDItem *ditem = BONOBO_CONFIG_DITEM (object);
	CORBA_Environment      ev;

	CORBA_exception_init (&ev);

	bonobo_url_unregister ("BONOBO_CONF:DITEM", ditem->filename, &ev);
      
	CORBA_exception_free (&ev);

	if (ditem->es)
		bonobo_object_unref (BONOBO_OBJECT (ditem->es));

	parent_class->finalize (object);
}


static void
bonobo_config_ditem_class_init (BonoboConfigDatabaseClass *class)
{
	GObjectClass *object_class = (GObjectClass *) class;
	BonoboConfigDatabaseClass *cd_class;

	parent_class = g_type_class_peek_parent (class);

	object_class->finalize = bonobo_config_ditem_finalize;

	cd_class = BONOBO_CONFIG_DATABASE_CLASS (class);
}

static void
bonobo_config_ditem_init (BonoboConfigDItem *ditem)
{
}

BONOBO_TYPE_FUNC (BonoboConfigDItem, PARENT_TYPE, bonobo_config_ditem);

Bonobo_ConfigDatabase
bonobo_config_ditem_new (const char *filename)
{
	BonoboConfigDItem     *ditem;
	Bonobo_ConfigDatabase  db;
	CORBA_Environment      ev;
	char                  *real_name;

	g_return_val_if_fail (filename != NULL, NULL);

	CORBA_exception_init (&ev);

	if (filename [0] == '~' && filename [1] == '/')
		real_name = g_strconcat (g_get_home_dir (), &filename [1], 
					 NULL);
	else
		real_name = g_strdup (filename);

	db = bonobo_url_lookup ("BONOBO_CONF:DITEM", real_name, &ev);

	if (BONOBO_EX (&ev))
		db = CORBA_OBJECT_NIL;
	
	CORBA_exception_free (&ev);

	if (db) {
		g_free (real_name);
		return bonobo_object_dup_ref (db, NULL);
	}

	if (!(ditem = g_object_new (BONOBO_CONFIG_DITEM_TYPE, NULL))) {
		g_free (real_name);
		return CORBA_OBJECT_NIL;
	}

	ditem->filename = real_name;

	BONOBO_CONFIG_DATABASE (ditem)->writeable = TRUE;
		       
	ditem->es = bonobo_event_source_new ();

	bonobo_object_add_interface (BONOBO_OBJECT (ditem), 
				     BONOBO_OBJECT (ditem->es));

	db = CORBA_Object_duplicate (BONOBO_OBJREF (ditem), NULL);

	bonobo_url_register ("BONOBO_CONF:DITEM", real_name, NULL, db, &ev);

	return db;
}
