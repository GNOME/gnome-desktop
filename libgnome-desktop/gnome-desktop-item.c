/* -*- Mode: C; c-set-style: linux indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gnome-ditem.h - GNOME Desktop File Representation 

   Copyright (C) 1999, 2000 Red Hat Inc.
   All rights reserved.

   This file is part of the Gnome Library.

   Developed by Elliot Lee <sopwith@redhat.com>
   
   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
   
   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */
/*
  @NOTATION@
 */

#include "config.h"

#define DI_DEBUG 1

#include <limits.h>
#include <ctype.h>
#include <stdio.h>
#include <glib.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include "libgnomeP.h"
#include "gnome-ditem.h"
#include "gnome-util.h"
#include "gnome-config.h"
#include "gnome-exec.h"
#include "gnome-i18nP.h"
#include "gnome-url.h"
#include <popt.h>

#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs-uri.h>

struct _GnomeDesktopItem {
        GHashTable *name; /* key is language, value is translated string */
        GHashTable *comment; /* key is language, value is translated string */

	char *type; /* type as in Application, Directory, URL, ... will
		       indicate what do do with the Exec line */

        char *exec;

        char *icon_path;

        char *location;

        GHashTable *other_attributes; /* string key, string value */

        GSList *order; /* If GNOME_DESKTOP_ITEM_IS_DIRECTORY */

        time_t mtime;

        guchar refcount;

        GnomeDesktopItemFormat item_format : 4;
        GnomeDesktopItemFlags item_flags : 8;
};

/**
 * gnome_desktop_item_new:
 *
 * Creates a GnomeDesktopItem object. The reference count on the returned value is set to '1'.
 *
 * Returns: The new GnomeDesktopItem
 */
GnomeDesktopItem *
gnome_desktop_item_new(void)
{
        GnomeDesktopItem *retval;

        retval = g_new0(GnomeDesktopItem, 1);

        retval->refcount++;

        return retval;
}

static void
ditem_copy_key_value(gpointer key, gpointer value, gpointer user_data)
{
        g_hash_table_insert(user_data, g_strdup(key), g_strdup(value));
}

/**
 * gnome_desktop_item_copy:
 * @item: The item to be copied
 *
 * Creates a copy of a GnomeDesktopItem.  The new copy has a refcount of 1.
 *
 * Returns: The new copy 
 */
GnomeDesktopItem *
gnome_desktop_item_copy (const GnomeDesktopItem *item)
{
        GnomeDesktopItem *retval;
	GSList *li;

        g_return_val_if_fail(item, NULL);

        retval = gnome_desktop_item_new();

        retval->item_format = item->item_format;
        retval->item_flags = item->item_flags;

	/* g_strdup handles NULLs correctly */
        retval->icon_path = g_strdup(item->icon_path);
        retval->location = g_strdup(item->location);
        retval->exec = g_strdup(item->exec);
        retval->type = g_strdup(item->type);

        if(item->name
           && g_hash_table_size(item->name) > 0) {
                retval->name = g_hash_table_new (g_str_hash, g_str_equal);
                g_hash_table_foreach(item->name, ditem_copy_key_value, retval->name);      
        }

        if(item->comment
           && g_hash_table_size(item->comment) > 0) {
                retval->comment = g_hash_table_new (g_str_hash, g_str_equal);
                g_hash_table_foreach(item->comment, ditem_copy_key_value, retval->comment);
        }

        if(item->other_attributes
           && g_hash_table_size(item->other_attributes) > 0) {
                retval->other_attributes = g_hash_table_new (g_str_hash, g_str_equal);
                g_hash_table_foreach(item->other_attributes, ditem_copy_key_value, retval->other_attributes);
        }

        retval->order = g_slist_copy(item->order);
	for(li = retval->order; li; li = li->next)
		li->data = g_strdup(li->data);

        return retval;
}

/* If you find a bug here, odds are that it exists in ditem_gnome_load() too */
static GnomeDesktopItem *
ditem_kde_load (const char *data_file, GnomeDesktopItemLoadFlags flags,
		GnomeDesktopItemFlags item_flags)
{
        GnomeDesktopItem *retval = gnome_desktop_item_new();
        char confpath[PATH_MAX];
        char *key, *value;
        void *iter;

        g_snprintf(confpath, sizeof(confpath), "=%s=/KDE Desktop Entry", data_file);

        retval->item_flags = item_flags;
        retval->name = g_hash_table_new(g_str_hash, g_str_equal);
        retval->comment = g_hash_table_new(g_str_hash, g_str_equal);
        retval->other_attributes = g_hash_table_new(g_str_hash, g_str_equal);

        iter = gnome_config_init_iterator(confpath);
        while((iter = gnome_config_iterator_next(iter, &key, &value))) {
		/* we need to keep things where the value is empty however */
                if(!*key) {
                        g_free(key);
                        g_free(value);
                        continue;
                }

                if(!strcmp(key, "Name")) {
                        g_hash_table_insert(retval->name, g_strdup("C"), value);
                } else if(!strncmp(key, "Name[", 5)) {
                        char *mylang, *ctmp;

                        mylang = key + 5;
                        ctmp = strchr(mylang, ']');
                        if(ctmp) *ctmp = '\0';

                        g_hash_table_insert(retval->name, g_strdup(mylang), value);
                        g_free(key);
                } else if(!strcmp(key, "Comment")) {
                        g_hash_table_insert(retval->comment, g_strdup("C"), value);
                } else if(!strncmp(key, "Comment[", strlen("Comment["))) {
                        char *mylang, *ctmp;

                        mylang = key + strlen("Comment[");
                        ctmp = strchr(mylang, ']');
                        if(ctmp) *ctmp = '\0';

                        g_hash_table_insert(retval->name, g_strdup(mylang), value);
                        g_free(key);
                } else if(!strcmp(key, "Icon")) {
                        retval->icon_path = value;
                        g_free(key);
                } else if(!strcmp(key, "Terminal")) {
                        if(tolower(*value) == 'y' || tolower(*value) == 't' || atoi(value))
                                retval->item_flags |= GNOME_DESKTOP_ITEM_RUN_IN_TERMINAL;
                        else
                                retval->item_flags &= (~GNOME_DESKTOP_ITEM_RUN_IN_TERMINAL);

                        g_free(key);
                        g_free(value);
                } else if(!strcmp(key, "Exec")) {
			retval->exec = value;
			if(strstr(value,"%f")) {
				retval->item_flags |= GNOME_DESKTOP_ITEM_NO_URL_DROP;
			} else if(strstr(value,"%F")) {
				retval->item_flags |= GNOME_DESKTOP_ITEM_NO_URL_DROP |
					GNOME_DESKTOP_ITEM_SINGLE_FILE_DROP_ONLY;
			} else if(strstr(value,"%u")) {
				/* everything is set just right :), don't touch it */
			} else 
				retval->item_flags |= GNOME_DESKTOP_ITEM_OLD_STYLE_DROP;
                        g_free(key);
                } else if(!strcmp(key, "Type")) {
			retval->type = value;
			if(strcmp(value,"KonsoleApplication")==0)
				retval->item_flags |=
					GNOME_DESKTOP_ITEM_RUN_IN_TERMINAL;
                        g_free(key);
                } else if(!strcmp(key, "SortOrder")) {
			char *p;
                        g_free(key);
			p = strtok(value,",");
			while(p) {
				retval->order = g_slist_prepend(retval->order,
								g_strdup(p));
				p = strtok(NULL,",");
			}
                        g_free(value);
			retval->order = g_slist_reverse(retval->order);
                } else {
                        g_hash_table_insert(retval->other_attributes,
					    key, value);
                }
        }

        if ((flags & GNOME_DESKTOP_ITEM_LOAD_ONLY_IF_EXISTS)
            && !(retval->item_flags & GNOME_DESKTOP_ITEM_IS_DIRECTORY)) {
                char *tryme;
                /* We don't use gnome_desktop_item_exists() here because it is more thorough than the TryExec stuff
                   which GNOME_DESKTOP_ITEM_LOAD_ONLY_IF_EXISTS specifies */

                if(!retval->exec)
                        goto errout;

                tryme = g_hash_table_lookup(retval->other_attributes, "TryExec");

                if(tryme && !(tryme = gnome_is_program_in_path(tryme))) {
			g_free(tryme);
                        goto errout;
		}
        }

        return retval;

 errout:
        gnome_desktop_item_unref(retval);
        return NULL;
}

