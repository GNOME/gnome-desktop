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
#include <stdarg.h>
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
	UNKNOWN_ENCODING,
	UTF8_ENCODING,
	LEGACY_MIXED_ENCODING
};


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

struct _BonoboConfigDItemPrivate {
	guint                 time_id;
	gboolean              modified;

	Directory            *dir;

	BonoboEventSource    *es;
};

static char *
decode_ascii_string_and_dup (const char *s)
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
	char *p = g_malloc (strlen (s) + 1);
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

static gint
key_compare_func (gconstpointer a, gconstpointer b)
{
	DirEntry *de = (DirEntry *) a;

	return strcmp (de->name, b);
}

static DirEntry *
create_key_maybe_localized (Section *section, const char *key)
{
	DirEntry *de = NULL;

	de = g_new0 (DirEntry, 1);
	de->name = g_strdup (key);

	/* Note here, that some strings are localestrings even if they include
	 * no language, however, then they are in 'C' locale and thus should fit
	 * in standard 7bit ASCII which should be valid unicode, SO we just mark
	 * them as nonlocale string */
	if (strchr (key, '[') != NULL)
		de->localestring = TRUE;
	else
		de->localestring = FALSE;

	return de;
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

static void
insert_key_maybe_localized (Section *section, DirEntry *key, int encoding, const char *value)
{
	gchar *a, *b, *locale = NULL;
	DirEntry *de = NULL;
	GSList *l = NULL;

	a = strchr (key->name, '[');
	b = strrchr (key->name, ']');
	if ((a != NULL) && (b != NULL)) {
		*a++ = '\0'; *b = '\0'; locale = a;
		l = g_slist_find_custom (section->root.subvalues, key->name, key_compare_func);
	}

	/* if legacy mixed, then convert */
	if (key->localestring && encoding == LEGACY_MIXED_ENCODING) {
		const char *char_encoding = get_encoding_from_locale (locale);
		char *utf8_string;
		if (char_encoding == NULL) {
			g_free (key->name);
			g_free (key);
			return;
		}
		utf8_string = g_convert (value, -1, "UTF-8", char_encoding,
					NULL, NULL, NULL);
		if (utf8_string == NULL) {
			g_free (key->name);
			g_free (key);
			return;
		}
		key->value = decode_utf8_string_and_dup (utf8_string);
		g_free (utf8_string);
	/* if utf8, then validate */
	} else if (key->localestring && encoding == UTF8_ENCODING) {
		if ( ! g_utf8_validate (value, -1, NULL)) {
			/* invalid utf8, ignore this key */
			g_free (key->name);
			g_free (key);
			return;
		}
		key->value = decode_utf8_string_and_dup (value);
	} else {
		key->value = decode_ascii_string_and_dup (value);
	}

	if (locale != NULL && l == NULL) {
		/* Eeek: insert a dummy "C" locale value witht he current value, likely
		 * the C locale one will come later */
		DirEntry * dummy = create_key_maybe_localized (section, key->name);
		section->root.subvalues = g_slist_append (section->root.subvalues, dummy);
		l = g_slist_find_custom (section->root.subvalues, key->name, key_compare_func);
	}

	if (l != NULL) {
		DirEntry *parent = (DirEntry *) l->data;

		g_free (key->name);
		key->name = g_strdup (locale);
		parent->subvalues = g_slist_append (parent->subvalues, key);
	} else {
		/* let's see if this is a duplicate */
		l = g_slist_find_custom (section->root.subvalues, key->name, key_compare_func);
		if (l == NULL) {
			section->root.subvalues = g_slist_append (section->root.subvalues, key);
		} else {
			/* copy put this value into the old key */
			DirEntry *old_de = l->data;
			g_free (old_de->value);
			old_de->value = key->value;
			key->value = NULL;
			g_free (key->name);
			g_free (key);
		}
	}
}

static int
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
				return UTF8_ENCODING;
			} else if (strcmp ("Encoding=Legacy-Mixed", buf) == 0) {
				return LEGACY_MIXED_ENCODING;
			} else if (strncmp ("Encoding=", buf,
					    strlen ("Encoding=")) == 0) {
				/* According to the spec we're not supposed
				 * to read a file like this */
				return UNKNOWN_ENCODING;
			}
			if (strcmp ("[KDE Desktop Entry]", buf) == 0) {
				old_kde = TRUE;
				/* don't break yet, we still want to support
				 * Encoding even here */
			}
		}
	}

	if (old_kde)
		return LEGACY_MIXED_ENCODING;

	/* try to guess by location */
	if (strstr (file, "gnome/apps/") != NULL) {
		/* old gnome */
		return LEGACY_MIXED_ENCODING;
	}

	/* A dillema, new KDE files are in UTF-8 but have no Encoding
	 * info, at this time we really can't tell.  The best thing to
	 * do right now is to just assume UTF-8 I suppose */
	return UTF8_ENCODING;
}

