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

#include "GNOME_Desktop.h"

static GObjectClass *parent_class = NULL;

#define CLASS(o) BONOBO_CONFIG_DITEM_CLASS (G_OBJECT_GET_CLASS (o))

#define PARENT_TYPE BONOBO_CONFIG_DATABASE_TYPE
#define FLUSH_INTERVAL 30 /* 30 seconds */

typedef struct _BonoboConfigDItemKey		BonoboConfigDItemKey;
typedef struct _BonoboConfigDItemSection	BonoboConfigDItemSection;
typedef struct _BonoboConfigDItemDirectory	BonoboConfigDItemDirectory;

struct _BonoboConfigDItemKey {
	gchar *name;
	CORBA_TypeCode tc;
	DynamicAny_DynAny dyn;
	gpointer data;
	GHashTable *subvalues;
	gboolean dirty;
};

struct _BonoboConfigDItemSection {
	gchar *name;
	BonoboConfigDItemKey root;
};

struct _BonoboConfigDItemDirectory {
	gchar *path;

	GSList *sections;
};

struct _BonoboConfigDItemPrivate {
	guint                        time_id;
	gboolean                     modified;

	BonoboConfigDItemDirectory  *dir;

	BonoboEventSource           *es;
};

#define STRSIZE 4096
#define overflow (next == &CharBuffer [STRSIZE-1])

enum {
	FirstBrace,
	OnSecHeader,
	IgnoreToEOL,
	IgnoreToEOLFirst,
	KeyDef,
	KeyDefOnKey,
	KeyValue,
	WideUnicodeChar
};

static gint
key_compare_func (gconstpointer a, gconstpointer b)
{
	BonoboConfigDItemKey *key = (BonoboConfigDItemKey *) a;

	return strcmp (key->name, b);
}

static gint
section_compare_func (gconstpointer a, gconstpointer b)
{
	BonoboConfigDItemSection *section = (BonoboConfigDItemSection *) a;

	return strcmp (section->name, b);
}

static void
free_key (BonoboConfigDItemKey *key)
{
	if (!key)
		return;

	g_free (key->name);
	g_hash_table_destroy (key->subvalues);
	CORBA_Object_release ((CORBA_Object) key->dyn, NULL);
	CORBA_free (key->data);
	g_free (key);
}

static void
setup_key (BonoboConfigDItemKey *key)
{
	DynamicAny_DynAnyFactory f;
	CORBA_TypeCode real_tc;
	CORBA_Environment ev;

	real_tc = key->tc;
	while (real_tc->kind == CORBA_tk_alias)
		real_tc = real_tc->subtypes [0];

	g_assert (key->dyn == CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);
	f = (DynamicAny_DynAnyFactory) CORBA_ORB_resolve_initial_references
		(bonobo_orb (), "DynAnyFactory", &ev);
	g_assert (!BONOBO_EX (&ev));
			
	key->dyn = DynamicAny_DynAnyFactory_create_dyn_any_from_type_code (f, key->tc, &ev);
	g_assert (!BONOBO_EX (&ev));

	key->subvalues = g_hash_table_new_full (g_str_hash, g_str_equal,
						(GDestroyNotify) g_free,
						(GDestroyNotify) free_key);

	switch (real_tc->kind) {
	case CORBA_tk_struct: {
		gulong length, i;
		CORBA_sequence_DynamicAny_NameDynAnyPair *members;

		length = real_tc->sub_parts;

		members = CORBA_sequence_DynamicAny_NameDynAnyPair__alloc ();
		members->_length = length;
		members->_buffer = CORBA_sequence_DynamicAny_NameDynAnyPair_allocbuf (length);

		key->data = members;

		for (i = 0; i < length; i++) {
			DynamicAny_NameDynAnyPair *this;
			BonoboConfigDItemKey *subkey;

			subkey = g_new0 (BonoboConfigDItemKey, 1);
			subkey->name = g_strdup (real_tc->subnames [i]);
			subkey->tc = real_tc->subtypes [i];

			setup_key (subkey);

			this = &members->_buffer [i];
			this->id = CORBA_string_dup (real_tc->subnames [i]);
			this->value = subkey->dyn;

			g_hash_table_insert (key->subvalues, subkey->name, subkey);
		}

		break;
	}

	default:
		break;
	}
}

static void
setup_section (BonoboConfigDItemSection *section, const char *name, CORBA_TypeCode tc)
{
	CORBA_TypeCode real_tc;

	section->root.tc = tc;

	setup_key (&section->root);
}