/* There is entirely too much duplication between this function and ditem_kde_load() */
static GnomeDesktopItem *
ditem_gnome_load (const char *data_file, GnomeDesktopItemLoadFlags flags,
		  GnomeDesktopItemFlags item_flags)
{
        GnomeDesktopItem *retval = gnome_desktop_item_new();
        char confpath[PATH_MAX];
        char *key, *value;
        void *iter;
	int got_drop_keys = FALSE; /* if any new drop keys were set, otherwise use
				      OLD_STYLE_DROP */

        g_snprintf(confpath, sizeof(confpath), "=%s=/Desktop Entry", data_file);

        retval->item_flags = item_flags;
        retval->name = g_hash_table_new(g_str_hash, g_str_equal);
        retval->comment = g_hash_table_new(g_str_hash, g_str_equal);
        retval->other_attributes = g_hash_table_new(g_str_hash, g_str_equal);

        iter = gnome_config_init_iterator(confpath);
        while((iter = gnome_config_iterator_next(iter, &key, &value))) {
		/* we need to keep things where the value is empty however */
                if(!*key) {
                        g_free(key);
                        g_free(value);
                        continue;
                }

		if(strcmp(key, "FileDropExec")==0 ||
		   strcmp(key, "URLDropExec")==0)
			got_drop_keys = TRUE;

                if(!strcmp(key, "Name")) {
                        g_hash_table_insert(retval->name, g_strdup("C"), value);
                } else if(!strncmp(key, "Name[", 5)) {
                        char *mylang, *ctmp;

                        mylang = key + strlen("Name[");
                        ctmp = strchr(mylang, ']');
                        if(ctmp) *ctmp = '\0';

                        g_hash_table_insert(retval->name, g_strdup(mylang), value);
                        g_free(key);
                } else if(!strcmp(key, "Comment")) {
                        g_hash_table_insert(retval->comment, g_strdup("C"), value);
                } else if(!strncmp(key, "Comment[", strlen("Comment["))) {
                        char *mylang, *ctmp;

                        mylang = key + strlen("Comment[");
                        ctmp = strchr(mylang, ']');
                        if(ctmp) *ctmp = '\0';

                        g_hash_table_insert(retval->name, g_strdup(mylang), value);
                        g_free(key);
                } else if(!strcmp(key, "Icon")) {
                        retval->icon_path = value;
                        g_free(key);
                } else if(!strcmp(key, "Terminal")) {
                        if(tolower(*value) == 'y' || tolower(*value) == 't' || atoi(value))
                                retval->item_flags |= GNOME_DESKTOP_ITEM_RUN_IN_TERMINAL;
                        else
                                retval->item_flags &= (~GNOME_DESKTOP_ITEM_RUN_IN_TERMINAL);

                        g_free(key);
                        g_free(value);
                } else if(!strcmp(key, "SingleFileDropOnly")) {
                        if(tolower(*value) == 'y' || tolower(*value) == 't' || atoi(value))
                                retval->item_flags |= GNOME_DESKTOP_ITEM_SINGLE_FILE_DROP_ONLY;
                        else
                                retval->item_flags &= (~GNOME_DESKTOP_ITEM_SINGLE_FILE_DROP_ONLY);
                        g_free(key);
                        g_free(value);
                } else if(!strcmp(key, "SingleURLDropOnly")) {
                        if(tolower(*value) == 'y' || tolower(*value) == 't' || atoi(value))
                                retval->item_flags |= GNOME_DESKTOP_ITEM_SINGLE_URL_DROP_ONLY;
                        else
                                retval->item_flags &= (~GNOME_DESKTOP_ITEM_SINGLE_URL_DROP_ONLY);
                        g_free(key);
                        g_free(value);
                } else if(!strcmp(key, "Exec")) {
                        retval->exec = value;
                        g_free(key);
                } else if(!strcmp(key, "Type")) {
                        retval->type = value;
                        g_free(key);
                } else {
                        g_hash_table_insert(retval->other_attributes, key, value);
                }
        }

	if(got_drop_keys) {
		char *s;
                s = g_hash_table_lookup(retval->other_attributes, "FileDropExec");
		if(!s || !*s)
			retval->item_flags |= GNOME_DESKTOP_ITEM_NO_FILE_DROP;
                s = g_hash_table_lookup(retval->other_attributes, "URLDropExec");
		if(!s || !*s)
			retval->item_flags |= GNOME_DESKTOP_ITEM_NO_URL_DROP;
	} else
		retval->item_flags |= GNOME_DESKTOP_ITEM_OLD_STYLE_DROP;

        if ((flags & GNOME_DESKTOP_ITEM_LOAD_ONLY_IF_EXISTS)
            && !(retval->item_flags & GNOME_DESKTOP_ITEM_IS_DIRECTORY)) {
                char *tryme;

                if(!retval->exec)
                        goto errout;

                tryme = g_hash_table_lookup(retval->other_attributes, "TryExec");

                if(tryme && !(tryme = gnome_is_program_in_path(tryme))) {
			g_free(tryme);
                        goto errout;
		}
        }

        /* Read the .order file */
        if(retval->item_flags & GNOME_DESKTOP_ITEM_IS_DIRECTORY) {
                FILE *fh;
		char *dn;

		dn = g_path_get_dirname(data_file);
                g_snprintf(confpath, sizeof(confpath), "%s/.order", dn);
		g_free(dn);

                fh = fopen(confpath, "r");

                if(fh) {
                        char buf[PATH_MAX];

			/* we prepend the things onto the list so the
			   list will actually be the reverse of the file */
			while(fgets(buf, PATH_MAX, fh)) {
				retval->order = g_slist_prepend(retval->order,
								g_strdup(buf));
			}

			retval->order = g_slist_reverse(retval->order);

                        fclose(fh);
                }
        }

        return retval;

 errout:
        gnome_desktop_item_unref(retval);
        return NULL;
}

/* load all the extra sections and store them as attributes in form of
   section/attribute */
static void
ditem_load_other_sections(GnomeDesktopItem *item, const char *data_file,
			  GSList *other_sections)
{
	GSList *li;

	for(li = other_sections; li!=NULL; li = li->next) {
		char confpath[PATH_MAX];
		char *key, *value;
		void *iter;
		char *secname = li->data;

		g_snprintf(confpath, sizeof(confpath),
			   "=%s=/%s", data_file, secname);

		iter = gnome_config_init_iterator(confpath);
		while((iter = gnome_config_iterator_next(iter, &key, &value))) {
			char *realkey;
			if(!*key) {
				g_free(key);
				g_free(value);
				continue;
			}
			realkey = g_strconcat(secname,"/",key,NULL);

			g_hash_table_insert(item->other_attributes,
					    realkey, value);
			g_free(key);
		}
        }
}

/**
 * gnome_desktop_item_new_from_file:
 * @file: The filename or directory path to load the GnomeDesktopItem from
 * @flags: Flags to influence the loading process
 *
 * This function loads 'file' and turns it into a GnomeDesktopItem.
 * If 'file' is a directory, it loads all the items under that
 * directory as subitems of the directory's GnomeDesktopItem.
 *
 * Returns: The newly loaded item.
 */