static void
free_direntry (DirEntry *de)
{
	if (de != NULL) {
		g_free (de->name);
		de->name = NULL;
		g_free (de->value);
		de->value = NULL;

		g_slist_foreach (de->subvalues, (GFunc)free_direntry, NULL);

		g_free (de);
	}
}

static void
free_section (Section *section)
{
	if (section != NULL) {
		g_free (section->name);
		section->name = NULL;

		g_slist_foreach (section->root.subvalues, (GFunc)free_direntry, NULL);

		g_free (section);
	}
}

static void
free_directory (Directory *dir)
{
	if (dir != NULL) {
		g_free (dir->path);
		dir->path = NULL;

		g_slist_foreach (dir->sections, (GFunc)free_section, NULL);

		g_free (dir);
	}
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
	int utf8_chars_to_go = 0;
	int encoding;
	Section *main_section = NULL;
	gboolean got_encoding = FALSE;
	gboolean got_version = FALSE;
	
	if ((f = fopen (file, "r"))==NULL)
		return NULL;

	encoding = get_encoding (file, f);
	if (encoding == UNKNOWN_ENCODING) {
		fclose (f);
		/* spec says, don't read this file */
		return NULL;
	}

	/* Rewind since get_encoding goes through the file */
	rewind (f);

	dir = g_new0 (Directory, 1);
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
				} else if (strcmp (CharBuffer, "Desktop Entry") == 0) {
					main_section = section;
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

				Key = create_key_maybe_localized (section, CharBuffer);
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

				insert_key_maybe_localized (section, Key, encoding, CharBuffer);
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
		insert_key_maybe_localized (section, Key, encoding, CharBuffer);
	}
	fclose (f);

	if (main_section == NULL) {
		free_directory (dir);
		return NULL;
	}

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

	return dir;
}

static void
dump_subkeys (FILE *f, DirEntry *de)
{
	GSList *c;

	for (c = de->subvalues; c; c = c->next) {
		DirEntry *subentry = c->data;

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
dump_key (FILE *f, DirEntry *de)
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

	if (create) {
		de = g_new0 (DirEntry, 1);
		
		de->name = g_strdup (name);
		section->root.subvalues = g_slist_prepend (section->root.subvalues, de);

		return de;
	}

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

	if (create) {
		Section *section;

		section = g_new0 (Section, 1);
		section->name = g_strdup (name);

		dir->sections = g_slist_prepend (dir->sections, section);

		return section;
	}

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

static CORBA_TypeCode
real_get_type (BonoboConfigDatabase *db,
	       const CORBA_char     *key,
	       CORBA_Environment    *ev)
{
	BonoboConfigDItem *ditem = BONOBO_CONFIG_DITEM (db);
	gchar             *dir_name, *leaf_name;
	DirEntry          *de;
	CORBA_any         *default_value = NULL;
	CORBA_TypeCode     tc;

	de = lookup_dir_entry (ditem, key, FALSE);
	if (!de) {
		bonobo_exception_set (ev, ex_Bonobo_PropertyBag_NotFound);
		return CORBA_OBJECT_NIL;
	}

	default_value = Bonobo_ConfigDatabase_getDefault (BONOBO_OBJREF (db), key, ev);
	if (!BONOBO_EX (ev) && default_value != NULL)
		return default_value->_type;

	dir_name = bonobo_config_dir_name (key);
	if (!dir_name)
		return CORBA_OBJECT_NIL;

	leaf_name = bonobo_config_leaf_name (key);
	if (!leaf_name)
		return CORBA_OBJECT_NIL;

	CORBA_exception_init (ev);

	default_value = Bonobo_ConfigDatabase_getDefault (BONOBO_OBJREF (db), dir_name, ev);
	if (BONOBO_EX (ev) || default_value == NULL)
		return CORBA_OBJECT_NIL;

	tc = bonobo_config_ditem_get_subtype (ditem, de, leaf_name, default_value->_type, ev);

	return tc;
}

static CORBA_any *
real_get_value (BonoboConfigDatabase *db,
		const CORBA_char     *key, 
		CORBA_Environment    *ev)
{
	BonoboConfigDItem *ditem = BONOBO_CONFIG_DITEM (db);
	DirEntry          *de;
	CORBA_TypeCode     tc;
	CORBA_any         *value = NULL;
	char              *locale = NULL; 
				
	/* fixme: how to handle locale correctly ? */

	de = lookup_dir_entry (ditem, key, FALSE);
	if (!de) {
		bonobo_exception_set (ev, ex_Bonobo_PropertyBag_NotFound);
		return NULL;
	}

	tc = real_get_type (db, key, ev);
	if (!tc)
		return NULL;

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

	if (ditem->_priv != NULL) {

		free_directory (ditem->_priv->dir);
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
	Directory             *dir;

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

	dir = load (real_name);
	if (dir == NULL) {
		g_free (real_name);
		return CORBA_OBJECT_NIL;
	}

	if (!(ditem = g_object_new (BONOBO_CONFIG_DITEM_TYPE, NULL))) {
		free_directory (dir);
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
