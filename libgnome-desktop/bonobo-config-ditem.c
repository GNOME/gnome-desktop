/* -*- Mode: C; c-set-style: gnu indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
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
#include <stdarg.h>
#include <bonobo/bonobo-arg.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo-config/bonobo-config-utils.h>

#include "bonobo-config-ditem.h"
#include "bonobo-config-ditem-internals.h"

#include "GNOME_Desktop.h"

static GObjectClass *parent_class = NULL;

#define CLASS(o) BONOBO_CONFIG_DITEM_CLASS (G_OBJECT_GET_CLASS (o))

#define PARENT_TYPE BONOBO_CONFIG_DATABASE_TYPE
#define FLUSH_INTERVAL 30 /* 30 seconds */

struct _BonoboConfigDItemPrivate {
	guint                        time_id;
	gboolean                     modified;

	BonoboConfigDItemDirectory  *dir;

	BonoboEventSource           *es;
};

#ifdef FIXME
static void
dump_subkeys (FILE *f, BonoboConfigDItemKey *de)
{
	GSList *c;

	for (c = de->subvalues; c; c = c->next) {
		BonoboConfigDItemKey *subentry = c->data;

		if (subentry->value) {
			gchar *t;

			if (subentry->localestring)
				t = escape_utf8_string_and_dup (subentry->value);
			else
				t = escape_ascii_string_and_dup (subentry->value);

			fprintf (f, "%s[%s]=%s\n", de->name, subentry->name, t);

			g_free (t);
		}
	}
}

static void
dump_key (FILE *f, BonoboConfigDItemKey *de)
{
	if (de->value) {
		gchar *t;

		if (de->localestring)
			t = escape_utf8_string_and_dup (de->value);
		else
			t = escape_ascii_string_and_dup (de->value);

		fprintf (f, "%s=%s\n", de->name, t);

		g_free (t);
	}

	if (de->subvalues)
		dump_subkeys (f, de);
}

static void 
dump_section (FILE *f, BonoboConfigDItemSection *section)
{
	GSList *l;

	fprintf (f, "\n[%s]\n", section->name);
	for (l = section->root.subvalues; l; l = l->next) {
		BonoboConfigDItemKey *de = l->data;

		dump_key (f, de);
	}
}
#endif

static gboolean
save (const char *file_name, BonoboConfigDItemDirectory *dir)
{
#ifdef FIXME
	GSList *l;
	FILE *f;
	
	if ((f = fopen (file_name, "w"))==NULL)
		return FALSE;

	g_message (G_STRLOC ": |%s|", file_name);

	for (l = dir->sections; l; l = l->next) {
		BonoboConfigDItemSection *section = l->data;

		dump_section (f, section);
	}

	fclose (f);
	return TRUE;
#else
	return FALSE;
#endif
}

static CORBA_TypeCode
real_get_type (BonoboConfigDatabase *db,
	       const CORBA_char     *key,
	       CORBA_Environment    *ev)
{
	BonoboConfigDItem *ditem = BONOBO_CONFIG_DITEM (db);
	BonoboConfigDItemKey          *de;

	de = bonobo_config_ditem_lookup (ditem->_priv->dir, key, FALSE);
	if (!de) {
		bonobo_exception_set (ev, ex_Bonobo_PropertyBag_NotFound);
		return CORBA_OBJECT_NIL;
	}

	g_assert (de->tc != NULL);
	return de->tc;
}

static CORBA_any *
real_get_value (BonoboConfigDatabase *db,
		const CORBA_char     *key, 
		CORBA_Environment    *ev)
{
	BonoboConfigDItem *ditem = BONOBO_CONFIG_DITEM (db);
	BonoboConfigDItemKey          *de;
	CORBA_any         *value = NULL;
/*	char              *locale = NULL;  */
				
	/* fixme: how to handle locale correctly ? */

	de = bonobo_config_ditem_lookup (ditem->_priv->dir, key, FALSE);
	if (!de) {
		bonobo_exception_set (ev, ex_Bonobo_PropertyBag_NotFound);
		return NULL;
	}

	if (!de->dyn)
		return NULL;

	bonobo_config_ditem_sync_key (de);

	CORBA_exception_init (ev);

	value = DynamicAny_DynAny_to_any (de->dyn, ev);
	g_assert (!BONOBO_EX (ev));

	if (!value)
		bonobo_exception_set (ev, ex_Bonobo_PropertyBag_NotFound);

	return value;
}

