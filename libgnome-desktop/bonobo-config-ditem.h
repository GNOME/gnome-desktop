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

G_BEGIN_DECLS

#define BONOBO_CONFIG_DITEM_TYPE        (bonobo_config_ditem_get_type ())
#define BONOBO_CONFIG_DITEM(o)	        (G_TYPE_CHECK_INSTANCE_CAST ((o), BONOBO_CONFIG_DITEM_TYPE, BonoboConfigDItem))
#define BONOBO_CONFIG_DITEM_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), BONOBO_CONFIG_DITEM_TYPE, BonoboConfigDItemClass))
#define BONOBO_IS_CONFIG_DITEM(o)       (G_TYPE_CHECK_INSTANCE_CAST ((o), BONOBO_CONFIG_DITEM_TYPE))
#define BONOBO_IS_CONFIG_DITEM_CLASS(k) (G_TYPE_CHECK_CLASS_CAST ((k), BONOBO_CONFIG_DITEM_TYPE))

typedef struct _BonoboConfigDItem        BonoboConfigDItem;

struct _BonoboConfigDItem {
	BonoboConfigDatabase  base;
	
	char                 *filename;
	guint                 time_id;

	BonoboEventSource    *es;
};

typedef struct {
	BonoboConfigDatabaseClass parent_class;
} BonoboConfigDItemClass;


GType		      
bonobo_config_ditem_get_type  (void);

Bonobo_ConfigDatabase
bonobo_config_ditem_new (const char *filename);

G_END_DECLS

#endif /* ! __BONOBO_CONFIG_DITEM_H__ */