GnomeDesktopItem *
gnome_desktop_item_new_from_file (const char *file, GnomeDesktopItemLoadFlags flags)
{
        GnomeDesktopItem *retval;
        char subfn[PATH_MAX], headbuf[256], confpath[PATH_MAX];
        FILE *fh;
        GnomeDesktopItemFlags item_flags;
        GnomeDesktopItemFormat fmt;
        struct stat sbuf;
	GSList *other_sections = NULL; /* names of other sections */

        g_return_val_if_fail (file, NULL);

#ifdef DI_DEBUG
        /*  g_print("Loading file %s\n", file); */
#endif

        if (stat(file, &sbuf) != 0)
                return NULL;

        /* Step one - figure out what type of file this is */
        flags = 0;
        fmt = GNOME_DESKTOP_ITEM_UNKNOWN;
        item_flags = 0;
        if (S_ISDIR(sbuf.st_mode)) {
                item_flags |= GNOME_DESKTOP_ITEM_IS_DIRECTORY;
                g_snprintf(subfn, sizeof(subfn), "%s/.directory", file);
		/* get the correct time for this file */
		if (stat(subfn, &sbuf) != 0)
			sbuf.st_mtime = 0;
        } else {
		char *base;
		/* here we figure out if we're talking about a directory
		   but have passed the .directory file */
		base = g_path_get_basename(file);
		if(strcmp(base, ".directory") == 0)
			item_flags |= GNOME_DESKTOP_ITEM_IS_DIRECTORY;
		g_free(base);
                strcpy(subfn, file);
        }


        fh = fopen(subfn, "r");

        if(!fh && (item_flags & GNOME_DESKTOP_ITEM_IS_DIRECTORY))
                fmt = GNOME_DESKTOP_ITEM_GNOME; /* Empty dir becomes a GNOME thingie */
        else if(fh) {
		/* we go through the entire file looking for section headers,
		   we do this as the right section header does not have to
		   be at the top.  We however assume that it's either a
		   GNOME desktop entry or a KDE one, and never both,
		   perhaps we should us gnome_config_* for this already
		   as it would probably speed things up, as gnome_config_*
		   would already cache the file for later reading, we could
		   also just do this from the file extention! */
		while(fgets(headbuf, sizeof(headbuf), fh)) {
			char *p;
			g_strchug(headbuf);
			if(headbuf[0] != '[')
				continue;

			if(!strncmp(headbuf, "[Desktop Entry]", strlen("[Desktop Entry]"))) {
				fmt = GNOME_DESKTOP_ITEM_GNOME;
				if(flags & GNOME_DESKTOP_ITEM_LOAD_NO_OTHER_SECTIONS)
					break;
			} else if(!strncmp(headbuf, "[KDE Desktop Entry]", strlen("[KDE Desktop Entry]"))) {
				fmt = GNOME_DESKTOP_ITEM_KDE;
				if(flags & GNOME_DESKTOP_ITEM_LOAD_NO_OTHER_SECTIONS)
					break;
			}

			if(flags & GNOME_DESKTOP_ITEM_LOAD_NO_OTHER_SECTIONS)
				continue;

			p = strchr(headbuf,']');
			if(!p) continue;
			*p = '\0';
			/* store a list of other sections to read */
			other_sections =
				g_slist_prepend(other_sections,
						g_strdup(&headbuf[1]));
		}

                fclose(fh);
        }

        switch (fmt) {
        case GNOME_DESKTOP_ITEM_KDE:
                retval = ditem_kde_load(subfn, flags, item_flags);
                break;
        case GNOME_DESKTOP_ITEM_GNOME:
                retval = ditem_gnome_load(subfn, flags, item_flags);
                break;
        default:
                g_warning(_("Unknown desktop file format %d"), fmt);
		/* if we have names of other sections, free that list */
		if(other_sections) {
			g_slist_foreach(other_sections,(GFunc)g_free,NULL);
			g_slist_free(other_sections);
		}
                return NULL;
                break;
        }

        if(!retval)
                goto out;

	ditem_load_other_sections(retval, subfn, other_sections);

	/* make the 'Type' sane */
        if(item_flags & GNOME_DESKTOP_ITEM_IS_DIRECTORY) {
		/* perhaps a little ineficent way to ensure this
		   is the correct type */
		g_free(retval->type);
		retval->type = g_strdup("Directory");
	} else {
		/* this is not a directory so sanitize it by killing the
		   type altogether if type happens to be "Directory" */
		if(retval->type && strcmp(retval->type, "Directory")==0) {
			g_free(retval->type);
			retval->type = NULL;
		}
	}

	if(subfn[0] == '/')
		retval->location = g_strdup(subfn);
	else {
		char *curdir = g_get_current_dir();
		retval->location = g_concat_dir_and_file(curdir,subfn);
		g_free(curdir);
	}
        retval->item_format = fmt;
        retval->mtime = sbuf.st_mtime;

        if(!g_hash_table_size(retval->name)) {
                g_hash_table_destroy(retval->name);
                retval->name = NULL;
        }
        if(!g_hash_table_size(retval->comment)) {
                g_hash_table_destroy(retval->comment);
                retval->comment = NULL;
        }
        if(!g_hash_table_size(retval->other_attributes)) {
                g_hash_table_destroy(retval->other_attributes);
                retval->other_attributes = NULL;
        }

 out:
	/* if we have names of other sections, free that list */
	if(other_sections) {
		g_slist_foreach(other_sections,(GFunc)g_free,NULL);
		g_slist_free(other_sections);
	}
        if(!(flags & GNOME_DESKTOP_ITEM_LOAD_NO_SYNC)) {
		g_snprintf(confpath, sizeof(confpath), "=%s=", subfn);
                gnome_config_sync_file(confpath);
	}

        return retval;
}

static void
save_lang_val(gpointer key, gpointer value, gpointer user_data)
{
        char *valptr, tmpbuf[256];

        if(!key || !strcmp(key, "C"))
                valptr = user_data;
        else
                g_snprintf((valptr = tmpbuf), sizeof(tmpbuf), "%s[%s]", (char *)user_data, (char *)key);

        gnome_config_set_string(valptr, value);
}

static void
save_key_val(gpointer key, gpointer value, gpointer user_data)
{
	GnomeDesktopItem *item = user_data;
	
	/* in case the user set the OLD_STYLE_DROP flag on ignore
	   the FileDropExec and URLDropExec so that next time we
	   load we get the OLD_STYLE_DROP behaviour */
	if(item->item_flags == GNOME_DESKTOP_ITEM_OLD_STYLE_DROP &&
	   (strcmp(key,"FileDropExec")==0 ||
	    strcmp(key,"URLDropExec")==0))
		return;

        gnome_config_set_string(key, value);
}

/**
 * gnome_desktop_item_save:
 * @item: A desktop item
 * @under: The directory to save the tree underneath, can be NULL
 *
 * Writes the specified item to disk.  If the 'under' is NULL, the original
 * location is used.  It sets the location of this entry to point to the
 * new location.
 *
 * Returns: boolean. %TRUE if the file was saved, %FALSE otherwise
 */
gboolean
gnome_desktop_item_save (GnomeDesktopItem *item, const char *under)
{
        char fnbuf[PATH_MAX + sizeof("==/KDE Desktop Entry/")];

        g_return_val_if_fail(item, FALSE);
        g_return_val_if_fail(under, FALSE);
        g_return_val_if_fail(item->location, FALSE);

	/* first we setup the new location if we need to */
	if(under) {
		char *old_loc = item->location;
		char *base;
		base = g_path_get_basename(item->location);
		item->location = g_concat_dir_and_file(under, base);
		g_free(base);
		g_free(old_loc);
	}

	/* save the .order for gnome directory entries */
	if(item->item_flags & GNOME_DESKTOP_ITEM_IS_DIRECTORY &&
	   item->item_format == GNOME_DESKTOP_ITEM_GNOME) {
                FILE *fh;
		char *dir = g_path_get_dirname(item->location);

                g_snprintf(fnbuf, sizeof(fnbuf), "%s/.order", dir);

		g_free(dir);

		fh = fopen(fnbuf, "w");

		if(fh) {
			GSList *li;
			for(li = item->order; li; li = li->next)
				fprintf(fh, "%s\n", (char *)li->data);
			fclose(fh);
		} else
			return FALSE;
	}

	/* clean the file first, do not drop as if we dropped it the old
	   stuff would be realoded on the next gnome_config_set_*, also
	   this saves a syscall for the file remove */
	g_snprintf(fnbuf, sizeof(fnbuf), "=%s=", item->location);
	gnome_config_clean_file(fnbuf);

	g_snprintf(fnbuf, sizeof(fnbuf), "=%s=/%sDesktop Entry/",
		   item->location,
		   (item->item_format==GNOME_DESKTOP_ITEM_KDE)?"KDE ":"");

        gnome_config_push_prefix(fnbuf);

        if(item->icon_path)
                gnome_config_set_string("Icon", item->icon_path);

        if(item->exec)
                gnome_config_set_string("Exec", item->exec);

	/* set the type, if we haven't got one, make one up, trying to
	   guess the correct one */
        if(item->type)
                gnome_config_set_string("Type", item->type);
	else
                gnome_config_set_string("Type", (item->item_flags & GNOME_DESKTOP_ITEM_IS_DIRECTORY)?"Directory":"Application");

        if(item->name)
                g_hash_table_foreach(item->name, save_lang_val, "Name");
	if(!gnome_desktop_item_get_name(item, "C")) {
		/* whoops, there is not default for name if no language is
		 * found, this could cause this not to be loaded so set it
		 * to the name of the file if possible, or "Unknown"
		 * (not to be translated "C" is supposed to be english) */
		char *name;
		name = g_path_get_basename(item->location);
		if(!name)
			name = g_strdup("Unknown");
		gnome_config_set_string("Name", name);
		g_free(name);
	}

        if(item->comment)
                g_hash_table_foreach(item->comment, save_lang_val, "Comment");
        if(item->other_attributes)
                g_hash_table_foreach(item->other_attributes,
				     save_key_val,
				     item);
        if(item->item_flags & GNOME_DESKTOP_ITEM_RUN_IN_TERMINAL)
                gnome_config_set_bool("Terminal", TRUE);

        if(item->item_format == GNOME_DESKTOP_ITEM_KDE &&
	   item->order) {
		GString *gs;
		GSList *li;

		gs = g_string_new("");
		for(li = item->order; li; li = li->next) {
			if(li != item->order)
				g_string_append_c(gs,',');
			g_string_append(gs,li->data);
		}

                gnome_config_set_string("SortOrder", gs->str);
		g_string_free(gs,TRUE);
        }

        gnome_config_pop_prefix();

        item->mtime = time(NULL);

	/* sync the file onto disk */
	g_snprintf(fnbuf, sizeof(fnbuf), "=%s=", item->location);

	return gnome_config_sync_file(fnbuf);
}