static void
set_sequence_dynany (gchar *name, BonoboConfigDItemKey *key, DynamicAny_DynAnySeq *members)
{
	members->_buffer [members->_length] = (CORBA_Object) key->dyn;
	members->_length++;
}

static void
sync_sequence_members (BonoboConfigDItemKey *key)
{
	CORBA_TypeCode real_tc;
	CORBA_Environment ev;
	CORBA_sequence_DynamicAny_DynAny *members;
	gulong length;

	real_tc = key->tc;
	while (real_tc->kind == CORBA_tk_alias)
		real_tc = real_tc->subtypes [0];

	g_assert (real_tc->kind == CORBA_tk_sequence);

	if (key->data)
		CORBA_free (key->data);

	length = g_hash_table_size (key->subvalues);

	members = CORBA_sequence_DynamicAny_DynAny__alloc ();
	members->_buffer = CORBA_sequence_DynamicAny_DynAny_allocbuf (length);

	g_hash_table_foreach (key->subvalues, (GHFunc) set_sequence_dynany, members);

	key->data = members;

	CORBA_exception_init (&ev);
	DynamicAny_DynSequence_set_length ((DynamicAny_DynSequence) key->dyn, length, &ev);
	g_assert (!BONOBO_EX (&ev));
}

BonoboConfigDItemKey *
bonobo_config_ditem_lookup_key (BonoboConfigDItemSection *section, const char *name, gboolean create)
{
	gchar *a, *b, *locale = NULL;
	BonoboConfigDItemKey *key, *subkey;
	GSList *l;

	a = strchr (name, '[');
	b = strrchr (name, ']');
	if ((a != NULL) && (b != NULL)) {
		*a++ = '\0'; *b = '\0'; locale = a;
	}

	key = g_hash_table_lookup (section->root.subvalues, name);

	if (!key || !locale)
		return key;

	if (!CORBA_TypeCode_equal (key->tc, TC_GNOME_LocalizedStringList, NULL))
		return NULL;

	subkey = g_hash_table_lookup (key->subvalues, locale);
	if (subkey)
		return subkey;

	if (!create)
		return NULL;

	subkey = g_new0 (BonoboConfigDItemKey, 1);
	subkey->name = g_strdup (locale);
	subkey->tc = TC_GNOME_LocalizedString;

	setup_key (subkey);

	g_hash_table_insert (key->subvalues, subkey->name, subkey);

	sync_sequence_members (key);

	return subkey;
}

static BonoboConfigDItemSection *
lookup_dir (BonoboConfigDItemDirectory *dir, const char *path)
{
	const char *s, *e;
	BonoboConfigDItemSection *section;
	GSList *l;
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

	l = g_slist_find_custom (dir->sections, name, section_compare_func);
	if (l) {
		g_free (name);
		return l->data;
	}

	g_free (name);
	return NULL;
}

BonoboConfigDItemSection *
bonobo_config_ditem_lookup_section (BonoboConfigDItemDirectory *dir, const char *name)
{
	BonoboConfigDItemSection *section;
	char *dir_name;

	dir_name = bonobo_config_dir_name (name);
	section = lookup_dir (dir, dir_name ? dir_name : name);
	g_free (dir_name);

	return section;
}

BonoboConfigDItemKey *
bonobo_config_ditem_lookup (BonoboConfigDItemDirectory *dir, const char *name, gboolean create)
{
	BonoboConfigDItemSection *section;
	BonoboConfigDItemKey *key;
	char *dir_name, *leaf_name;

	if ((dir_name = bonobo_config_dir_name (name))) {
		section = lookup_dir (dir, dir_name);
		g_free (dir_name);
	} else {
		section = lookup_dir (dir, name);

		if (section)
			return &section->root;
		else
			return NULL;
	}

	if (!section)
		return NULL;

	if (!(leaf_name = bonobo_config_leaf_name (name)))
		return &section->root;

	key = bonobo_config_ditem_lookup_key (section, leaf_name, create);

	g_free (leaf_name);

	return key;
}

static char *
decode_ascii_string_and_dup (const char *s)
{
	char *p = g_malloc (strlen (s) + 10);
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
	*p++ = '\0';
	return q;
}

static char *
escape_ascii_string_and_dup (const char *s)
{
	char *return_value, *p;
	const char *q;
	int len = 0;

	if(!s)
		return g_strdup("");
	
	q = s;
	while (*q){
		len++;
		if (*q == '\n' || *q == '\\' || *q == '\r' || *q == '\0')
			len++;
		q++;
	}
	return_value = p = (char *) g_malloc (len + 1);
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
	*p++ = '\0';
	return return_value;
}