static void
copy_to_keylist (gchar *name, BonoboConfigDItemKey *de, Bonobo_KeyList *key_list)
{
	key_list->_buffer [key_list->_length] = CORBA_string_dup (de->name);
	key_list->_length++;
}

static Bonobo_KeyList *
real_get_dirs (BonoboConfigDatabase *db,
	       const CORBA_char     *dir,
	       CORBA_Environment    *ev)
{
	BonoboConfigDItem *ditem = BONOBO_CONFIG_DITEM (db);
	Bonobo_KeyList *key_list;
	BonoboConfigDItemSection *section;
	GSList *l;
	int len;

	key_list = Bonobo_KeyList__alloc ();
	key_list->_length = 0;

	if (!(section = bonobo_config_ditem_lookup_section (ditem->_priv->dir, dir)))
		return key_list;

	len = g_hash_table_size (section->root.subvalues);
	if (!len)
		return key_list;

	key_list->_maximum = len;
	key_list->_buffer = CORBA_sequence_CORBA_string_allocbuf (len);
	CORBA_sequence_set_release (key_list, TRUE);

	g_hash_table_foreach (section->root.subvalues, (GHFunc) copy_to_keylist, key_list);
	
	return key_list;
}

static Bonobo_KeyList *
real_get_keys (BonoboConfigDatabase *db,
	       const CORBA_char     *dir,
	       CORBA_Environment    *ev)
{
	BonoboConfigDItem *ditem = BONOBO_CONFIG_DITEM (db);
	Bonobo_KeyList *key_list;
	BonoboConfigDItemSection *section;
	BonoboConfigDItemKey *de;
	GSList *l;
	int i, len;
	
	key_list = Bonobo_KeyList__alloc ();
	key_list->_length = 0;

	if (!(section = bonobo_config_ditem_lookup_section (ditem->_priv->dir, dir)))
		return key_list;

	len = g_hash_table_size (section->root.subvalues);
	if (!len)
		return key_list;

	key_list->_maximum = key_list->_length = len;
	key_list->_buffer = CORBA_sequence_CORBA_string_allocbuf (len);
	CORBA_sequence_set_release (key_list, TRUE); 

	g_hash_table_foreach (section->root.subvalues, (GHFunc) copy_to_keylist, key_list);

	return key_list;
}

static CORBA_boolean
real_has_dir (BonoboConfigDatabase *db,
	      const CORBA_char     *dir,
	      CORBA_Environment    *ev)
{
	BonoboConfigDItem *ditem = BONOBO_CONFIG_DITEM (db);

	if (bonobo_config_ditem_lookup_section (ditem->_priv->dir, dir))
		return TRUE;

	return FALSE;
}

static void
real_sync (BonoboConfigDatabase *db, 
	   CORBA_Environment    *ev)
{
	BonoboConfigDItem *ditem = BONOBO_CONFIG_DITEM (db);
	char *tmp_name;

	if (!db->writeable || !ditem->_priv->modified)
		return;

	tmp_name = g_strdup_printf ("%s.tmp.%d", ditem->filename, getpid ());

	if (!save (tmp_name, ditem->_priv->dir)) {
		g_free (tmp_name);
		db->writeable = FALSE;
		return;
	}

	if (rename (tmp_name, ditem->filename) < 0) {
		g_free (tmp_name);
		db->writeable = FALSE;
		return;
	}

	ditem->_priv->modified = FALSE;

	g_free (tmp_name);
	return;
}