/**
 * gnome_desktop_item_ref:
 * @item: A desktop item
 *
 * Increases the reference count of the specified item.
 */
void
gnome_desktop_item_ref (GnomeDesktopItem *item)
{
        g_return_if_fail(item);

        item->refcount++;
}

static gboolean
ditem_free_key_value(gpointer key, gpointer value, gpointer user_data)
{
        g_free(key);
        g_free(value);
        return TRUE;
}

/**
 * gnome_desktop_item_ref:
 * @item: A desktop item
 *
 * Decreases the reference count of the specified item, and destroys the item if there are no more references left.
 */
void
gnome_desktop_item_unref (GnomeDesktopItem *item)
{
        g_return_if_fail(item);

        item->refcount--;

        if(item->refcount != 0)
                return;

        g_free(item->type);
        g_free(item->exec);
        g_free(item->icon_path);
        g_free(item->location);

        if(item->name) {
                g_hash_table_foreach_remove(item->name, ditem_free_key_value, NULL);
                g_hash_table_destroy(item->name);
        }

        if(item->comment) {
                g_hash_table_foreach_remove(item->comment, ditem_free_key_value, NULL);
                g_hash_table_destroy(item->comment);
        }

        if(item->other_attributes) {
                g_hash_table_foreach_remove(item->other_attributes, ditem_free_key_value, NULL);
                g_hash_table_destroy(item->other_attributes);
        }

        g_slist_foreach(item->order, (GFunc)g_free, NULL);
        g_slist_free(item->order);

        g_free(item);
}

/* replace %? (ps) with a string, the string can be NULL which is the same
   as "", it works on a vector and appends the 'tofree' with strings which
   should be freed later, returns TRUE if it replaced something, FALSE
   otherwise, if only_once is true we jump out after the first, it can actually
   only replace one 'ps' per argument to avoid infinite loops (in case 'string'
   would contain 'ps' */
static gboolean
replace_percentsign(int argc, char **argv, const char *ps,
		    const char *string, GSList **tofree,
		    gboolean only_once)
{
	int i;
	gboolean ret = FALSE;
	if(!string) string = "";
	for(i=0;i<argc;i++) {
		char *arg = argv[i];
		char *p = strstr(arg,ps);
		char *s;
		size_t start, string_len, ps_len;
		if(!p) continue;
		string_len = strlen(string);
		ps_len = strlen(ps);
		s = g_new(char, strlen(arg) - ps_len + string_len + 1);
		start = p - arg;
		strncpy(s,arg,start);
		strcpy(&s[start],string);
		strcpy(&s[start+string_len],&arg[start+ps_len]);
		argv[i] = s;
		*tofree = g_slist_prepend(*tofree, s);
		ret = TRUE;
		if(only_once) return ret;
	}
	return ret;
}

/* the appargv, will get modified but not freed, the replacekdefiles
   tells us to replace the %f %u and %F for kde files with nothing */
static int
ditem_execute(const GnomeDesktopItem *item, int appargc, const char *appargv[], int argc, const char *argv[], gboolean replacekdefiles)
{
        char **real_argv;
        int real_argc;
        int i, j, ret;
	char **term_argv = NULL;
	int term_argc = 0;
	GSList *tofree = NULL;

        g_return_val_if_fail(item, -1);
        g_return_val_if_fail(item->exec, -1);

        if(!argv)
                argc = 0;


	/* check the type, if there is one set */
	if(item->type) {
		if(item->item_format == GNOME_DESKTOP_ITEM_KDE) {
			if(strcmp(item->type,"KonsoleApplication")!=0 &&
			   strcmp(item->type,"Application")!=0) {
				/* we are not an application so just exit
				   with an error */
				g_warning(_("Attempting to launch a non-application"));
				return -1;
			}
		} else if(item->item_format == GNOME_DESKTOP_ITEM_GNOME) {
			if(strcmp(item->type,"Application")!=0) {
				/* we are not an application so just exit
				   with an error */
				g_warning(_("Attempting to launch a non-application"));
				return -1;
			}
		} else {
			g_warning(_("Unknown desktop file format %d"),
				  item->item_format);
		}
	}

	if(item->item_flags & GNOME_DESKTOP_ITEM_RUN_IN_TERMINAL) {
		/* somewhat hackish I guess, we should use this in
		 * the prepend mode rather then just getting a new
		 * vector, but I didn't want to rewrite this whole
		 * function */
		term_argc = 0;
		term_argv = NULL;
		gnome_prepend_terminal_to_vector (&term_argc, &term_argv);
	}

        real_argc = term_argc + argc + appargc;
        real_argv = g_alloca((real_argc + 1) * sizeof(char *));

        for(i = 0; i < term_argc; i++)
                real_argv[i] = term_argv[i];

        for(j = 0; j < appargc; j++, i++)
                real_argv[i] = (char *)appargv[j];

        for(j = 0; j < argc; j++, i++)
                real_argv[i] = (char *)argv[j];

	/* if we are kde, replace the standard %? thingies */
	if(item->item_format == GNOME_DESKTOP_ITEM_KDE) {
		if(replacekdefiles) {
			replace_percentsign(real_argc, real_argv, "%f", NULL, &tofree, TRUE);
			replace_percentsign(real_argc, real_argv, "%F", NULL, &tofree, TRUE);
			replace_percentsign(real_argc, real_argv, "%u", NULL, &tofree, TRUE);
		}
		replace_percentsign(real_argc, real_argv, "%d", g_getenv("PWD"), &tofree, FALSE);
		replace_percentsign(real_argc, real_argv, "%i", item->icon_path, &tofree, FALSE);
		replace_percentsign(real_argc, real_argv, "%m", 
				    gnome_desktop_item_get_attribute(item,"MiniIcon"), &tofree, FALSE);
		replace_percentsign(real_argc, real_argv, "%c", 
				    gnome_desktop_item_get_local_comment(item), &tofree, FALSE);
	}

        ret = gnome_execute_async(NULL, real_argc, real_argv);

	if(tofree) {
		g_slist_foreach(tofree,(GFunc)g_free,NULL);
		g_slist_free(tofree);
	}

	if(term_argv)
		g_strfreev(term_argv);

	return ret;
}

/* strip any trailing &, return FALSE if bad things happen and
   we end up with an empty string */
static gboolean
strip_the_amp(char *exec)
{
	size_t exec_len;

	g_strstrip(exec);
	if(!*exec) return FALSE;

	exec_len = strlen(exec);
	/* kill any trailing '&' */
	if(exec[exec_len-1] == '&') {
		exec[exec_len-1] = '\0';
		g_strchomp(exec);
	}

	/* can't exactly launch an empty thing */
	if(!*exec) return FALSE;

	return TRUE;
}

/**
 * gnome_desktop_item_launch:
 * @item: A desktop item
 * @argc: An optional count of arguments to be added to the arguments defined.
 * @argv: An optional argument array, of length 'argc', to be appended
 *        to the command arguments specified in 'item'. Can be NULL.
 *
 * This function runs the program listed in the specified 'item',
 * optionally appending additional arguments to its command line.  It uses
 * poptParseArgvString to parse the the exec string into a vector which is
 * then passed to gnome_exec_async for execution with argc and argv appended.
 *
 * Returns: The value returned by gnome_execute_async() upon execution of
 * the specified item or -1 on error.  It may also return a 0 on success
 * if pid is not available, such as in a case where the entry is a URL
 * entry.
 */