static char *
decode_utf8_string_and_dup (const char *s)
{
	char *p = g_malloc (strlen (s) + 2);
	char *q = p;
	int skip = 0;

	do {
		if (skip > 0) {
			skip--;
			*p++ = *s;
		} else if (*s == '\\'){
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
		} else if (g_utf8_skip [(guchar)*s] > 1) {
			skip = g_utf8_skip [(guchar)*s] - 1;
			*p++ = *s;
		} else {
			*p++ = *s;
		}
	} while (*s++);
	*p++ = '\0';
	return q;
}

static char *
escape_utf8_string_and_dup (const char *s)
{
	char *return_value, *p;
	const char *q;
	int len = 0;
	int skip = 0;

	if(!s)
		return g_strdup("");
	
	q = s;
	while (*q){
		len++;
		if (*q == '\n' || *q == '\\' || *q == '\r' || *q == '\0')
			len++;
		q = g_utf8_next_char (q);
	}
	return_value = p = (char *) g_malloc (len + 1);
	do {
		if (skip > 0) {
			*p++ = *s;
			skip --;
		} else {
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
				if (g_utf8_skip [(guchar)*s] > 1)
					skip = g_utf8_skip [(guchar)*s] - 1;
				*p++ = *s;
			}
		}
	} while (*s++);
	*p++ = '\0';
	return return_value;
}

static gboolean
check_locale (const char *locale)
{
	GIConv cd = g_iconv_open ("UTF-8", locale);
	if ((GIConv)-1 == cd)
		return FALSE;
	g_iconv_close (cd);
	return TRUE;
}

static void
insert_locales (GHashTable *encodings, char *enc, ...)
{
	va_list args;
	char *s;

	va_start (args, enc);
	for (;;) {
		s = va_arg (args, char *);
		if (s == NULL)
			break;
		g_hash_table_insert (encodings, s, enc);
	}
	va_end (args);
}

/* make a standard conversion table from the desktop standard spec */
static GHashTable *
init_encodings (void)
{
	GHashTable *encodings = g_hash_table_new (g_str_hash, g_str_equal);

	insert_locales (encodings, "ARMSCII-8", "by", NULL);
	insert_locales (encodings, "BIG5", "zh_TW", NULL);
	insert_locales (encodings, "CP1251", "be", "bg", NULL);
	if (check_locale ("EUC-CN")) {
		insert_locales (encodings, "EUC-CN", "zh_CN", NULL);
	} else {
		insert_locales (encodings, "GB2312", "zh_CN", NULL);
	}
	insert_locales (encodings, "EUC-JP", "ja", NULL);
	insert_locales (encodings, "EUC-KR", "ko", NULL);
	/*insert_locales (encodings, "GEORGIAN-ACADEMY", NULL);*/
	insert_locales (encodings, "GEORGIAN-PS", "ka", NULL);
	insert_locales (encodings, "ISO-8859-1", "br", "ca", "da", "de", "en", "es", "eu", "fi", "fr", "gl", "it", "nl", "wa", "no", "pt", "pt", "sv", NULL);
	insert_locales (encodings, "ISO-8859-2", "cs", "hr", "hu", "pl", "ro", "sk", "sl", "sq", "sr", NULL);
	insert_locales (encodings, "ISO-8859-3", "eo", NULL);
	insert_locales (encodings, "ISO-8859-5", "mk", "sp", NULL);
	insert_locales (encodings, "ISO-8859-7", "el", NULL);
	insert_locales (encodings, "ISO-8859-9", "tr", NULL);
	insert_locales (encodings, "ISO-8859-13", "lt", "lv", "mi", NULL);
	insert_locales (encodings, "ISO-8859-14", "ga", "cy", NULL);
	insert_locales (encodings, "ISO-8859-15", "et", NULL);
	insert_locales (encodings, "KOI8-R", "ru", NULL);
	insert_locales (encodings, "KOI8-U", "uk", NULL);
	if (check_locale ("TCVN-5712")) {
		insert_locales (encodings, "TCVN-5712", "vi", NULL);
	} else {
		insert_locales (encodings, "TCVN", "vi", NULL);
	}
	insert_locales (encodings, "TIS-620", "th", NULL);
	/*insert_locales (encodings, "VISCII", NULL);*/

	return encodings;
}

