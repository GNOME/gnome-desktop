/* -*- Mode: C; c-set-style: gnu indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/**
 * bonobo-config-ditem.c: ditem configuration database implementation.
 *
 * Author:
 *   Martin Baulig (baulig@suse.de)
 *
 * Copyright 2001 SuSE Linux AG.
 */
#ifndef __BONOBO_CONFIG_DITEM_UTILS_H__
#define __BONOBO_CONFIG_DITEM_UTILS_H__

#include "bonobo-config-ditem.h"

G_BEGIN_DECLS

CORBA_any *
bonobo_config_ditem_decode_any (BonoboConfigDItem *ditem, DirEntry *de,
				const gchar *path, CORBA_TypeCode tc,
				CORBA_Environment *ev);

G_END_DECLS

#endif /* ! __BONOBO_CONFIG_DITEM_UTILS_H__ */
