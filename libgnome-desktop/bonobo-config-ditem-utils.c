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
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo-config/bonobo-config-utils.h>
#include <libgnome/Gnome.h>

#include "bonobo-config-ditem-utils.h"

static gint
key_compare_func (gconstpointer a, gconstpointer b)
{
	DirEntry *de = (DirEntry *) a;

	return strcmp (de->name, b);
}

static GSList *
read_string_as_vector (const gchar *value)
{
	GSList *retval = NULL;
	gchar buffer [BUFSIZ+3], *pos, sep1 = ',', sep2 = ';';
	const gchar *c;

	if (!value)
		return NULL;

	pos = buffer; c = value;
	while (*c != '\0') {
		if (pos >= &buffer [BUFSIZ])
			// Ooooops
			break;

		if (*c == '\\') {
			c++;
			if (*c == '\0')
				break;
			else
				*pos++ = *c++;
			continue;
		}

		if ((*c == sep1) || (*c == sep2)) {
			*pos++ = '\0';
			retval = g_slist_prepend (retval, g_strdup (buffer));
			pos = buffer; c++;
			continue;
		}

		*pos++ = *c;
		c++;
	}

	*pos++ = '\0';
	if (buffer [0] != '\0')
		retval = g_slist_prepend (retval, g_strdup (buffer));

	return g_slist_reverse (retval);
}