static const char *
get_encoding_from_locale (const char *locale)
{
	char lang[3];
	const char *encoding;
	static GHashTable *encodings = NULL;

	if (locale == NULL)
		return NULL;

	/* if locale includes encoding, use it */
	encoding = strchr (locale, '.');
	if (encoding != NULL) {
		return encoding+1;
	}

	if (encodings == NULL)
		encodings = init_encodings ();

	/* first try the entire locale (at this point ll_CC) */
	encoding = g_hash_table_lookup (encodings, locale);
	if (encoding != NULL)
		return encoding;

	/* Try just the language */
	strncpy (lang, locale, 2);
	lang[2] = '\0';
	return g_hash_table_lookup (encodings, lang);
}

static GNOME_Desktop_Encoding
get_encoding (const char *file, FILE *f)
{
	int c;
	gboolean old_kde = FALSE;
	
	while ((c = getc_unlocked (f)) != EOF) {
		/* find starts of lines */
		if (c == '\n' || c == '\r') {
			char buf[256];
			int i = 0;
			while ((c = getc_unlocked (f)) != EOF) {
				if (c == '\n' ||
				    c == '\r' ||
				    i >= sizeof(buf) - 1)
					break;
				buf[i++] = c;
			}
			buf[i++] = '\0';
			if (strcmp ("Encoding=UTF-8", buf) == 0) {
				return GNOME_Desktop_ENCODING_UTF8;
			} else if (strcmp ("Encoding=Legacy-Mixed", buf) == 0) {
				return GNOME_Desktop_ENCODING_LEGACY_MIXED;
			} else if (strncmp ("Encoding=", buf,
					    strlen ("Encoding=")) == 0) {
				/* According to the spec we're not supposed
				 * to read a file like this */
				return GNOME_Desktop_ENCODING_UNKNOWN;
			}
			if (strcmp ("[KDE Desktop Entry]", buf) == 0) {
				old_kde = TRUE;
				/* don't break yet, we still want to support
				 * Encoding even here */
			}
		}
	}

	if (old_kde)
		return GNOME_Desktop_ENCODING_LEGACY_MIXED;

	/* try to guess by location */
	if (strstr (file, "gnome/apps/") != NULL) {
		/* old gnome */
		return GNOME_Desktop_ENCODING_LEGACY_MIXED;
	}

	/* A dillema, new KDE files are in UTF-8 but have no Encoding
	 * info, at this time we really can't tell.  The best thing to
	 * do right now is to just assume UTF-8 I suppose */
	return GNOME_Desktop_ENCODING_UTF8;
}

static CORBA_char *
encode_string (const gchar *value, GNOME_Desktop_Encoding encoding, const gchar *locale)
{
	gchar *newvalue = NULL;
	CORBA_char *retval;

	/* if legacy mixed, then convert */
	if (locale && encoding == GNOME_Desktop_ENCODING_LEGACY_MIXED) {
		const char *char_encoding = get_encoding_from_locale (locale);
		char *utf8_string;
		if (char_encoding == NULL)
			return NULL;
		utf8_string = g_convert (value, -1, "UTF-8", char_encoding,
					NULL, NULL, NULL);
		if (utf8_string == NULL)
			return NULL;
		newvalue = decode_utf8_string_and_dup (utf8_string);
		g_free (utf8_string);
	/* if utf8, then validate */
	} else if (locale && encoding == GNOME_Desktop_ENCODING_UTF8) {
		if (!g_utf8_validate (value, -1, NULL))
			/* invalid utf8, ignore this key */
			return NULL;
		newvalue = decode_utf8_string_and_dup (value);
	} else {
		newvalue = decode_ascii_string_and_dup (value);
	}

	retval = CORBA_string_dup (newvalue ? newvalue : "");
	g_free (newvalue);

	return retval;
}

