/* -*- Mode: C; c-set-style: gnu indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/**
 * bonobo-config-ditem.c: ditem configuration database implementation.
 *
 * Author:
 *   Martin Baulig (baulig@suse.de)
 *
 * Copyright 2001 SuSE Linux AG.
 */
#ifndef __BONOBO_CONFIG_DITEM_H__
#define __BONOBO_CONFIG_DITEM_H__

#include <stdio.h>
#include <bonobo-config/bonobo-config-database.h>
#include <bonobo/bonobo-event-source.h>
#include <dmalloc.h>

G_BEGIN_DECLS

#define BONOBO_CONFIG_DITEM_TYPE        (bonobo_config_ditem_get_type ())
#define BONOBO_CONFIG_DITEM(o)	        (G_TYPE_CHECK_INSTANCE_CAST ((o), BONOBO_CONFIG_DITEM_TYPE, BonoboConfigDItem))
#define BONOBO_CONFIG_DITEM_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), BONOBO_CONFIG_DITEM_TYPE, BonoboConfigDItemClass))
#define BONOBO_IS_CONFIG_DITEM(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), BONOBO_CONFIG_DITEM_TYPE))
#define BONOBO_IS_CONFIG_DITEM_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), BONOBO_CONFIG_DITEM_TYPE))

typedef struct _BonoboConfigDItem        BonoboConfigDItem;
typedef struct _BonoboConfigDItemClass   BonoboConfigDItemClass;
typedef struct _BonoboConfigDItemPrivate BonoboConfigDItemPrivate;

struct _BonoboConfigDItem {
	BonoboConfigDatabase      base;

	char                     *filename;

	BonoboConfigDItemPrivate *_priv;
};

struct _BonoboConfigDItemClass {
	BonoboConfigDatabaseClass parent_class;
};


GType		      
bonobo_config_ditem_get_type  (void);

Bonobo_ConfigDatabase
bonobo_config_ditem_new (const char *filename);

G_END_DECLS

#endif /* ! __BONOBO_CONFIG_DITEM_H__ */