int
gnome_desktop_item_launch (const GnomeDesktopItem *item, int argc, const char **argv)
{
	const char **temp_argv = NULL;
	int temp_argc = 0;
	char *the_exec;
	int ret;

        g_return_val_if_fail(item, -1);

	if(!item->exec) {
		g_warning(_("Trying to execute an item with no 'Exec'"));
		return -1;
	}

	/* This is a URL, so launch it as a url */
	if (item->type != NULL &&
	    item->item_format == GNOME_DESKTOP_ITEM_GNOME &&
	    strcmp (item->type, "URL") == 0) {
		if (gnome_url_show (item->exec))
			return 0;
		else
			return -1;
	}

	/* make a new copy and get rid of spaces */
	the_exec = g_alloca(strlen(item->exec)+1);
	strcpy(the_exec,item->exec);

	if(!strip_the_amp(the_exec))
		return -1;

	/* we use a popt function as it does exactly what we want to do and
	   gnome already uses popt */
	if(poptParseArgvString(item->exec, &temp_argc, &temp_argv) != 0) {
		/* no we have failed in making this a vector what should
		   we do? we can pretend that the exec failed and return -1,
		   perhaps we should give a little better error? */
		return -1;
	}

	ret = ditem_execute(item, temp_argc, temp_argv, argc, argv, TRUE);

	/* free the array done by poptParseArgvString, not by
	   g_free but free, this is not a memory leak the whole thing
	   is one allocation */
	free(temp_argv);

	return ret;
}

/* replace %s, %f, %u or %F in exec with file and run */
static int
ditem_exec_single_file (const GnomeDesktopItem *item, const char *exec, char *file)
{
	const char **temp_argv = NULL;
	int temp_argc = 0;
	char *the_exec;
	GSList *tofree = NULL;
	int ret;

        g_return_val_if_fail(item, -1);
	if(!exec) {
		g_warning(_("No exec field for this drop"));
		return -1;
	}

	/* make a new copy and get rid of spaces */
	the_exec = g_alloca(strlen(exec)+1);
	strcpy(the_exec,exec);

	if(!strip_the_amp(the_exec))
		return -1;

	/* we use a popt function as it does exactly what we want to do and
	   gnome already uses popt */
	if(poptParseArgvString(exec, &temp_argc, &temp_argv) != 0) {
		/* no we have failed in making this a vector what should
		   we do? we can pretend that the exec failed and return -1,
		   perhaps we should give a little better error? */
		return -1;
	}

	/* we are gnomish and we want to use %s, but if someone uses %f, %u or %F
	   we forgive them and do it too */
	if(replace_percentsign(temp_argc, (char **)temp_argv, "%s", file, &tofree,TRUE))
		goto perc_replaced;
	if(replace_percentsign(temp_argc, (char **)temp_argv, "%f", file, &tofree,TRUE))
		goto perc_replaced;
	if(replace_percentsign(temp_argc, (char **)temp_argv, "%u", file, &tofree,TRUE))
		goto perc_replaced;
	if(replace_percentsign(temp_argc, (char **)temp_argv, "%F", file, &tofree,TRUE))
		goto perc_replaced;
	g_free(temp_argv);
	g_warning(_("Nothing to replace with a file in the exec string"));
	return -1;
perc_replaced:

	ret = ditem_execute(item, temp_argc, temp_argv, 0, NULL, FALSE);

	if(tofree) {
		g_slist_foreach(tofree,(GFunc)g_free,NULL);
		g_slist_free(tofree);
	}

	/* free the array done by poptParseArgvString, not by
	   g_free but free, this is not a memory leak the whole thing
	   is one allocation */
	free(temp_argv);

	return ret;
}

/* works sort of like strcmp(g_strstrip(s1),s2), but returns only
 * TRUE or FALSE and does not modify s1, basically it tells if s2
 * is the same as s1 stripped */
static gboolean
stripstreq(const char *s1, const char *s2)
{
	size_t len2;

	/* skip over initial spaces */
	while(*s1 == ' ' || *s1 == '\t')
		s1++;
	len2 = strlen(s2);
	/* compare inner parts */
	if(strncmp(s1, s2, len2) != 0)
		return FALSE;
	/* skip over beyond the equal parts */
	s1 += len2;
	for(; *s1; s1++) {
		/* if we find anything but whitespace that bad */
		if(*s1 != ' ' && *s1 != '\t')
			return FALSE;
	}

	return TRUE;
}

/* replace %s, %f, %u or %F in exec with files and run, the trick here is
   that we will only replace %? in case it is a complete argument */
static int
ditem_exec_multiple_files (const GnomeDesktopItem *item, const char *exec,
			   GList *files)
{
	const char **temp_argv = NULL;
	int temp_argc = 0;
	char *the_exec;
	int ret;
	int real_argc;
	const char **real_argv;
	int i,j;
	gboolean replaced = FALSE;

        g_return_val_if_fail(item, -1);
	if(!exec) {
		g_warning(_("No exec field for this drop"));
		return -1;
	}

	/* make a new copy and get rid of spaces */
	the_exec = g_alloca(strlen(exec)+1);
	strcpy(the_exec,exec);

	if(!strip_the_amp(the_exec))
		return -1;

	/* we use a popt function as it does exactly what we want to do and
	   gnome already uses popt */
	if(poptParseArgvString(exec, &temp_argc, &temp_argv) != 0) {
		/* no we have failed in making this a vector what should
		   we do? we can pretend that the exec failed and return -1,
		   perhaps we should give a little better error? */
		return -1;
	}

	real_argc = temp_argc - 1 + g_list_length(files);
	/* we allocate +2, one for NULL, one as a buffer in case
	   we don't find anything to replace */
	real_argv = g_alloca((real_argc + 2) * sizeof(char *));

	for(i=0,j=0; i < real_argc && j < temp_argc; i++, j++) {
		GList *li;
		real_argv[i] = temp_argv[j];
		/* we only replace once */
		if(replaced)
			continue;

		if(!stripstreq(real_argv[i],"%s") &&
		   !stripstreq(real_argv[i],"%f")!=0 &&
		   !stripstreq(real_argv[i],"%u")!=0 &&
		   !stripstreq(real_argv[i],"%F")!=0)
			continue;

		/* replace the argument with the files we got passed */
		for(li = files; li!=NULL; li=li->next)
			real_argv[i++] = li->data;

		/* since we will increment in the for loop itself */
		i--;

		/* don't replace again! */
		replaced = TRUE;
	}

	/* we haven't replaced anything */
	if(!replaced) {
		g_warning(_("There was no argument to replace with dropped files"));
		return -1;
	}

	ret = ditem_execute(item, real_argc, real_argv, 0, NULL, FALSE);

	/* free the array done by poptParseArgvString, not by
	   g_free but free, this is not a memory leak the whole thing
	   is one allocation */
	free(temp_argv);

	return ret;
}


/**
 * gnome_desktop_item_drop_uri_list:
 * @item: A desktop item
 * @uri_list: an uri_list as if gotten from #gnome_uri_list_extract_uris
 *
 * A list of files or urls dropped onto an icon, the proper (Url or File)
 * exec is run you can pass directly the output of 
 * #gnome_uri_list_extract_uris.  The input thus must be a GList of
 * strings in the URI format.  If the ditem didn't specify any DND
 * options we assume old style and drop by appending the list to the end
 * of 'Exec' (see #gnome_desktop_item_get_command).  If there were any urls
 * dropped the item must have specified being able to run URLs or you'll get
 * a warning and an error.  Basically if there are ANY urls in the list
 * it's taken as a list of urls and works only on apps that can take urls.
 * If the program can't take that many arguments, as many instances as
 * necessary will be started.
 *
 * Returns: The value returned by gnome_execute_async() upon execution of
 * the specified item or -1 on error.  If multiple instances are run, the
 * return of the last one is returned.
 */
/*FIXME: perhaps we should be able to handle mixed url/file lists better
  rather then treating it as a list of urls */