void
bonobo_config_ditem_write_key (BonoboConfigDItemKey *key, GNOME_Desktop_Encoding encoding, const char *value)
{
	CORBA_TypeCode real_tc;

	real_tc = key->tc;
	while (real_tc->kind == CORBA_tk_alias)
		real_tc = real_tc->subtypes [0];

	switch (real_tc->kind) {
	case CORBA_tk_string: {
		CORBA_char *newvalue = NULL;
		CORBA_Environment ev;

		newvalue = encode_string (value, encoding, NULL);

		CORBA_exception_init (&ev);
		DynamicAny_DynAny_insert_string (key->dyn, newvalue, &ev);
		g_assert (!BONOBO_EX (&ev));

		key->dirty = TRUE;

		break;
	}

	case CORBA_tk_struct: {
		GNOME_LocalizedString localized;
		CORBA_Environment ev;
		CORBA_any *any;

		g_assert (CORBA_TypeCode_equal (real_tc, TC_GNOME_LocalizedString, NULL));

		localized.locale = CORBA_string_dup (key->name);
		localized.text = encode_string (value, encoding, key->name);

		any = bonobo_arg_new (TC_GNOME_LocalizedString);
		BONOBO_ARG_SET_GENERAL (any, localized, TC_GNOME_LocalizedString,
					GNOME_LocalizedString, &ev);

		CORBA_exception_init (&ev);
		DynamicAny_DynAny_from_any (key->dyn, any, &ev);
		g_assert (!BONOBO_EX (&ev));

		key->dirty = TRUE;

		break;
	}

	default:
		g_assert_not_reached ();
		break;
	}
}

static void
sync_key (gchar *name, BonoboConfigDItemKey *key, gboolean *dirty)
{
	if (bonobo_config_ditem_sync_key (key))
		*dirty = TRUE;
}

static void
key_modified (BonoboConfigDItemKey *key)
{
	CORBA_TypeCode real_tc;
	CORBA_Environment ev;

	real_tc = key->tc;
	while (real_tc->kind == CORBA_tk_alias)
		real_tc = real_tc->subtypes [0];

	key->dirty = FALSE;

	if (key->subvalues)
		g_hash_table_destroy (key->subvalues);

	key->subvalues = g_hash_table_new_full (g_str_hash, g_str_equal,
						(GDestroyNotify) g_free,
						(GDestroyNotify) free_key);

	CORBA_free (key->data);
	key->data = NULL;

	switch (real_tc->kind) {
	case CORBA_tk_struct: {
		DynamicAny_NameDynAnyPairSeq *members;
		gulong i;

		CORBA_exception_init (&ev);
		members = DynamicAny_DynStruct_get_members_as_dyn_any
			((DynamicAny_DynStruct) key->dyn, &ev);
		g_assert (!BONOBO_EX (&ev));

		key->data = members;

		for (i = 0; i < members->_length; i++) {
			DynamicAny_NameDynAnyPair *this = &members->_buffer [i];
			BonoboConfigDItemKey *subkey;

			subkey = g_new0 (BonoboConfigDItemKey, 1);
			subkey->name = g_strdup (real_tc->subnames [i]);
			subkey->tc = real_tc->subtypes [i];
			subkey->dyn = this->value;

			key_modified (subkey);

			g_hash_table_insert (key->subvalues, subkey->name, subkey);
		}

		break;
	}

	default:
		break;
	}
}

gboolean
bonobo_config_ditem_sync_key (BonoboConfigDItemKey *key)
{
	CORBA_TypeCode real_tc;
	CORBA_Environment ev;

	/* If the key has been modified, propagate the changes to its subkeys. */
	if (key->dirty) {
		key_modified (key);
		return TRUE;
	}

	/* Check whether one of our subkeys is dirty. */
	g_hash_table_foreach (key->subvalues, (GHFunc) sync_key, &key->dirty);

	if (!key->dirty)
		return FALSE;

	/* Set the key's value from its subkeys. */
	real_tc = key->tc;
	while (real_tc->kind == CORBA_tk_alias)
		real_tc = real_tc->subtypes [0];

	switch (real_tc->kind) {
	case CORBA_tk_struct:
		CORBA_exception_init (&ev);
		DynamicAny_DynStruct_set_members_as_dyn_any ((DynamicAny_DynStruct) key->dyn,
							     key->data, &ev);
		g_assert (!BONOBO_EX (&ev));

		break;

	case CORBA_tk_sequence:
		CORBA_exception_init (&ev);
		DynamicAny_DynSequence_set_elements_as_dyn_any ((DynamicAny_DynSequence) key->dyn,
								key->data, &ev);
		g_assert (!BONOBO_EX (&ev));

		break;

	default:
		break;
	}

	key->dirty = FALSE;

	return TRUE;
}

static void
free_section (BonoboConfigDItemSection *section)
{
	if (!section)
		return;

	g_free (section->name);
	g_hash_table_destroy (section->root.subvalues);
	g_free (section);
}

void
bonobo_config_ditem_free_directory (BonoboConfigDItemDirectory *dir)
{
	if (!dir)
		return;

	g_free (dir->path);
	g_slist_foreach (dir->sections, (GFunc) free_section, NULL);
	g_free (dir);
}

