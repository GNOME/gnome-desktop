/* -*- Mode: C; c-set-style: gnu indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/**
 * bonobo-config-ditem-file.h: ditem configuration database implementation.
 *
 * Author:
 *   Martin Baulig (baulig@suse.de)
 *
 * Copyright 2001 SuSE Linux AG.
 */
#ifndef __BONOBO_CONFIG_DITEM_FILE_H__
#define __BONOBO_CONFIG_DITEM_FILE_H__

#include <stdio.h>
#include <bonobo-config/bonobo-config-database.h>
#include <bonobo/bonobo-event-source.h>
#include <gnome-desktop/GNOME_Desktop.h>

G_BEGIN_DECLS

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

void
bonobo_config_ditem_setup_section (BonoboConfigDItemSection *section, const char *name, CORBA_TypeCode tc);

void
bonobo_config_ditem_write_key (BonoboConfigDItemKey *key, GNOME_Desktop_Encoding encoding, const char *value);

gboolean
bonobo_config_ditem_sync_key (BonoboConfigDItemKey *key);

BonoboConfigDItemKey *
bonobo_config_ditem_lookup_key (BonoboConfigDItemSection *section, const char *name, gboolean create);

BonoboConfigDItemSection *
bonobo_config_ditem_lookup_section (BonoboConfigDItemDirectory *dir, const char *name);

BonoboConfigDItemKey *
bonobo_config_ditem_lookup (BonoboConfigDItemDirectory *dir, const char *name, gboolean create);

void
bonobo_config_ditem_free_directory (BonoboConfigDItemDirectory *dir);

BonoboConfigDItemDirectory *
bonobo_config_ditem_load (const gchar *file);

G_END_DECLS

#endif /* ! __BONOBO_CONFIG_DITEM_FILE_H__ */