CORBA_any *
bonobo_config_ditem_decode_any (BonoboConfigDItem *ditem, DirEntry *de, const gchar *path,
				CORBA_TypeCode type, CORBA_Environment *ev)
{
	CORBA_any *any = NULL;

	g_return_val_if_fail (ditem != NULL, NULL);
	g_return_val_if_fail (BONOBO_IS_CONFIG_DITEM (ditem), NULL);
	g_return_val_if_fail (de != NULL, NULL);
	g_return_val_if_fail (ev != NULL, NULL);

#if 0
	g_message (G_STRLOC ": |%s| - %d - %s (%s)", de->value,
		   type->kind, type->name, type->repo_id);
#endif

	switch (type->kind) {

#define DECODE_BASIC(k,t,v) \
case CORBA_tk_##k##: \
	any = bonobo_arg_new (TC_CORBA_##t##); \
	BONOBO_ARG_SET_GENERAL (any, ##v##, TC_CORBA_##k##, CORBA_##t##, ev); \
	break;

		DECODE_BASIC (short,     short,              atoi  (de->value));
		DECODE_BASIC (long,      long,               atol  (de->value));
		DECODE_BASIC (ushort,    unsigned_short,     atoi  (de->value));
		DECODE_BASIC (ulong,     unsigned_long,      atol  (de->value));
		DECODE_BASIC (longlong,  long_long,          atoll (de->value));
		DECODE_BASIC (float,     float,              atof  (de->value));
		DECODE_BASIC (double,    double,             atof  (de->value));
		DECODE_BASIC (ulonglong, unsigned_long_long, atoll (de->value));
		DECODE_BASIC (char,      char,               de->value [0]);
		DECODE_BASIC (octet,     octet,              atoi  (de->value));

#undef DECODE_BASIC

	case CORBA_tk_boolean:
		if (!strcmp (de->value, "0") || !strcasecmp (de->value, "false")) {
			any = bonobo_arg_new (TC_CORBA_boolean);
			BONOBO_ARG_SET_BOOLEAN (any, FALSE);
		} else if (!strcmp (de->value, "1") || !strcasecmp (de->value, "true")) {
			any = bonobo_arg_new (TC_CORBA_boolean);
			BONOBO_ARG_SET_BOOLEAN (any, TRUE);
		}
		break;

	case CORBA_tk_string:
		any = bonobo_arg_new (TC_CORBA_string);
		BONOBO_ARG_SET_STRING (any, de->value);
		break;

	case CORBA_tk_alias:
		any = bonobo_config_ditem_decode_any (ditem, de, path, type->subtypes [0], ev);
		CORBA_Object_release ((CORBA_Object) any->_type, NULL);
		any->_type = (CORBA_TypeCode) CORBA_Object_duplicate ((CORBA_Object )type, NULL);
		break;

	case CORBA_tk_sequence: {
		DynamicAny_DynSequence dynseq;
		DynamicAny_DynAny_AnySeq *dynany_anyseq;
		gulong length = 0, i;
		GSList *l, *array = NULL;

		if (de->subvalues) {
			length = g_slist_length (de->subvalues);
			if (de->value)
				length++;
		} else if (de->value) {
			array = read_string_as_vector (de->value);
			length = g_slist_length (array);
		}

		dynseq = CORBA_ORB_create_dyn_sequence (bonobo_orb (), type, ev);
		DynamicAny_DynSequence_set_length (dynseq, length, ev);

		dynany_anyseq = CORBA_sequence_DynamicAny_DynAny_AnySeq__alloc ();
		dynany_anyseq->_length = length;
		dynany_anyseq->_buffer = CORBA_sequence_DynamicAny_DynAny_AnySeq_allocbuf (length);

		if (de->subvalues) {
			l = de->subvalues;

			if (de->value) {
				DirEntry subentry;

				memset (&subentry, 0, sizeof (DirEntry));
				subentry.value = de->value;

				dynany_anyseq->_buffer [0] = bonobo_config_ditem_decode_any
					(ditem, &subentry, path, type->subtypes [0], ev);
				i = 1;
			} else
				i = 0;

			while (l) {
				DirEntry *subentry = (DirEntry *)l->data;

				dynany_anyseq->_buffer [i] = bonobo_config_ditem_decode_any
					(ditem, subentry, path, type->subtypes [0], ev);

				l = l->next;
				i++;
			}
		} else {
			l = array;

			i = 0;
			while (l) {
				DirEntry subentry;

				memset (&subentry, 0, sizeof (DirEntry));
				subentry.value = l->data;

				dynany_anyseq->_buffer [i] = bonobo_config_ditem_decode_any
					(ditem, &subentry, path, type->subtypes [0], ev);

				l = l->next;
				i++;
			}
		}

		g_slist_foreach (array, (GFunc) g_free, NULL);
		g_slist_free (array);

		DynamicAny_DynSequence_set_elements (dynseq, dynany_anyseq, ev);

		any = DynamicAny_DynAny_to_any (dynseq, ev);

		break;

	}

	case CORBA_tk_struct: {
		DynamicAny_DynStruct dynstruct;
		CORBA_NameValuePairSeq *members;
		gulong length, i, j;
		GSList *l;

		if (CORBA_TypeCode_equal (type, TC_GNOME_LocalizedString, NULL)) {
			GNOME_LocalizedString localized;

			localized.locale = CORBA_string_dup (de->name);
			localized.text = CORBA_string_dup (de->value);

			any = bonobo_arg_new (TC_GNOME_LocalizedString);
			BONOBO_ARG_SET_GENERAL (any, localized, TC_GNOME_LocalizedString,
						GNOME_LocalizedString, ev);
			break;
		}

		dynstruct = CORBA_ORB_create_dyn_struct (bonobo_orb (), type, ev);

		length = CORBA_TypeCode_member_count (type, ev);

		members = CORBA_sequence_CORBA_NameValuePair__alloc ();
		members->_length = length;
		members->_buffer = CORBA_sequence_CORBA_NameValuePair_allocbuf (length);

		for (i = 0; i < length; i++) {
			CORBA_Identifier member_name;
			CORBA_TypeCode member_type;
			CORBA_any *subvalue = NULL;
			DirEntry *subentry = NULL;

			member_name = CORBA_TypeCode_member_name (type, i, ev);
			member_type = CORBA_TypeCode_member_type (type, i, ev);

			if (CORBA_TypeCode_equal (member_type, TC_GNOME_ExtraAttributes, NULL)) {
				GNOME_ExtraAttributes attr_list;
				GSList *extra_attrs = NULL;
				gulong extra_length;

				l = de->subvalues;
				while (l) {
					Bonobo_Pair *pair;
					gchar *default_key;
					CORBA_TypeCode subtk;
					CORBA_any *any;

					subentry = l->data;

					for (j = 0; j < length; j++) {
						CORBA_Identifier cur_name;

						cur_name = CORBA_TypeCode_member_name (type, j, ev);
						if (!strcmp (subentry->name, cur_name))
							goto next_extra_attr_loop;
					}

					default_key = g_strdup_printf ("%s/%s", path, subentry->name);

					subtk = Bonobo_ConfigDatabase_getType (BONOBO_OBJREF (ditem),
									       default_key, ev);

					any = bonobo_config_ditem_decode_any
						(ditem, subentry, path, subtk, ev);

					pair = Bonobo_Pair__alloc ();
					pair->name = CORBA_string_dup (subentry->name);
					if (any)
						CORBA_any__copy (&pair->value, any);
					else
						pair->value._type = TC_null;

					extra_attrs = g_slist_prepend (extra_attrs, pair);

					g_free (default_key);

				next_extra_attr_loop:
					l = l->next;
				}

				extra_length = g_slist_length (extra_attrs);
				attr_list._buffer = CORBA_sequence_Bonobo_Pair_allocbuf (extra_length);
				attr_list._length = attr_list._maximum = extra_length;
				attr_list._release = TRUE;

				j = 0;
				l = extra_attrs;
				while (l) {
					Bonobo_Pair *pair = l->data;

					attr_list._buffer [j] = *pair;

					l = l->next;
					j++;
				}

				g_slist_free (extra_attrs);

				members->_buffer [i].id = CORBA_string_dup (member_name);

				subvalue = bonobo_arg_new (TC_GNOME_ExtraAttributes);
				BONOBO_ARG_SET_GENERAL (subvalue, attr_list, TC_GNOME_ExtraAttributes,
							GNOME_ExtraAttributes, ev);
				CORBA_any__copy (&members->_buffer [i].value, subvalue);

				continue;
			}

			l = g_slist_find_custom (de->subvalues, member_name, key_compare_func);
			if (l) {
				subentry = l->data;
				subvalue = bonobo_config_ditem_decode_any
					(ditem, subentry, path, member_type, ev);
			}

			if (!subvalue)
				subvalue = bonobo_arg_new (member_type);

			members->_buffer [i].id = CORBA_string_dup (member_name);
			CORBA_any__copy (&members->_buffer [i].value, subvalue);
		}

		DynamicAny_DynStruct_set_members (dynstruct, members, ev);

		CORBA_free (members);

		members = DynamicAny_DynStruct_get_members (dynstruct, ev);

		any = DynamicAny_DynAny_to_any (dynstruct, ev);

		break;
	}

	case CORBA_tk_enum: {
		DynamicAny_DynEnum dynenum;

		dynenum = CORBA_ORB_create_dyn_enum (bonobo_orb (), type, ev);

		if (CORBA_TypeCode_equal (type, TC_GNOME_DesktopEntryType, NULL)) {
			gchar *value, *up;

			up = g_strup (g_strdup (de->value ? de->value : "UNKNOWN"));
			value = g_strdup_printf ("DESKTOP_ENTRY_TYPE_%s", up);
			DynamicAny_DynEnum_set_as_string (dynenum, value, ev);
			g_free (value); g_free (up);
		} else {
			DynamicAny_DynEnum_set_as_string (dynenum, de->value, ev);
		}

		any = DynamicAny_DynAny_to_any (dynenum, ev);

		break;
	}

	default:
		g_message (G_STRLOC ": |%s| - %d - %s (%s)", de->value,
			   type->kind, type->name, type->repo_id);
		break;
	}

	return any;
}

static void
free_dir_entry (DirEntry *de)
{
	g_free (de->name);
	g_free (de->value);
	g_slist_foreach (de->subvalues, (GFunc) free_dir_entry, NULL);
	g_slist_free (de->subvalues);
	g_free (de);
}

static void
clear_dir_entry (DirEntry *de)
{
	g_free (de->value);
	de->value = NULL;
	g_slist_foreach (de->subvalues, (GFunc) free_dir_entry, NULL);
	g_slist_free (de->subvalues);
	de->subvalues = NULL;
}

void
bonobo_config_ditem_encode_any (BonoboConfigDItem *ditem, DirEntry *de, const gchar *path,
				const CORBA_any *any, CORBA_Environment *ev)
{
	g_return_if_fail (ditem != NULL);
	g_return_if_fail (BONOBO_IS_CONFIG_DITEM (ditem));
	g_return_if_fail (any != NULL);
	g_return_if_fail (de != NULL);
	g_return_if_fail (ev != NULL);

	switch (any->_type->kind) {

#define ENCODE_BASIC(k,t,f) \
case CORBA_tk_##k##: \
	clear_dir_entry (de); \
	de->value = g_strdup_printf (f, BONOBO_ARG_GET_GENERAL (any, TC_CORBA_##t##, CORBA_##t##, ev)); \
	break;

		ENCODE_BASIC (short,     short,              "%hd");
		ENCODE_BASIC (long,      long,               "%d");
		ENCODE_BASIC (ushort,    unsigned_short,     "%hu");
		ENCODE_BASIC (ulong,     unsigned_long,      "%u");
		ENCODE_BASIC (longlong,  long_long,          "%lld");
		ENCODE_BASIC (float,     float,              "%g");
		ENCODE_BASIC (double,    double,             "%G");
		ENCODE_BASIC (ulonglong, unsigned_long_long, "%llu");
		ENCODE_BASIC (char,      char,               "%c");
		ENCODE_BASIC (octet,     octet,              "%d");

#undef ENCODE_BASIC

	case CORBA_tk_boolean:
		clear_dir_entry (de);
		de->value = g_strdup (BONOBO_ARG_GET_BOOLEAN (any) ? "true" : "false");
		break;

	case CORBA_tk_string:
		clear_dir_entry (de);
		de->value = g_strdup (BONOBO_ARG_GET_STRING (any));
		break;

	case CORBA_tk_struct: {
		DynamicAny_DynStruct dynstruct;
		CORBA_NameValuePairSeq *members;
		DirEntry *subentry;
		gulong i, j;
		GSList *l;

		dynstruct = CORBA_ORB_create_dyn_struct (bonobo_orb (), any->_type, ev);

		DynamicAny_DynAny_from_any (dynstruct, (CORBA_any *) any, ev);

		members = DynamicAny_DynStruct_get_members (dynstruct, ev);

		clear_dir_entry (de);

		for (i = 0; i < members->_length; i++) {
			CORBA_NameValuePair *pair = &members->_buffer [i];

			if (CORBA_TypeCode_equal (pair->value._type, TC_GNOME_ExtraAttributes, NULL)) {
				Bonobo_PropertySet *list = pair->value._value;

				for (j = 0; j < list->_length; j++) {
					subentry = g_new0 (DirEntry, 1);
					subentry->name = g_strdup (list->_buffer [j].name);
					bonobo_config_ditem_encode_any (ditem, subentry, path,
									&list->_buffer [j].value, ev);
					de->subvalues = g_slist_prepend (de->subvalues, subentry);
				}
			} else {
				subentry = g_new0 (DirEntry, 1);
				subentry->name = g_strdup (pair->id);

				bonobo_config_ditem_encode_any (ditem, subentry, path, &pair->value, ev);

				de->subvalues = g_slist_prepend (de->subvalues, subentry);
			}
		}

		break;
	}

	case CORBA_tk_enum: {
		DynamicAny_DynEnum dynenum;
		gchar *value;

		dynenum = CORBA_ORB_create_dyn_enum (bonobo_orb (), any->_type, ev);

		DynamicAny_DynAny_from_any (dynenum, (CORBA_any *) any, ev);

		value = DynamicAny_DynEnum_get_as_string (dynenum, ev);

		clear_dir_entry (de);
		if (CORBA_TypeCode_equal (any->_type, TC_GNOME_DesktopEntryType, NULL))
			de->value = g_strdown (g_strdup (value + 19));
		else
			de->value = g_strdup (value);

		CORBA_free (value);

		break;
	}

	case CORBA_tk_alias: {
		CORBA_any *new_value;
		CORBA_TypeCode type;

		type = any->_type->subtypes [0];
		new_value = bonobo_arg_copy (any);
		CORBA_Object_release ((CORBA_Object) new_value->_type, NULL);
		new_value->_type = (CORBA_TypeCode) CORBA_Object_duplicate ((CORBA_Object )type, NULL);

		bonobo_config_ditem_encode_any (ditem, de, path, new_value, ev);

		bonobo_arg_release (new_value);

		break;
	}

	case CORBA_tk_sequence: {
		DynamicAny_DynSequence dynseq;
		DynamicAny_DynAny_AnySeq *dynany_anyseq;
		CORBA_TypeCode type;
		DirEntry *subentry;
		gulong i;

		dynseq = CORBA_ORB_create_dyn_sequence (bonobo_orb (), any->_type, ev);

		DynamicAny_DynAny_from_any (dynseq, (CORBA_any *) any, ev);

		dynany_anyseq = DynamicAny_DynSequence_get_elements (dynseq, ev);

		type = any->_type->subtypes [0];

		clear_dir_entry (de);

		for (i = 0; i < dynany_anyseq->_length; i++) {
			if (CORBA_TypeCode_equal (type, TC_GNOME_LocalizedString, NULL)) {
				GNOME_LocalizedString *localized = dynany_anyseq->_buffer [i]->_value;

				if (localized->locale) {
					subentry = g_new0 (DirEntry, 1);
					subentry->name = g_strdup (localized->locale);
					subentry->value = g_strdup (localized->text);
					de->subvalues = g_slist_prepend (de->subvalues, subentry);
				} else
					de->value = g_strdup (localized->text);
			} else {
				subentry = g_new0 (DirEntry, 1);

				bonobo_config_ditem_encode_any (ditem, subentry, path,
								dynany_anyseq->_buffer [i], ev);

				de->subvalues = g_slist_prepend (de->subvalues, subentry);
			}
		}

		break;
	}

	default:
		g_message (G_STRLOC ": %d - %s (%s)", any->_type->kind,
			   any->_type->name, any->_type->repo_id);

		break;
	}
}

