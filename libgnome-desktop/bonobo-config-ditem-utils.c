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
	gchar buffer [BUFSIZ+3], *pos, sep = ',';
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

		if (*c == sep) {
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
bonobo_config_ditem_decode_any (DirEntry *de, CORBA_TypeCode type, CORBA_Environment *ev)
{
	CORBA_any *any = NULL;

	g_return_val_if_fail (de != NULL, NULL);
	g_return_val_if_fail (ev != NULL, NULL);

#if 0
	g_message (G_STRLOC ": |%s| - %d - %s (%s)", de->value,
		   type->kind, type->name, type->repo_id);
#endif

	switch (type->kind) {
	case CORBA_tk_string:
		any = bonobo_arg_new (TC_CORBA_string);
		BONOBO_ARG_SET_STRING (any, de->value);
		break;

	case CORBA_tk_alias:
		any = bonobo_config_ditem_decode_any (de, type->subtypes [0], ev);
		CORBA_Object_release ((CORBA_Object) any->_type, NULL);
		any->_type = (CORBA_TypeCode) CORBA_Object_duplicate ((CORBA_Object )type, NULL);
		break;

	case CORBA_tk_sequence: {
		DynamicAny_DynSequence dynseq;
		DynamicAny_DynAny_AnySeq *dynany_anyseq;
		gulong length = 0, i;
		GSList *l, *array = NULL;

		if (de->subvalues)
			length = g_slist_length (de->subvalues);
		else if (de->value) {
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

			i = 0;
			while (l) {
				DirEntry *subentry = (DirEntry *)l->data;

				dynany_anyseq->_buffer [i] = bonobo_config_ditem_decode_any
					(subentry, type->subtypes [0], ev);

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

				g_message (G_STRLOC ": `%s'", subentry.value);

				dynany_anyseq->_buffer [i] = bonobo_config_ditem_decode_any
					(&subentry, type->subtypes [0], ev);

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
		gulong length, i;
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

			l = g_slist_find_custom (de->subvalues, member_name, key_compare_func);
			if (l) {
				subentry = l->data;
				subvalue = bonobo_config_ditem_decode_any (subentry, member_type, ev);
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

	default:
		break;
	}

	return any;
}