int
gnome_desktop_item_drop_uri_list (const GnomeDesktopItem *item,
				  GList *uri_list)
{
	GList *li;
	gboolean any_urls = FALSE;
	GList *file_list = NULL;
	int ret = -1;

	g_return_val_if_fail (item != NULL, -1);
	g_return_val_if_fail (uri_list != NULL, -1);

	/* How could you drop something on a URL entry, that would
	 * be bollocks */
	if (item->type != NULL &&
	    item->item_format == GNOME_DESKTOP_ITEM_GNOME &&
	    strcmp (item->type, "URL") == 0) {
		return -1;
	}

	/* do we even allow drops? */
	if(item->item_flags & GNOME_DESKTOP_ITEM_NO_FILE_DROP &&
	   item->item_flags & GNOME_DESKTOP_ITEM_NO_URL_DROP)
		return -1;

	/* the simple case */
	if(item->item_flags & GNOME_DESKTOP_ITEM_OLD_STYLE_DROP) {
		int i;
		int argc = g_list_length(uri_list);
		const char **argv = g_alloca((argc + 1) * sizeof(char *));

		for(i=0,li=uri_list; li; i++, li=li->next) {
			argv[i] = li->data;
		}
		argv[i] = NULL;
		return gnome_desktop_item_launch(item, argc, argv);
	}

	/* figure out if we have any urls in the dropped files,
	   this also builds a list of all the files */
	for(li=uri_list; li; li=li->next) {
		file_list = g_list_prepend(file_list, g_strdup (li->data));
	}
	if(file_list)
		file_list = g_list_reverse(file_list);

	/* if there are any urls, or if the app didn't specify a command
	   for files use the URL command */
	if(any_urls || item->item_flags & GNOME_DESKTOP_ITEM_NO_FILE_DROP) {
		if(item->item_flags & GNOME_DESKTOP_ITEM_NO_URL_DROP) {
			g_warning(_("Dropped URLs on an app that can only take files"));
			return -1;
		}

		if(item->item_flags & GNOME_DESKTOP_ITEM_SINGLE_URL_DROP_ONLY) {
			GList *li;
			const char *exec = NULL;
			if(item->item_format == GNOME_DESKTOP_ITEM_GNOME)
				exec = gnome_desktop_item_get_attribute(item,"URLDropExec");
			if(!exec) exec = item->exec;
			for(li = uri_list; li; li=li->next)
				ret = ditem_exec_single_file(item, exec, li->data);
		} else {
			const char *exec = NULL;
			if(item->item_format == GNOME_DESKTOP_ITEM_GNOME)
				exec = gnome_desktop_item_get_attribute(item,"URLDropExec");
			if(!exec) exec = item->exec;
			ret = ditem_exec_multiple_files(item, exec, uri_list);
		}
	} else {
		if(item->item_flags & GNOME_DESKTOP_ITEM_SINGLE_FILE_DROP_ONLY) {
			GList *li;
			const char *exec = NULL;
			if(item->item_format == GNOME_DESKTOP_ITEM_GNOME)
				exec = gnome_desktop_item_get_attribute(item,"FileDropExec");
			if(!exec) exec = item->exec;
			for(li = file_list; li; li=li->next)
				ret = ditem_exec_single_file(item, exec,
							     li->data);
		} else {
			const char *exec = NULL;
			if(item->item_format == GNOME_DESKTOP_ITEM_GNOME)
				exec = gnome_desktop_item_get_attribute(item,"FileDropExec");
			if(!exec) exec = item->exec;
			ret = ditem_exec_multiple_files(item, exec, file_list);
		}
	}

	if(file_list) {
		g_list_foreach(file_list,(GFunc)g_free,NULL);
		g_list_free(file_list);
	}

	return ret;
}

/**
 * gnome_desktop_item_exists:
 * @item: A desktop item
 *
 * Attempt to figure out if the program that can be executed by this item
 * actually exists.  First it tries the TryExec attribute to see if that
 * contains a program that is in the path.  Then if there is no such
 * attribute, it tries the first word of the Exec attribute.
 *
 * Returns: A boolean, %TRUE if it exists, %FALSE otherwise.
 */
gboolean
gnome_desktop_item_exists (const GnomeDesktopItem *item)
{
        g_return_val_if_fail(item, FALSE);

        if(item->other_attributes) {
                char *tryme;

                tryme = g_hash_table_lookup(item->other_attributes, "TryExec");
                if(tryme) {
                        tryme = gnome_is_program_in_path(tryme);
                        if(tryme) {
                                g_free(tryme);
                                return TRUE;
                        }
                        else
                                return FALSE;
                }
        }

        if(item->exec) {
		const char **temp_argv = NULL;
		int temp_argc = 0;
		gboolean ret = FALSE;

		/* we use a popt function as it does exactly what we want to
		   do and gnome already uses popt */
		if(poptParseArgvString(item->exec,
				       &temp_argc,
				       &temp_argv) != 0) {
			/* we have failed, so we can't actually parse the
			   exec string and do the only sane thing and
			   return FALSE */
			return FALSE;
		}

                if(temp_argv[0][0] == PATH_SEP)
                        ret = g_file_test(temp_argv[0], G_FILE_TEST_EXISTS);
                else {
                        char *tryme = gnome_is_program_in_path(temp_argv[0]);
                        if(tryme) {
                                g_free(tryme);
                                ret = TRUE;
                        }
                        else
                                ret = FALSE;
                }
		/* free the array done by poptParseArgvString, not by
		   g_free but free, this is not a memory leak the whole
		   thing is one allocation */
		free(temp_argv);
		return ret;
        }

        return TRUE;
}

/**
 * gnome_desktop_item_get_flags:
 * @item: A desktop item
 *
 * Returns: The flags associated with the specified 'item'
 */
GnomeDesktopItemFlags
gnome_desktop_item_get_flags (const GnomeDesktopItem *item)
{
        g_return_val_if_fail(item, 0);

        return item->item_flags;
}

/**
 * gnome_desktop_item_get_type:
 * @item: A desktop item
 *
 * Gets the type attribute (the 'Type' field) of the item.  This should
 * usually be 'Application' for an application, but it can be 'Directory'
 * for a directory description.  There are other types available as well.
 * The type usually indicates how the desktop item should be handeled and
 * how the 'Exec' field should be handeled.
 *
 * Returns: The type of the specified 'item'. The returned
 * memory remains owned by the GnomeDesktopItem and should not be freed.
 */
const char *
gnome_desktop_item_get_type (const GnomeDesktopItem *item)
{
        g_return_val_if_fail(item, NULL);

        return item->type;
}

/**
 * gnome_desktop_item_get_command:
 * @item: A desktop item
 *
 * Gets the command attribute (the 'Exec' field) of the item.  
 * The command should be a single string which can be parsed into
 * a vector directly for execution.  Note that it is NOT a shell
 * expression.  It is allowed to have escapes and quoting as per
 * the standard quoting rules however.
 *
 * Returns: The command associated with the specified 'item'. The returned
 * memory remains owned by the GnomeDesktopItem and should not be freed.
 */
const char *
gnome_desktop_item_get_command (const GnomeDesktopItem *item)
{
        g_return_val_if_fail(item, NULL);

        return item->exec;
}

/**
 * gnome_desktop_item_get_icon_path:
 * @item: A desktop item
 *
 * Returns: The icon filename associated with the specified 'item'.
 * The returned memory remains owned by the GnomeDesktopItem
 * and should not be freed.
 */
const char *
gnome_desktop_item_get_icon_path (const GnomeDesktopItem *item)
{
        g_return_val_if_fail(item, NULL);

        return item->icon_path;
}

/**
 * gnome_desktop_item_get_name:
 * @item: A desktop item
 * @language: The language translation for which the name should be returned. If NULL is passed, it defaults to "C".
 *
 * Returns: The human-readable name for the specified 'item', in the
 *          specified 'language'. The returned memory remains owned by
 *          the GnomeDesktopItem and should not be freed.
 * */
const char *
gnome_desktop_item_get_name (const GnomeDesktopItem *item, const char *language)
{
        g_return_val_if_fail(item, NULL);

        if(item->name)
                return g_hash_table_lookup(item->name, language?language:"C");
        else
                return NULL;
}

/**
 * gnome_desktop_item_get_comment:
 * @item: A desktop item
 * @language: The language translation for which the comment should be returned. If NULL is passed, it defaults to "C".
 *
 * Returns: The human-readable comment for the specified 'item', in the
 *          specified 'language'. The returned memory remains owned by
 *          the GnomeDesktopItem and should not be freed.
 *
 */
const char *
gnome_desktop_item_get_comment (const GnomeDesktopItem *item, const char *language)
{
        g_return_val_if_fail(item, NULL);

        if(item->comment)
                return g_hash_table_lookup(item->comment, language?language:"C");
        else
                return NULL;
}

