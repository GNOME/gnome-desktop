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

/* fixme: this is a ORBit function, which should be public */
static gpointer
ORBit_demarshal_allocate_mem(CORBA_TypeCode tc, gint nelements)
{
	size_t block_size;
	gpointer retval = NULL;

	if (!nelements)
		return retval;

	block_size = ORBit_gather_alloc_info (tc);

	if (block_size) {
		retval = ORBit_alloc_2 
			(block_size * nelements,
			 (ORBit_free_childvals) ORBit_free_via_TypeCode,
			 GINT_TO_POINTER (nelements), sizeof (CORBA_TypeCode));

		*(CORBA_TypeCode *)((char *)retval - 
				    sizeof(ORBit_mem_info) - 
				    sizeof(CORBA_TypeCode)) = 
			(CORBA_TypeCode)CORBA_Object_duplicate
			((CORBA_Object)tc, NULL);
	}

	return retval;
}

CORBA_any *
bonobo_config_ditem_decode_any (DirEntry *de, CORBA_TypeCode type, CORBA_Environment *ev)
{
	CORBA_any *any = NULL;

	g_return_val_if_fail (de != NULL, NULL);
	g_return_val_if_fail (ev != NULL, NULL);

	g_message (G_STRLOC ": |%s| - %d - %s (%s)", de->value,
		   type->kind, type->name, type->repo_id);

	switch (type->kind) {
	case CORBA_tk_string:
		any = bonobo_arg_new (TC_string);
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
		gulong length, i;
		GSList *l;

		length = g_slist_length (de->subvalues);

		dynseq = CORBA_ORB_create_dyn_sequence (bonobo_orb (), type, ev);
		DynamicAny_DynSequence_set_length (dynseq, length, ev);

		dynany_anyseq = CORBA_sequence_DynamicAny_DynAny_AnySeq__alloc ();
		dynany_anyseq->_length = length;
		dynany_anyseq->_buffer = CORBA_sequence_DynamicAny_DynAny_AnySeq_allocbuf (length);

		l = de->subvalues;

		i = 0;
		while (l) {
			DirEntry *subentry = (DirEntry *)l->data;

			dynany_anyseq->_buffer [i] = bonobo_config_ditem_decode_any
				(subentry, type->subtypes [0], ev);

			l = l->next;
			i++;
		}

		DynamicAny_DynSequence_set_elements (dynseq, dynany_anyseq, ev);

		any = DynamicAny_DynAny_to_any (dynseq, ev);

		break;

	}

	case CORBA_tk_struct:
		if (CORBA_TypeCode_equal (type, TC_GNOME_LocalizedString, NULL)) {
			GNOME_LocalizedString localized;

			localized.locale = CORBA_string_dup (de->name);
			localized.text = CORBA_string_dup (de->value);

			any = bonobo_arg_new (TC_GNOME_LocalizedString);
			BONOBO_ARG_SET_GENERAL (any, localized, TC_GNOME_LocalizedString,
						GNOME_LocalizedString, ev);
		}

		break;

	default:
		break;
	}

	return any;
}
