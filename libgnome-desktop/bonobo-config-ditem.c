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
#include <bonobo-conf/bonobo-config-utils.h>

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

struct _BonoboConfigDItemPrivate {
	guint                 time_id;
	gboolean              modified;

	Directory            *dir;

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

static gint
key_compare_func (gconstpointer a, gconstpointer b)
{
	DirEntry *de = (DirEntry *) a;

	return strcmp (de->name, b);
}

static DirEntry *
insert_key_maybe_localized (Section *section, gchar *key)
{
	gchar *a, *b, *locale = NULL;
	DirEntry *de = NULL;
	GSList *l = NULL;

	a = strchr (key, '[');
	b = strrchr (key, ']');
	if ((a != NULL) && (b != NULL)) {
		*a++ = '\0'; *b = '\0'; locale = a;
		l = g_slist_find_custom (section->root.subvalues, key, key_compare_func);
	}
	de = g_new0 (DirEntry, 1);

	if (l) {
		DirEntry *parent = (DirEntry *) l->data;

		de->name = g_strdup (locale);
		parent->subvalues = g_slist_append (parent->subvalues, de);
	} else {
		de->name = g_strdup (key);
		section->root.subvalues = g_slist_append (section->root.subvalues, de);
	}

	return de;
}

static Directory *
load (const char *file)
{
	FILE *f;
	int state;
	DirEntry *Key = 0;
	Directory *dir = 0;
	Section *section = 0;
	char CharBuffer [STRSIZE];
	char *next = "";		/* Not needed */
	int c;
	
	if ((f = fopen (file, "r"))==NULL)
		return NULL;

	dir = g_new0 (Directory, 1);
	dir->path = g_strdup (file);
	
	state = FirstBrace;
	while ((c = getc_unlocked (f)) != EOF){
		if (c == '\r')		/* Ignore Carriage Return */
			continue;
		
		switch (state){
			
		case OnSecHeader:
			if (c == ']' || overflow){
				*next = '\0';
				next = CharBuffer;
				section->name = g_strdup (CharBuffer);
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
				section = g_new0 (Section, 1);
				dir->sections = g_slist_append (dir->sections, section);
				state = OnSecHeader;
				next = CharBuffer;
				Key = 0;
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
				*next = '\0';

				Key = insert_key_maybe_localized (section, CharBuffer);
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
				Key->value = decode_string_and_dup (CharBuffer);
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
		Key->value = decode_string_and_dup (CharBuffer);
	}
	fclose (f);

	return dir;
}

static void
dump_subkeys (FILE *f, DirEntry *de)
{
	GSList *c;

	for (c = de->subvalues; c; c = c->next) {
		DirEntry *subentry = c->data;

		if (subentry->value) {
			gchar *t = escape_string_and_dup (subentry->value);

			fprintf (f, "%s[%s]=%s\n", de->name, subentry->name, t);
		}
	}
}

static void
dump_key (FILE *f, DirEntry *de)
{
	if (de->value) {
		gchar *t = escape_string_and_dup (de->value);

		fprintf (f, "%s=%s\n", de->name, de->value);
	}

	if (de->subvalues)
		dump_subkeys (f, de);
}

static void 
dump_section (FILE *f, Section *section)
{
	GSList *l;

	fprintf (f, "\n[%s]\n", section->name);
	for (l = section->root.subvalues; l; l = l->next) {
		DirEntry *de = l->data;

		dump_key (f, de);
	}
}

static gboolean
save (const char *file_name, Directory *dir)
{
	GSList *l;
	FILE *f;
	
	if ((f = fopen (file_name, "w"))==NULL)
		return FALSE;

	g_message (G_STRLOC ": |%s|", file_name);

	for (l = dir->sections; l; l = l->next) {
		Section *section = l->data;

		dump_section (f, section);
	}

	fclose (f);
	return TRUE;
}

static DirEntry *
dir_lookup_entry (Section    *section,
		  char       *name,
		  gboolean    create)
{
	GSList *l;
	DirEntry *de;

	for (l = section->root.subvalues; l; l = l->next) {
		de = (DirEntry *)l->data;

		if (!strcmp (de->name, name))
			return de;
	}

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

static Section *
dir_lookup_subdir (Directory   *dir,
		   char        *name,
		   gboolean     create)
{
	GSList *l;

	for (l = dir->sections; l; l = l->next) {
		Section *section = l->data;

		if (!strcmp (section->name, name))
			return section;
	}

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

static Section *
lookup_dir (Directory  *dir,
	    const char *path,
	    gboolean    create)
{
	const char *s, *e;
	Section *section;
	char *name;
	
	s = path;
	while (*s == '/') s++;
	
	if (*s == '\0')
		return NULL;

	if ((e = strchr (s, '/')))
		name = g_strndup (s, e - s);
	else
		name = g_strdup (s);

	if (e) {
		g_free (name);
		return NULL;
	}
	
	if ((section = dir_lookup_subdir (dir, name, create))) {
		g_free (name);
		return section;
		
	}

	return NULL;
}

static DirEntry *
lookup_dir_entry (BonoboConfigDItem *ditem,
		  const char        *key, 
		  gboolean           create)
{
	char       *dir_name;
	char       *leaf_name;
	DirEntry   *de;
	Section    *section;

	if ((dir_name = bonobo_config_dir_name (key))) {
		section = lookup_dir (ditem->_priv->dir, dir_name, create);
		
		g_free (dir_name);

	} else {
		section = lookup_dir (ditem->_priv->dir, key, create);

		if (section)
			return &section->root;
		else
			return NULL;
	}

	if (!section)
		return NULL;

	if (!(leaf_name = bonobo_config_leaf_name (key)))
		return &section->root;

	de = dir_lookup_entry (section, leaf_name, create);

	g_free (leaf_name);

	return de;
}

static CORBA_any *
real_get_value (BonoboConfigDatabase *db,
		const CORBA_char     *key, 
		CORBA_Environment    *ev)
{
	BonoboConfigDItem *ditem = BONOBO_CONFIG_DITEM (db);
	DirEntry          *de;
	CORBA_TypeCode     tc;
	CORBA_any         *default_value = NULL;
	CORBA_any         *value = NULL;
	char              *locale = NULL; 
				
	/* fixme: how to handle locale correctly ? */

	de = lookup_dir_entry (ditem, key, FALSE);
	if (!de) {
		bonobo_exception_set (ev, ex_Bonobo_PropertyBag_NotFound);
		return NULL;
	}

	g_message (G_STRLOC ": (%s) - `%s' - `%s' - %p", key, de->name, de->value,
		   de->subvalues);

	default_value = Bonobo_ConfigDatabase_getDefault (BONOBO_OBJREF (db), key, ev);
	if (BONOBO_EX (ev) || !default_value)
		tc = TC_CORBA_string;
	else
		tc = default_value->_type;

	CORBA_exception_init (ev);

	value = bonobo_config_ditem_decode_any (ditem, de, key, tc, ev);

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
	Section *section;
	GSList *l;
	int len;

	key_list = Bonobo_KeyList__alloc ();
	key_list->_length = 0;

	if (!(section = lookup_dir (ditem->_priv->dir, dir, FALSE)))
		return key_list;

	len = g_slist_length (section->root.subvalues);
	if (!len)
		return key_list;

	key_list->_maximum = len;
	key_list->_buffer = CORBA_sequence_CORBA_string_allocbuf (len);
	CORBA_sequence_set_release (key_list, TRUE); 
	
	for (l = section->root.subvalues; l; l = l->next) {
		DirEntry *de = l->data;

		key_list->_buffer [key_list->_length] = CORBA_string_dup (de->name);
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
	Section *section;
	DirEntry *de;
	GSList *l;
	int i, len;
	
	key_list = Bonobo_KeyList__alloc ();
	key_list->_length = 0;

	if (!(section = lookup_dir (ditem->_priv->dir, dir, FALSE)))
		return key_list;

	len = g_slist_length (section->root.subvalues);
	if (!len)
		return key_list;

	key_list->_maximum = key_list->_length = len;
	key_list->_buffer = CORBA_sequence_CORBA_string_allocbuf (len);
	CORBA_sequence_set_release (key_list, TRUE); 
	
	i = 0;
	for (l = section->root.subvalues; l != NULL; l = l->next) {
		de = (DirEntry *)l->data;
	       
		key_list->_buffer [i] = CORBA_string_dup (de->name);
		i++;
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
	DirEntry *de;

	de = lookup_dir_entry (ditem, key, TRUE);

	g_message (G_STRLOC ": |%s| - %p", key, de);

	if (!de)
		return;

	bonobo_config_ditem_encode_any (ditem, de, key, value, ev);

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
