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
#include <bonobo/bonobo-arg.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo-config/bonobo-config-utils.h>

#include "bonobo-config-ditem.h"
#include "bonobo-config-ditem-utils.h"

static GObjectClass *parent_class = NULL;

#define CLASS(o) BONOBO_CONFIG_DITEM_CLASS (G_OBJECT_GET_CLASS (o))

#define PARENT_TYPE BONOBO_CONFIG_DATABASE_TYPE
#define FLUSH_INTERVAL 30 /* 30 seconds */

#define STRSIZE 4096
#define overflow (next == &CharBuffer [STRSIZE-1])

enum {
	FirstBrace,
	OnSecHeader,
	IgnoreToEOL,
	IgnoreToEOLFirst,
	KeyDef,
	KeyDefOnKey,
	KeyValue
};

typedef struct {
	int type;
	void *value;
} iterator_type;

typedef enum {
	LOOKUP,
	SET
} access_type;

typedef struct TKeys {
	char *key_name;
	char *value;
	struct TKeys *link;
} TKeys;

typedef struct TSecHeader {
	char *section_name;
	TKeys *keys;
	struct TSecHeader *link;
} TSecHeader;

struct _BonoboConfigDItemPrivate {
	guint                 time_id;

	TSecHeader           *dir;

	BonoboEventSource    *es;
};

static char *
decode_string_and_dup (char *s)
{
	char *p = g_malloc (strlen (s) + 1);
	char *q = p;

	do {
		if (*s == '\\'){
			switch (*(++s)){
			case 'n':
				*p++ = '\n';
				break;
			case '\\':
				*p++ = '\\';
				break;
			case 'r':
				*p++ = '\r';
				break;
			default:
				*p++ = '\\';
				*p++ = *s;
			}
		} else
			*p++ = *s;
	} while (*s++);
	return q;
}

static char *
escape_string_and_dup (char *s)
{
	char *return_value, *p = s;
	int len = 0;

	if(!s)
		return g_strdup("");
	
	while (*p){
		len++;
		if (*p == '\n' || *p == '\\' || *p == '\r' || *p == '\0')
			len++;
		p++;
	}
	return_value = p = (char *) g_malloc (len + 1);
	if (!return_value)
		return 0;
	do {
		switch (*s){
		case '\n':
			*p++ = '\\';
			*p++ = 'n';
			break;
		case '\r':
			*p++ = '\\';
			*p++ = 'r';
			break;
		case '\\':
			*p++ = '\\';
			*p++ = '\\';
			break;
		default:
			*p++ = *s;
		}
	} while (*s++);
	return return_value;
}

static TSecHeader *
load (const char *file)
{
	FILE *f;
	int state;
	TSecHeader *SecHeader = 0;
	char CharBuffer [STRSIZE];
	char *next = "";		/* Not needed */
	int c;
	
	if ((f = fopen (file, "r"))==NULL)
		return NULL;
	
	state = FirstBrace;
	while ((c = getc_unlocked (f)) != EOF){
		if (c == '\r')		/* Ignore Carriage Return */
			continue;
		
		switch (state){
			
		case OnSecHeader:
			if (c == ']' || overflow){
				*next = '\0';
				next = CharBuffer;
				SecHeader->section_name = g_strdup (CharBuffer);
				state = IgnoreToEOL;
			} else
				*next++ = c;
			break;

		case IgnoreToEOL:
		case IgnoreToEOLFirst:
			if (c == '\n'){
				if (state == IgnoreToEOLFirst)
					state = FirstBrace;
				else
					state = KeyDef;
				next = CharBuffer;
			}
			break;

		case FirstBrace:
		case KeyDef:
		case KeyDefOnKey:
			if (c == '#') {
				if (state == FirstBrace)
					state = IgnoreToEOLFirst;
				else
					state = IgnoreToEOL;
				break;
			}

			if (c == '[' && state != KeyDefOnKey){
				TSecHeader *temp;
		
				temp = SecHeader;
				SecHeader = (TSecHeader *) g_malloc (sizeof (TSecHeader));
				SecHeader->link = temp;
				SecHeader->keys = 0;
				state = OnSecHeader;
				next = CharBuffer;
				break;
			}
			/* On first pass, don't allow dangling keys */
			if (state == FirstBrace)
				break;
	    
			if ((c == ' ' && state != KeyDefOnKey) || c == '\t')
				break;
	    
			if (c == '\n' || overflow) { /* Abort Definition */
				next = CharBuffer;
				state = KeyDef;
                                break;
                        }
	    
			if (c == '=' || overflow){
				TKeys *temp;

				temp = SecHeader->keys;
				*next = '\0';
				SecHeader->keys = (TKeys *) g_malloc (sizeof (TKeys));
				SecHeader->keys->link = temp;
				SecHeader->keys->key_name = g_strdup (CharBuffer);
				state = KeyValue;
				next = CharBuffer;
			} else {
				*next++ = c;
				state = KeyDefOnKey;
			}
			break;

		case KeyValue:
			if (overflow || c == '\n'){
				*next = '\0';
				SecHeader->keys->value = decode_string_and_dup (CharBuffer);
				state = c == '\n' ? KeyDef : IgnoreToEOL;
				next = CharBuffer;
#ifdef GNOME_ENABLE_DEBUG
#endif
			} else
				*next++ = c;
			break;
	    
		} /* switch */
	
	} /* while ((c = getc_unlocked (f)) != EOF) */
	if (c == EOF && state == KeyValue){
		*next = '\0';
		SecHeader->keys->value = decode_string_and_dup (CharBuffer);
	}
	fclose (f);
	return SecHeader;
}