static gboolean
timeout_cb (gpointer data)
{
	BonoboConfigDItem *ditem = BONOBO_CONFIG_DITEM (data);
	CORBA_Environment ev;

	CORBA_exception_init(&ev);

	real_sync (BONOBO_CONFIG_DATABASE (data), &ev);
	
	CORBA_exception_free (&ev);

	ditem->_priv->time_id = 0;

	/* remove the timeout */
	return FALSE;
}

static void
real_set_value (BonoboConfigDatabase *db,
		const CORBA_char     *key, 
		const CORBA_any      *value,
		CORBA_Environment    *ev)
{
	BonoboConfigDItem *ditem = BONOBO_CONFIG_DITEM (db);
	BonoboConfigDItemKey *de;

	de = bonobo_config_ditem_lookup (ditem->_priv->dir, key, TRUE);

	g_message (G_STRLOC ": |%s| - %p", key, de);

	if (!de)
		return;

	// bonobo_config_ditem_encode_any (ditem, de, key, value, ev);

	ditem->_priv->modified = TRUE;

	if (!ditem->_priv->time_id)
		ditem->_priv->time_id = g_timeout_add (FLUSH_INTERVAL * 1000, 
						       timeout_cb, ditem);
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

	bonobo_event_source_notify_listeners(ditem->_priv->es, ename, value, &ev);

	g_free (ename);
	
	if (!(dir_name = bonobo_config_dir_name (key)))
		dir_name = g_strdup ("");

	if (!(leaf_name = bonobo_config_leaf_name (key)))
		leaf_name = g_strdup ("");
	
	ename = g_strconcat ("Bonobo/ConfigDatabase:change", dir_name, ":", 
			     leaf_name, NULL);

	bonobo_event_source_notify_listeners (ditem->_priv->es, ename, value, &ev);
						   
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

	if (ditem->_priv != NULL) {

		bonobo_config_ditem_free_directory (ditem->_priv->dir);
		ditem->_priv->dir = NULL;

		if (ditem->_priv->es)
			bonobo_object_unref (BONOBO_OBJECT (ditem->_priv->es));
		ditem->_priv->es = CORBA_OBJECT_NIL;

	}

	g_free (ditem->_priv);
	ditem->_priv = NULL;

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

	cd_class->set_value    = real_set_value;
	cd_class->get_value    = real_get_value;
	cd_class->get_dirs     = real_get_dirs;
	cd_class->get_keys     = real_get_keys;
	cd_class->has_dir      = real_has_dir;
	cd_class->sync         = real_sync;
}

static void
bonobo_config_ditem_init (BonoboConfigDItem *ditem)
{
	ditem->_priv = g_new0 (BonoboConfigDItemPrivate, 1);
}

BONOBO_TYPE_FUNC (BonoboConfigDItem, PARENT_TYPE, bonobo_config_ditem);

Bonobo_ConfigDatabase
bonobo_config_ditem_new (const char *filename)
{
	BonoboConfigDItem     *ditem;
	Bonobo_ConfigDatabase  db;
	CORBA_Environment      ev;
	char                  *real_name;
	BonoboConfigDItemDirectory             *dir;

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

	dir = bonobo_config_ditem_load (real_name);
	if (dir == NULL) {
		g_free (real_name);
		return CORBA_OBJECT_NIL;
	}

	if (!(ditem = g_object_new (BONOBO_CONFIG_DITEM_TYPE, NULL))) {
		bonobo_config_ditem_free_directory (dir);
		g_free (real_name);
		return CORBA_OBJECT_NIL;
	}

	ditem->filename = real_name;
	ditem->_priv->dir = dir;

	BONOBO_CONFIG_DATABASE (ditem)->writeable = TRUE;

		       
	ditem->_priv->es = bonobo_event_source_new ();

	bonobo_object_add_interface (BONOBO_OBJECT (ditem), 
				     BONOBO_OBJECT (ditem->_priv->es));

	db = CORBA_Object_duplicate (BONOBO_OBJREF (ditem), NULL);

	bonobo_url_register ("BONOBO_CONF:DITEM", real_name, NULL, db, &ev);

	return db;
}