BonoboConfigDItemDirectory *
bonobo_config_ditem_load (const gchar *file)
{
	FILE *f;
	int state;
	BonoboConfigDItemKey *Key = 0;
	BonoboConfigDItemDirectory *dir = 0;
	BonoboConfigDItemSection *section = 0;
	char CharBuffer [STRSIZE];
	char *next = "";		/* Not needed */
	int c;
	int utf8_chars_to_go = 0;
	GNOME_Desktop_Encoding encoding;
	BonoboConfigDItemSection *main_section = NULL;
	gboolean got_encoding = FALSE;
	gboolean got_version = FALSE;
	
	if ((f = fopen (file, "r"))==NULL)
		return NULL;

	encoding = get_encoding (file, f);
	if (encoding == GNOME_Desktop_ENCODING_UNKNOWN) {
		fclose (f);
		/* spec says, don't read this file */
		return NULL;
	}

	/* Rewind since get_encoding goes through the file */
	rewind (f);

	dir = g_new0 (BonoboConfigDItemDirectory, 1);
	dir->path = g_strdup (file);
	
	state = FirstBrace;
	while ((c = getc_unlocked (f)) != EOF){
		if ( ! WideUnicodeChar &&
		    c == '\r')		/* Ignore Carriage Return,
					   unless in a wide unicode char */
			continue;
		
		switch (state){
			
		case OnSecHeader:
			if (c == ']' || overflow){
				*next = '\0';
				next = CharBuffer;
				if (strcmp (CharBuffer, "KDE Desktop Entry") == 0) {
					strcpy (CharBuffer, "Desktop Entry");
					main_section = section;
					setup_section (section, "Desktop Entry", TC_GNOME_Desktop_Entry);
				} else if (strcmp (CharBuffer, "Desktop Entry") == 0) {
					main_section = section;
					setup_section (section, "Desktop Entry", TC_GNOME_Desktop_Entry);
				}
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
				section = g_new0 (BonoboConfigDItemSection, 1);
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

				Key = bonobo_config_ditem_lookup_key (section, CharBuffer, TRUE);
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
				/* we always store everything in UTF-8 */
				if (section == main_section &&
				    strcmp (Key->name, "Encoding") == 0) {
					strcpy (CharBuffer, "UTF-8");
					got_encoding = TRUE;
				} else if (section == main_section &&
					   strcmp (Key->name, "Version") == 0) {
					got_version = TRUE;
				}

				bonobo_config_ditem_write_key (Key, encoding, CharBuffer);
				state = c == '\n' ? KeyDef : IgnoreToEOL;
				next = CharBuffer;
			} else {
				if (g_utf8_skip[c] > 1) {
					state = WideUnicodeChar;
					utf8_chars_to_go = g_utf8_skip[c] - 1;
				}
				*next++ = c;
			}
			break;

		case WideUnicodeChar:
			utf8_chars_to_go--;
			if (utf8_chars_to_go <= 0) {
				state = KeyValue;
			}
			*next++ = c;
			break;
	    
		} /* switch */
	
	} /* while ((c = getc_unlocked (f)) != EOF) */
	if (c == EOF && state == KeyValue){
		*next = '\0';
		/* we always store everything in UTF-8 */
		if (section == main_section &&
		    strcmp (Key->name, "Encoding") == 0) {
			strcpy (CharBuffer, "UTF-8");
			got_encoding = TRUE;
		} else if (section == main_section &&
			   strcmp (Key->name, "Version") == 0) {
			got_version = TRUE;
		}
		bonobo_config_ditem_write_key (Key, encoding, CharBuffer);
	}
	fclose (f);

	if (main_section == NULL) {
		bonobo_config_ditem_free_directory (dir);
		return NULL;
	}

	main_section->root.tc = TC_GNOME_Desktop_Entry;

#if 0
	if ( ! got_encoding) {
		/* We store everything in UTF-8 so reflect that fact */
		Key = create_key_maybe_localized (main_section, "Encoding");
		insert_key_maybe_localized (main_section, Key, encoding, "UTF-8");
	}
	if ( ! got_version) {
		/* this is the version that we follow, so write it down */
		Key = create_key_maybe_localized (main_section, "Version");
		insert_key_maybe_localized (main_section, Key, encoding, "0.9.2");
	}
#endif

	return dir;
}

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

	/* Don't remove this g_print, it's to "hard-require" dmalloc so
	 * that the linker can't optimize it away. */
	g_print ("You can ignore this number: %d\n", dmalloc_errno);

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