static TKeys *
dir_lookup_entry (TSecHeader *dir,
		  char       *name,
		  gboolean    create)
{
	TKeys *de;
	
	for (de = dir->keys; de != NULL; de = de->link)
		if (!strcmp (de->key_name, name))
			return de;

#if 0
	if (create) {

		de = g_new0 (DirEntry, 1);
		
		de->dir = dir;

		de->name = g_strdup (name);

		dir->entries = g_slist_prepend (dir->entries, de);

		return de;
	}
#endif

	return NULL;
}

static TSecHeader *
dir_lookup_subdir (TSecHeader  *dir,
		   char        *name,
		   gboolean     create)
{
	TSecHeader *dd;
	
	for (dd = dir; dd != NULL; dd = dd->link)
		if (!strcmp (dd->section_name, name))
			return dd;

#if 0
	if (create) {

		dd = g_new0 (DirData, 1);

		dd->dir = dir;

		dd->name = g_strdup (name);

		dir->subdirs = g_slist_prepend (dir->subdirs, dd);

		return dd;
	}
#endif

	return NULL;
}

static TSecHeader *
lookup_dir (TSecHeader *dir,
	    const char *path,
	    gboolean    create)
{
	const char *s, *e;
	char *name;
	TSecHeader *dd = dir;
	
	s = path;
	while (*s == '/') s++;
	
	if (*s == '\0')
		return dir;

	if ((e = strchr (s, '/')))
		name = g_strndup (s, e - s);
	else
		name = g_strdup (s);

	if (e) {
		g_free (name);
		return NULL;
	}
	
	if ((dd = dir_lookup_subdir (dd, name, create))) {
		g_free (name);
		return dd;
		
	}

	return NULL;
}

static TKeys *
lookup_dir_entry (BonoboConfigDItem *ditem,
		  const char        *key, 
		  gboolean           create)
{
	char       *dir_name;
	char       *leaf_name;
	TKeys      *de;
	TSecHeader *dd;

	if ((dir_name = bonobo_config_dir_name (key))) {
		dd = lookup_dir (ditem->_priv->dir, dir_name, create);
		
		g_free (dir_name);

	} else {
		dd = ditem->_priv->dir;
	}

	if (!dd)
		return NULL;

	if (!(leaf_name = bonobo_config_leaf_name (key)))
		return NULL;

	de = dir_lookup_entry (dd, leaf_name, create);

	g_free (leaf_name);

	return de;
}

static CORBA_TypeCode
real_get_type (BonoboConfigDatabase *db,
	       const CORBA_char     *key, 
	       CORBA_Environment    *ev)
{
	bonobo_exception_set (ev, ex_Bonobo_PropertyBag_NotFound);

	return CORBA_OBJECT_NIL;
}

