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

#include "bonobo-config-ditem-utils.h"

CORBA_any *
bonobo_config_ditem_decode_any (const char *value, CORBA_TypeCode type, CORBA_Environment *ev)
{
	CORBA_any *any = NULL;

	g_return_val_if_fail (value != NULL, NULL);
	g_return_val_if_fail (ev != NULL, NULL);

	g_message (G_STRLOC ": |%s| - %d - %s (%s)\n", value,
		   type->kind, type->name, type->repo_id);

	any = bonobo_arg_new (TC_string);
	BONOBO_ARG_SET_STRING (any, value);

	return any;
}