/**
 * gnome_desktop_item_get_local_name:
 * @item: A desktop item
 *
 * Returns: The human-readable name for the specified @item, in
 *          the user's preferred locale or a fallback if
 *          necessary. The returned memory remains owned by the
 *          GnomeDesktopItem and should not be freed. If no
 *          name is set in any relevant locale, NULL is returned.
 * */

const char *
gnome_desktop_item_get_local_name   (const GnomeDesktopItem     *item)
{
	g_return_val_if_fail(item, NULL);

	if(item->name) {
		const GList *iter;

		iter = gnome_i18n_get_language_list(NULL);

		while (iter != NULL) {
			const gchar* name;

			name = g_hash_table_lookup(item->name, iter->data);

			if (name)
				return name;
			else
				iter = g_list_next(iter);
		}
		return NULL;
	} else
                return NULL;
}

/**
 * gnome_desktop_item_get_local_comment:
 * @item: A desktop item
 *
 * Returns: The human-readable comment for the specified @item, in
 *          the user's preferred locale or a fallback if
 *          necessary. The returned memory remains owned by the
 *          GnomeDesktopItem and should not be freed. If no comment
 *          exists then NULL is returned.
 * */
const char *
gnome_desktop_item_get_local_comment(const GnomeDesktopItem     *item)
{
        g_return_val_if_fail(item, NULL);

	if(item->comment) {
		const GList *iter;

		iter = gnome_i18n_get_language_list(NULL);

		while (iter != NULL) {
			const gchar* comment;

			comment = g_hash_table_lookup(item->comment, iter->data);

			if (comment)
				return comment;
			else
				iter = g_list_next(iter);
		}
		return NULL;
	} else
                return NULL;
}


/**
 * gnome_desktop_item_get_attribute:
 * @item: A desktop item
 * @attr_name: Name of the attribute to get
 *
 * Gets an extra attribute from the desktop item.  If there were other
 * sections in the item, they can be accessed as "section/key".  If you
 * attribute is a translatable string, use the
 * #gnome_desktop_item_get_local_attribute function.
 *
 * Returns: The value of the attribute 'attr_name' on the 'item'. The
 *          returned memory remains owned by the GnomeDesktopItem and
 *          should not be freed.
 *
 */
const char *
gnome_desktop_item_get_attribute (const GnomeDesktopItem *item, const char *attr_name)
{
        g_return_val_if_fail(item, NULL);
        g_return_val_if_fail(attr_name, NULL);

        if(item->other_attributes)
                return g_hash_table_lookup(item->other_attributes, attr_name);
        else
                return NULL;
}

/**
 * gnome_desktop_item_get_local_attribute:
 * @item: A desktop item
 * @attr_name: Name of the attribute to get
 *
 * Gets an extra attribute as a local string (if it was a translatable
 * attribute) from the desktop item.  If there were other
 * sections in the item, they can be accessed as "section/key".  If you
 * attribute is not a translatable string, use the
 * #gnome_desktop_item_get_attribute function.
 *
 * Returns: The value of the attribute 'attr_name' on the 'item'. The
 *          returned memory remains owned by the GnomeDesktopItem and
 *          should not be freed.
 *
 */
const char *
gnome_desktop_item_get_local_attribute (const GnomeDesktopItem *item,
					const char *attr_name)
{
        g_return_val_if_fail(item, NULL);
        g_return_val_if_fail(attr_name, NULL);

        if(item->other_attributes) {
		const GList *iter;

		iter = gnome_i18n_get_language_list(NULL);

		while (iter != NULL) {
			char *key;
			const gchar* value;
			char *lang = iter->data;

			if(lang && strcmp(lang,"C")!=0)
				key = g_strconcat(attr_name,"[",lang,"]",NULL);
			else
				key = (char *)attr_name;

			value = g_hash_table_lookup(item->other_attributes,
						    key);
			if(key != attr_name) g_free(key);

			if (value)
				return value;
			else
				iter = g_list_next(iter);
		}
	} 
	return NULL;
}

/**
 * gnome_desktop_item_get_order:
 * @item: A desktop item
 *
 * Get the order of the items in a subdirectory which this item describes.
 * When you load a directory you should load these first and then load
 * any remaining ones after.
 *
 * Returns: A GSList of strings which are base filesnames in the correct
 * order.  The returned list should not be freed as it is owned by the item.
 *
 */
const GSList *
gnome_desktop_item_get_order (const GnomeDesktopItem *item)
{
        g_return_val_if_fail(item, NULL);

        return item->order;
}

static void
ditem_add_key(gpointer key, gpointer value, gpointer user_data)
{
        GSList **list = user_data;

        *list = g_slist_prepend(*list, key);
}

/* add key to the list (in user_data), but test to see if it's
   there first */
static void
ditem_add_key_test(gpointer key, gpointer value, gpointer user_data)
{
        GSList **list = user_data;

	if(g_slist_find_custom(*list, key, (GCompareFunc)strcmp)==NULL)
		*list = g_slist_prepend(*list, key);
}

/**
 * gnome_desktop_item_get_languages:
 * @item: A desktop item
 * 
 * Gets a list of languages that you can use to get names and comments.
 *
 * Returns: A GSList of the language name strings.  The returned GSList
 * should be freed with g_slist_free, but the strings inside the list
 * should not.
 *
 */
GSList *
gnome_desktop_item_get_languages(const GnomeDesktopItem *item)
{
        GSList *retval = NULL;

        g_return_val_if_fail(item, NULL);

        if(item->name) {
                g_hash_table_foreach(item->name, ditem_add_key, &retval);
        }
        if(item->comment) {
                g_hash_table_foreach(item->name, ditem_add_key_test, &retval);
        }

        return retval;
}

/**
 * gnome_desktop_item_get_attributes:
 * @item: A desktop item
 *
 * Gets the list of the attributes which you can can get with the
 * #gnome_desktop_item_get_attribute function.
 *
 * Returns: A GSList of the attribute name strings.  The returned GSList
 * should be freed with g_slist_free, but the strings inside the list
 * should not.
 *
 */
GSList *
gnome_desktop_item_get_attributes(const GnomeDesktopItem *item)
{
        GSList *retval = NULL;

        g_return_val_if_fail(item, NULL);

        if(item->other_attributes) {
                g_hash_table_foreach(item->other_attributes, ditem_add_key, &retval);
        }

        return retval;
}

/**
 * gnome_desktop_item_get_format:
 * @item: A desktop item
 *
 * Returns: The format that the specified 'item' is stored on disk with
 */
GnomeDesktopItemFormat
gnome_desktop_item_get_format (const GnomeDesktopItem *item)
{
        g_return_val_if_fail(item, GNOME_DESKTOP_ITEM_UNKNOWN);

        return item->item_format;
}

/**
 * gnome_desktop_item_get_file_status:
 * @item: A desktop item
 *
 * This function checks the modification time of the on-disk file to
 * see if it is more recent than the in-memory data.
 *
 * Returns: An enum value that specifies whether the item has changed since being loaded.
 */
GnomeDesktopItemStatus
gnome_desktop_item_get_file_status (GnomeDesktopItem *item)
{
        struct stat sbuf;
        GnomeDesktopItemStatus retval;

        g_return_val_if_fail(item, GNOME_DESKTOP_ITEM_DISAPPEARED);
        g_return_val_if_fail(item->location, GNOME_DESKTOP_ITEM_DISAPPEARED);

        if(stat(item->location, &sbuf) != 0)
                return GNOME_DESKTOP_ITEM_DISAPPEARED;

        retval = (sbuf.st_mtime > item->mtime)?GNOME_DESKTOP_ITEM_CHANGED:GNOME_DESKTOP_ITEM_UNCHANGED;

        item->mtime = sbuf.st_mtime;

        return retval;
}

/**
 * gnome_desktop_item_get_location:
 * @item: A desktop item
 *
 * Returns: The file location associated with 'item'. The returned
 *          memory remains owned by the GnomeDesktopItem and should
 *          not be freed.
 *
 */
const char *
gnome_desktop_item_get_location (const GnomeDesktopItem *item)
{
        return item->location;
}

/******* Set... ******/

/**
 * gnome_desktop_item_clear_name:
 * @item: A desktop item
 *
 * Clear the name field for this item for all the languages.
 *
 * Returns:
 */
void
gnome_desktop_item_clear_name(GnomeDesktopItem *item)
{
        if(item->name) {
                g_hash_table_foreach_remove(item->name,
					    ditem_free_key_value, NULL);
                g_hash_table_destroy(item->name);
		item->name = NULL;
        }
}