static CORBA_any *
real_get_value (BonoboConfigDatabase *db,
		const CORBA_char     *key, 
		CORBA_Environment    *ev)
{
	BonoboConfigDItem *ditem = BONOBO_CONFIG_DITEM (db);
	TKeys             *de;
	CORBA_TypeCode     tc;
	CORBA_any         *value = NULL;
	char              *locale = NULL; 
				
	/* fixme: how to handle locale correctly ? */

	de = lookup_dir_entry (ditem, key, FALSE);
	if (!de) {
		bonobo_exception_set (ev, ex_Bonobo_PropertyBag_NotFound);
		return NULL;
	}

	tc = Bonobo_ConfigDatabase_getType (BONOBO_OBJREF (db), key, ev);
	if (BONOBO_EX (ev))
		tc = TC_string;

	CORBA_exception_init (ev);

	value = bonobo_config_ditem_decode_any (de->value, tc, ev);

	if (!value)
		bonobo_exception_set (ev, ex_Bonobo_PropertyBag_NotFound);

	return value;
}

static Bonobo_KeyList *
real_get_dirs (BonoboConfigDatabase *db,
	       const CORBA_char     *dir,
	       CORBA_Environment    *ev)
{
	BonoboConfigDItem *ditem = BONOBO_CONFIG_DITEM (db);
	Bonobo_KeyList *key_list;
	TSecHeader *dd, *c;
	int len;

	key_list = Bonobo_KeyList__alloc ();
	key_list->_length = 0;

	if (!(dd = lookup_dir (ditem->_priv->dir, dir, FALSE)))
		return key_list;

	for (len = 0, c = dd; c != NULL; c = c->link, len++)
		;

	if (!len)
		return key_list;

	key_list->_maximum = len;
	key_list->_buffer = CORBA_sequence_CORBA_string_allocbuf (len);
	CORBA_sequence_set_release (key_list, TRUE); 
	
	for (c = dd; c != NULL; c = c->link) {
		key_list->_buffer [key_list->_length] =
			CORBA_string_dup (c->section_name);
		key_list->_length++;
	}

	return key_list;
}

static Bonobo_KeyList *
real_get_keys (BonoboConfigDatabase *db,
	       const CORBA_char     *dir,
	       CORBA_Environment    *ev)
{
	BonoboConfigDItem *ditem = BONOBO_CONFIG_DITEM (db);
	Bonobo_KeyList *key_list;
	TSecHeader *dd;
	TKeys *c;
	int len;
	
	key_list = Bonobo_KeyList__alloc ();
	key_list->_length = 0;

	if (!(dd = lookup_dir (ditem->_priv->dir, dir, FALSE)))
		return key_list;

	for (len = 0, c = dd->keys; c != NULL; c = c->link, len++)
		;

	if (!len)
		return key_list;

	key_list->_maximum = len;
	key_list->_buffer = CORBA_sequence_CORBA_string_allocbuf (len);
	CORBA_sequence_set_release (key_list, TRUE); 
	
	for (c = dd->keys; c != NULL; c = c->link) {
		key_list->_buffer [key_list->_length] =
			CORBA_string_dup (c->key_name);
		key_list->_length++;
	}
	
	return key_list;
}

static CORBA_boolean
real_has_dir (BonoboConfigDatabase *db,
	      const CORBA_char     *dir,
	      CORBA_Environment    *ev)
{
	BonoboConfigDItem *ditem = BONOBO_CONFIG_DITEM (db);

	if (lookup_dir (ditem->_priv->dir, dir, FALSE))
		return TRUE;

	return FALSE;
}

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

	ditem->_priv->time_id = 0;

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

	if (ditem->_priv) {

		if (ditem->_priv->es)
			bonobo_object_unref (BONOBO_OBJECT (ditem->_priv->es));

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

	cd_class->get_type     = real_get_type;
	cd_class->get_value    = real_get_value;
	cd_class->get_dirs     = real_get_dirs;
	cd_class->get_keys     = real_get_keys;
	cd_class->has_dir      = real_has_dir;
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

	ditem->_priv->dir = load (ditem->filename);
		       
	ditem->_priv->es = bonobo_event_source_new ();

	bonobo_object_add_interface (BONOBO_OBJECT (ditem), 
				     BONOBO_OBJECT (ditem->_priv->es));

	db = CORBA_Object_duplicate (BONOBO_OBJREF (ditem), NULL);

	bonobo_url_register ("BONOBO_CONF:DITEM", real_name, NULL, db, &ev);

	return db;
}