/**
 * gnome_desktop_item_set_name:
 * @item: A desktop item
 * @language: The language for which to set the item name
 * @name: The item's name in the specified 'language'
 *
 * Set the name for language 'language' to 'name'.  If 'language' is
 * NULL it defaults to "C".  If 'name' is NULL it removes the name
 * for that language.
 *
 * Returns:
 */
void
gnome_desktop_item_set_name (GnomeDesktopItem *item, const char *language, const char *name)
{
        gpointer old_val = NULL, old_key = NULL;

        g_return_if_fail(item);

	if(!language) language = "C";

	if(!name) {
		if(!item->name ||
		   !g_hash_table_lookup_extended(item->name,
						 language,
						 &old_key, &old_val))
			return;
		g_hash_table_remove(item->name, old_key);
		g_free(old_key);
		g_free(old_val);
		return;
	}

        if(!item->name)
                item->name = g_hash_table_new(g_str_hash,
					      g_str_equal);

	if(g_hash_table_lookup_extended(item->name, language,
					&old_key, &old_val)) {
		g_hash_table_insert(item->name,
				    old_key, g_strdup(name));
		g_free(old_val);
        } else
		g_hash_table_insert(item->name,
				    g_strdup(language), g_strdup(name));
}

/**
 * gnome_desktop_item_set_type:
 * @item: A desktop item
 * @type: A string with the type
 *
 * Sets the type attribute (the 'Type' field) of the item.  This should
 * usually be 'Application' for an application, but it can be 'Directory'
 * for a directory description.  There are other types available as well.
 * The type usually indicates how the desktop item should be handeled and
 * how the 'Exec' field should be handeled.
 *
 * Returns:
 *
 */
void
gnome_desktop_item_set_type (GnomeDesktopItem *item, const char *type)
{
        g_return_if_fail(item);

        g_free(item->type);
        if(type)
                item->type = g_strdup(type);
        else
                item->type = NULL;
}

/**
 * gnome_desktop_item_set_command:
 * @item: A desktop item
 * @command: A string with the command
 *
 * Sets the command attribute (the 'Exec' field) of the item.  
 * The command should be a single string which can be parsed into
 * a vector directly for execution.  Note that it is NOT a shell
 * expression.  It is allowed to have escapes and quoting as per
 * the standard quoting rules however.
 *
 * Returns:
 *
 */
void
gnome_desktop_item_set_command (GnomeDesktopItem *item, const char *command)
{
        g_return_if_fail(item);

        g_free(item->exec);
        if(command)
                item->exec = g_strdup(command);
        else
                item->exec = NULL;
}

/**
 * gnome_desktop_item_set_icon_path:
 * @item: A desktop item
 * @icon_path: A string specifying the path to the icon file for this item.
 *
 */
void
gnome_desktop_item_set_icon_path (GnomeDesktopItem *item, const char *icon_path)
{
        g_return_if_fail(item);

        g_free(item->icon_path);
        if(icon_path)
                item->icon_path = g_strdup(icon_path);
        else
                item->icon_path = NULL;
}

/**
 * gnome_desktop_item_clear_comment:
 * @item: A desktop item
 *
 * Clear the comment field for this item for all the languages.
 *
 * Returns:
 */
void
gnome_desktop_item_clear_comment(GnomeDesktopItem *item)
{

        if(item->comment) {
                g_hash_table_foreach_remove(item->comment,
					    ditem_free_key_value, NULL);
                g_hash_table_destroy(item->comment);
		item->comment = NULL;
        }
}

/**
 * gnome_desktop_item_set_comment:
 * @item: A desktop item
 * @language: The language for which to set the item comment
 * @comment: The item's comment in the specified 'language'
 *
 * Set the comment for language 'language' to 'comment'.  If 'language' is
 * NULL it defaults to "C".  If 'comment' is NULL it removes the comment
 * for that language.
 *
 * Returns:
 */
void
gnome_desktop_item_set_comment (GnomeDesktopItem *item, const char *language, const char *comment)
{
        gpointer old_val = NULL, old_key = NULL;

        g_return_if_fail(item);

	if(!language) language = "C";

	if(!comment) {
		if(!item->comment ||
		   !g_hash_table_lookup_extended(item->comment,
						 language,
						 &old_key, &old_val))
			return;
		g_hash_table_remove(item->comment, old_key);
		g_free(old_key);
		g_free(old_val);
		return;
	}

        if(!item->comment)
                item->comment = g_hash_table_new(g_str_hash,
						 g_str_equal);

        if(g_hash_table_lookup_extended(item->comment, language,
					&old_key, &old_val)) {
		g_hash_table_insert(item->comment,
				    old_key, g_strdup(comment));
		g_free(old_val);
        } else
		g_hash_table_insert(item->comment,
				    g_strdup(language), g_strdup(comment));
}

/**
 * gnome_desktop_item_set_attribute:
 * @item: A desktop item
 * @attr_name: The name of the attribute
 * @attr_value: The value of the attribute
 *
 * Set an attribute of name 'attr_name' to 'attr_value'.  If attr_value
 * is NULL it removes the attribute completely.
 *
 */
void
gnome_desktop_item_set_attribute (GnomeDesktopItem *item,
				  const char *attr_name,
				  const char *attr_value)
{
        gpointer old_val = NULL, old_key = NULL;

        g_return_if_fail(item);
        g_return_if_fail(attr_name);

	if(!attr_value) {
		if(!item->other_attributes ||
		   !g_hash_table_lookup_extended(item->other_attributes,
						 attr_name,
						 &old_key, &old_val))
			return;
		g_hash_table_remove(item->other_attributes, old_key);
		g_free(old_key);
		g_free(old_val);
		return;
	}

        if(!item->other_attributes)
                item->other_attributes = g_hash_table_new(g_str_hash,
							  g_str_equal);

        if(g_hash_table_lookup_extended(item->other_attributes, attr_name,
					&old_key, &old_val)) {
		g_hash_table_insert(item->other_attributes,
				    old_key, g_strdup(attr_value));
		g_free(old_val);
        } else
		g_hash_table_insert(item->other_attributes,
				    g_strdup(attr_name), g_strdup(attr_value));
}

/**
 * gnome_desktop_item_set_order:
 * @item: A desktop item for a directory
 * @order: A GSList strings of the basenames of the files in this order
 *
 * This function sets the list of strings 'order' as the order of this item
 * which is assumed is a directory item.  It makes a local copy of the
 * 'order' list and the strings within it.
 *
 * Returns:
 */
void
gnome_desktop_item_set_order (GnomeDesktopItem *item, GSList *order)
{
	GSList *li;

        g_return_if_fail(item);
        g_return_if_fail(item->item_flags & GNOME_DESKTOP_ITEM_IS_DIRECTORY);

        g_slist_foreach(item->order, (GFunc)g_free, NULL);
        g_slist_free(item->order);

	/* copy the items */
        item->order = g_slist_copy(order);
	for(li = item->order; li; li = li->next)
		li->data = g_strdup(li->data);
}

/**
 * gnome_desktop_item_set_flags:
 * @item: A desktop item
 * @flags: The flags to be set
 *
 * Sets the flags of 'item' to 'flags'
 *
 * Returns:
 */
void
gnome_desktop_item_set_flags (GnomeDesktopItem *item, GnomeDesktopItemFlags flags)
{
        g_return_if_fail(item);

        item->item_flags = flags;
}

/**
 * gnome_desktop_item_set_format:
 * @item: A desktop item
 * @fmt: The format to be set
 */
void
gnome_desktop_item_set_format (GnomeDesktopItem *item, GnomeDesktopItemFormat fmt)
{
        g_return_if_fail(item);

	/* correctly convert from kde to gnome if the type is
	   KonsoleApplication */
	if(fmt == GNOME_DESKTOP_ITEM_GNOME &&
	   item->item_format == GNOME_DESKTOP_ITEM_KDE &&
	   item->type &&
	   strcmp(item->type,"KonsoleApplication")==0) {
		g_free(item->type);
		item->type = g_strdup("Application");
		item->item_flags |= GNOME_DESKTOP_ITEM_RUN_IN_TERMINAL;
	}

        item->item_format = fmt;

}

/**
 * gnome_desktop_item_set_location:
 * @item: A desktop item
 * @location: A string specifying the file location of this particular item.
 *
 * Set's the 'location' of this item.  It should be a full path to the item.
 *
 * Returns:
 */
void
gnome_desktop_item_set_location (GnomeDesktopItem *item, const char *location)
{
        g_free(item->location);
        item->location = g_strdup(location);
}
