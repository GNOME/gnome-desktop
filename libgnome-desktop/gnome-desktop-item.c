/* -*- Mode: C; c-set-style: linux indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gnome-ditem.h - GNOME Desktop File Representation 

   Copyright (C) 1999 Red Hat Inc.
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
#include "gnome-defs.h"
#include "gnome-portability.h"
#include "gnome-ditem.h"
#include "gnome-util.h"
#include "gnome-config.h"
#include "gnome-exec.h"
#include "gnome-i18n.h"

struct _GnomeDesktopItem {
        GHashTable *name; /* key is language, value is translated string */
        GHashTable *comment; /* key is language, value is translated string */

        char *exec;

        char *icon_path;

        char *location;

        GHashTable *other_attributes; /* string key, string value */

        GSList *subitems; /* If GNOME_DESKTOP_ITEM_IS_DIRECTORY */

        time_t mtime;

        guchar refcount;

        GnomeDesktopItemFormat item_format : 4;
        GnomeDesktopItemFlags item_flags : 4;
};

#ifdef DI_DEBUG
/* avoid warning */
void ditem_dump(const GnomeDesktopItem *ditem, int indent_level);
void
ditem_dump(const GnomeDesktopItem *ditem, int indent_level)
{
        int i;
        for(i = 0; i < indent_level; i++)
                g_print(" ");

        g_print("%s (%s)\n", ditem->name?(char *)g_hash_table_lookup(ditem->name, "C"):"ZOT", ditem->icon_path);
        g_slist_foreach(ditem->subitems, (GFunc)ditem_dump, GINT_TO_POINTER(indent_level + 3));
}
#endif

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
 * Creates a copy of a GnomeDesktopItem
 *
 * Returns: The new copy 
 */
GnomeDesktopItem *
gnome_desktop_item_copy (const GnomeDesktopItem *item)
{
        GnomeDesktopItem *retval;

        g_return_val_if_fail(item, NULL);

        retval = gnome_desktop_item_new();

        retval->item_format = item->item_format;
        retval->item_flags = item->item_flags;
        retval->icon_path = item->icon_path?g_strdup(item->icon_path):NULL;
        retval->location = item->location?g_strdup(item->location):NULL;

        retval->exec = item->exec?g_strdup(item->exec):NULL;

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

        retval->subitems = g_slist_copy(item->subitems);
        g_slist_foreach(retval->subitems, (GFunc)gnome_desktop_item_ref, NULL);

        return retval;
}

/* If you find a bug here, odds are that it exists in ditem_gnome_load() too */
static GnomeDesktopItem *
ditem_kde_load (const char *file, const char *data_file, GnomeDesktopItemLoadFlags flags,
		GnomeDesktopItemFlags item_flags, char ***sub_sort_order)
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
                if(!*key || !*value) {
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
                        if(tolower(*key) == 'y' || tolower(*key) == 't' || atoi(value))
                                retval->item_flags |= GNOME_DESKTOP_ITEM_RUN_IN_TERMINAL;
                        else
                                retval->item_flags &= (~GNOME_DESKTOP_ITEM_RUN_IN_TERMINAL);

                        g_free(key);
                        g_free(value);
                } else if(!strcmp(key, "Exec")) {
			retval->exec = value;
                        g_free(key);
                } else if(!strcmp(key, "SortOrder")) {
                        *sub_sort_order = g_strsplit(value, ",", -1);
                        g_free(key);
                        g_free(value);
                } else {
                        g_hash_table_insert(retval->other_attributes, key, value);
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
ditem_gnome_load (const char *file, const char *data_file, GnomeDesktopItemLoadFlags flags,
		  GnomeDesktopItemFlags item_flags, char ***sub_sort_order)
{
        GnomeDesktopItem *retval = gnome_desktop_item_new();
        char confpath[PATH_MAX];
        char *key, *value;
        void *iter;

        g_snprintf(confpath, sizeof(confpath), "=%s=/Desktop Entry", data_file);

        retval->item_flags = item_flags;
        retval->name = g_hash_table_new(g_str_hash, g_str_equal);
        retval->comment = g_hash_table_new(g_str_hash, g_str_equal);
        retval->other_attributes = g_hash_table_new(g_str_hash, g_str_equal);

        iter = gnome_config_init_iterator(confpath);
        while((iter = gnome_config_iterator_next(iter, &key, &value))) {
                if(!*key || !*value) {
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
                        if(tolower(*key) == 'y' || tolower(*key) == 't' || atoi(value))
                                retval->item_flags |= GNOME_DESKTOP_ITEM_RUN_IN_TERMINAL;
                        else
                                retval->item_flags &= (~GNOME_DESKTOP_ITEM_RUN_IN_TERMINAL);

                        g_free(key);
                        g_free(value);
                } else if(!strcmp(key, "Exec")) {
                        retval->exec = value;
                        g_free(key);
                } else {
                        g_hash_table_insert(retval->other_attributes, key, value);
                }
        }

        if ((flags & GNOME_DESKTOP_ITEM_LOAD_ONLY_IF_EXISTS)
            && !(retval->item_flags & GNOME_DESKTOP_ITEM_IS_DIRECTORY))
        {
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
        if(retval->item_flags & GNOME_DESKTOP_ITEM_IS_DIRECTORY)
        {
                FILE *fh;

                g_snprintf(confpath, sizeof(confpath), "%s/.order", file);

                fh = fopen(file, "r");

                if(fh) {
			GSList *order = NULL, *li;
			int count = 0;
                        char buf[PATH_MAX];

			/* we prepend the things onto the list so the
			   list will actually be the reverse of the file */
			while(fgets(buf, PATH_MAX, fh)) {
				order = g_slist_prepend(order, g_strdup(buf));
				count++;
			}

			*sub_sort_order = g_new0(char *, count + 1);
			/* we iterate through the list and put it in our vector
			   backwards */
			li = order;
			while(--count >= 0) {
				(*sub_sort_order)[count] = li->data;
				li = li->next;
			}

			g_slist_free(order);

                        fclose(fh);
                }
        }

        return retval;

 errout:
        gnome_desktop_item_unref(retval);
        return NULL;
}

static gboolean
string_in_array(const char *str, char **array)
{
        int i;

        if(!array)
                return FALSE;

        for(i = 0; array[i]; i++) {
                if(!strcmp(str, array[i]))
                        return TRUE;
        }

        return FALSE;
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
        char **sub_sort_order;
        struct stat sbuf;

        g_return_val_if_fail (file, NULL);

#ifdef DI_DEBUG
        //  g_print("Loading file %s\n", file);
#endif

        if (stat(file, &sbuf))
                return NULL;

        /* Step one - figure out what type of file this is */
        flags = 0;
        fmt = GNOME_DESKTOP_ITEM_UNKNOWN;
        item_flags = 0;
        if (S_ISDIR(sbuf.st_mode)) {
                if(flags & GNOME_DESKTOP_ITEM_LOAD_NO_DIRS)
                        return NULL;

                item_flags |= GNOME_DESKTOP_ITEM_IS_DIRECTORY;
                g_snprintf(subfn, sizeof(subfn), "%s/.directory", file);
        } else {
                strcpy(subfn, file);
        }

        fh = fopen(subfn, "r");

        if(!fh && (item_flags & GNOME_DESKTOP_ITEM_IS_DIRECTORY))
                fmt = GNOME_DESKTOP_ITEM_GNOME; /* Empty dir becomes a GNOME thingie */
        else if(fh) {
                fgets(headbuf, sizeof(headbuf), fh);

                if(!strncmp(headbuf, "[Desktop Entry]", strlen("[Desktop Entry]")))
                {
                        fmt = GNOME_DESKTOP_ITEM_GNOME;
                }
                else if(!strncmp(headbuf, "[KDE Desktop Entry]", strlen("[KDE Desktop Entry]")))
                {
                        fmt = GNOME_DESKTOP_ITEM_KDE;
                }

                fclose(fh);
        }

        sub_sort_order = NULL;
        switch (fmt) {
        case GNOME_DESKTOP_ITEM_KDE:
                retval = ditem_kde_load(file, subfn, flags, item_flags, &sub_sort_order);
                break;
        case GNOME_DESKTOP_ITEM_GNOME:
                retval = ditem_gnome_load(file, subfn, flags, item_flags, &sub_sort_order);
                break;
        default:
                g_warning("Unknown desktop file format %d", fmt);
                return NULL;
                break;
        }

        if(!retval)
                goto out;

        retval->location = g_strdup(g_basename(file));
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

        /* Now, read the subdirectory (if appropriate) */
        if((retval->item_flags & GNOME_DESKTOP_ITEM_IS_DIRECTORY)
           && !(flags & GNOME_DESKTOP_ITEM_LOAD_NO_SUBITEMS)) {
                int i;
                DIR *dirh;
                GnomeDesktopItemLoadFlags subflags;

                subflags = flags|GNOME_DESKTOP_ITEM_LOAD_NO_SYNC;

                if(flags & GNOME_DESKTOP_ITEM_LOAD_NO_SUBDIRS)
                        subflags |= GNOME_DESKTOP_ITEM_LOAD_NO_DIRS;

                if(sub_sort_order) {
                        for(i = 0; sub_sort_order[i]; i++) {
                                GnomeDesktopItem *subitem;

                                g_snprintf(confpath, sizeof(confpath), "%s/%s", file, sub_sort_order[i]);

                                subitem = gnome_desktop_item_new_from_file(confpath, subflags);

                                if(subitem)
                                        retval->subitems = g_slist_append(retval->subitems, subitem);
                        }
                }

                dirh = opendir(file);

                if(dirh) {
                        struct dirent *dent;

                        while((dent = readdir(dirh))) {
                                GnomeDesktopItem *subitem;

                                if(dent->d_name[0] == '.')
                                        continue;

                                if(string_in_array(dent->d_name, sub_sort_order))
                                        continue;

                                g_snprintf(confpath, sizeof(confpath), "%s/%s", file, dent->d_name);

                                subitem = gnome_desktop_item_new_from_file(confpath, subflags);

                                if(subitem)
                                        retval->subitems = g_slist_append(retval->subitems, subitem);
                        }

                        closedir(dirh);
                }
        }

 out:
        if(!(flags & GNOME_DESKTOP_ITEM_LOAD_NO_SYNC))
                gnome_config_sync();

        return retval;
}

static void
save_lang_val(gpointer key, gpointer value, gpointer user_data)
{
        char *valptr, tmpbuf[1024];

        if(!key || !strcmp(key, "C"))
                valptr = user_data;
        else
                g_snprintf((valptr = tmpbuf), sizeof(tmpbuf), "%s[%s]", (char *)user_data, (char *)key);

        gnome_config_set_string(valptr, value);
}

static void
save_key_val(gpointer key, gpointer value, gpointer user_data)
{
        gnome_config_set_string(key, value);
}

static void
ditem_save(GnomeDesktopItem *item, const char *under, GnomeDesktopItemLoadFlags save_flags,
	   const GnomeDesktopItem *parent)
{
        char fnbuf[PATH_MAX];

        gnome_config_sync();

        if(item->item_flags & GNOME_DESKTOP_ITEM_IS_DIRECTORY) {
                int my_errno;

                if(save_flags & GNOME_DESKTOP_ITEM_LOAD_NO_DIRS)
                        return;

                if(mkdir(under, 0777)
                   && ((my_errno = errno) != EEXIST)) {
                        g_warning("Couldn't create directory for desktop item: %s", g_strerror(my_errno));
                        return;
                }
        }

        if(item->item_flags & GNOME_DESKTOP_ITEM_IS_DIRECTORY) {
                FILE *fh;

                g_snprintf(fnbuf, sizeof(fnbuf), "%s/%s/.order", under, parent?item->location:"");

                if(item->subitems && (item->item_format == GNOME_DESKTOP_ITEM_GNOME))
                {
                        fh = fopen(fnbuf, "w");

                        if(fh)
                        {
                                GSList *subi;
                                for(subi = item->subitems; subi; subi = subi->next)
                                {
                                        GnomeDesktopItem *subdi;
                                        subdi = subi->data;

                                        if(!subdi->location)
                                                continue;

                                        fprintf(fh, "%s\n", subdi->location);
                                }
                        }
                } else
                        unlink(fnbuf);

                g_snprintf(fnbuf, sizeof(fnbuf), "%s/%s/.directory", under, parent?item->location:"");
                unlink(fnbuf);

                g_snprintf(fnbuf, sizeof(fnbuf), "=%s/%s/.directory=/%sDesktop Entry/", under,
                           (parent && item->location)?item->location:"",
                           (item->item_format==GNOME_DESKTOP_ITEM_KDE)?"KDE ":"");
        } else {
                g_snprintf(fnbuf, sizeof(fnbuf), "%s/%s", under, item->location);
                unlink(fnbuf);
                g_snprintf(fnbuf, sizeof(fnbuf), "=%s/%s=/%sDesktop Entry/", under, item->location,
                           (item->item_format==GNOME_DESKTOP_ITEM_KDE)?"KDE ":"");
        }

        gnome_config_push_prefix(fnbuf);

        if(item->icon_path)
                gnome_config_set_string("Icon", item->icon_path);

        if(item->exec)
                gnome_config_set_string("Exec", item->exec);

        if(item->name)
                g_hash_table_foreach(item->name, save_lang_val, "Name");
        if(item->comment)
                g_hash_table_foreach(item->comment, save_lang_val, "Comment");
        if(item->other_attributes)
                g_hash_table_foreach(item->other_attributes, save_key_val, NULL);
        if(item->item_flags & GNOME_DESKTOP_ITEM_RUN_IN_TERMINAL)
                gnome_config_set_bool("Terminal", TRUE);

        if(item->other_attributes && !g_hash_table_lookup(item->other_attributes, "Type"))
                gnome_config_set_string("Type", (item->item_flags & GNOME_DESKTOP_ITEM_IS_DIRECTORY)?"Directory":"Application");

        if((item->item_format == GNOME_DESKTOP_ITEM_KDE) && item->subitems) {
                char **myvec = g_alloca((g_slist_length(item->subitems) + 1) * sizeof(char *)), *sortorder;
                int i;
                GSList *cur;

                for(cur = item->subitems, i = 0; cur; cur = cur->next, i++) {
                        GnomeDesktopItem *subdi;
                        subdi = cur->data;

                        myvec[i] = subdi->location;
                }
                myvec[i] = NULL;

                sortorder = g_strjoinv(",", myvec);
                gnome_config_set_string("SortOrder", sortorder);
                g_free(sortorder);
        }

        gnome_config_pop_prefix();

        if(item->subitems
           && !(save_flags & GNOME_DESKTOP_ITEM_LOAD_NO_SUBITEMS)) {
                GSList *cur;
                GnomeDesktopItemLoadFlags subflags;

                subflags = save_flags;

                if(save_flags & GNOME_DESKTOP_ITEM_LOAD_NO_SUBDIRS)
                        subflags |= GNOME_DESKTOP_ITEM_LOAD_NO_DIRS;

                g_snprintf(fnbuf, sizeof(fnbuf), "%s/%s", under, item->location);

                for(cur = item->subitems; cur; cur = cur->next)
                        ditem_save(cur->data, fnbuf, subflags, item);
        }

        item->mtime = time(NULL);
}

/**
 * gnome_desktop_item_save:
 * @item: A desktop item
 * @under: The directory to save the tree underneath
 * @save_flags: Flags to be used while saving the item
 *
 * Writes the specified item to disk.
 */
/* XXX really should remove a directory before rewriting it */
void
gnome_desktop_item_save (GnomeDesktopItem *item, const char *under, GnomeDesktopItemLoadFlags save_flags)
{
        g_return_if_fail(item);
        g_return_if_fail(under);
        g_return_if_fail(item->location);

        ditem_save(item, under, save_flags, NULL);

        gnome_config_sync();
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

        g_free(item->exec);
        g_free(item->icon_path);

        if(item->name) {
                g_hash_table_freeze(item->name);
                g_hash_table_foreach_remove(item->name, ditem_free_key_value, NULL);
                g_hash_table_destroy(item->name);
        }

        if(item->comment) {
                g_hash_table_freeze(item->comment);
                g_hash_table_foreach_remove(item->comment, ditem_free_key_value, NULL);
                g_hash_table_destroy(item->comment);
        }

        if(item->other_attributes) {
                g_hash_table_freeze(item->other_attributes);
                g_hash_table_foreach_remove(item->other_attributes, ditem_free_key_value, NULL);
                g_hash_table_destroy(item->other_attributes);
        }

        g_slist_foreach(item->subitems, (GFunc)gnome_desktop_item_unref, NULL);
        g_slist_free(item->subitems);

        g_free(item);
}

/**
 * gnome_desktop_item_launch:
 * @item: A desktop item
 * @argc: An optional count of arguments to be added to the arguments defined.
 * @argv: An optional argument array, of length 'argc', to be appended
 *        to the command arguments specified in 'item'. Can be NULL.
 *
 * This function runs the program listed in the specified 'item', optionally appending additional arguments
 * to its command line.
 *
 * Returns: The value returned by gnome_execute_async() upon execution of the specified item.  */
int
gnome_desktop_item_launch (const GnomeDesktopItem *item, int argc, const char **argv)
{
        char **real_argv;
        int real_argc;
        int i, j, ret;
	char **temp_argv = NULL;
	int temp_argc = 0;

        g_return_val_if_fail(item, -1);
        g_return_val_if_fail(item->exec, -1);


        if(!argv)
                argc = 0;

	/* FIXME: I think this should use a shell to execute instead
	   of trying to pretend we can split strings into vectors
	   sanely */
	gnome_config_make_vector(item->exec, &temp_argc, &temp_argv);

        real_argc = argc + temp_argc;
        real_argv = g_alloca((real_argc + 1) * sizeof(char *));

        for(i = 0; i < temp_argc; i++)
                real_argv[i] = temp_argv[i];

        for(j = 0; j < argc; j++, i++)
                real_argv[i] = (char *)argv[j];

        ret = gnome_execute_async(NULL, real_argc, real_argv);

	g_strfreev(temp_argv);

	return ret;
}

/**
 * gnome_desktop_item_exists:
 * @item: A desktop item
 *
 * Returns: Whether the program specified by 'item' is available to run on the system.
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

	/* FIXME: since I think the exec should be a shell expression
	   this is bad, it is also bad since even if we don't make it a shell
	   expression, when people would then want to do that they would do
	   /bin/sh -c 'blah'
	   Now even if blah doesn't exist we're gonna thing it does */
        if(item->exec) {
		char **temp_argv = NULL;
		int temp_argc = 0;
		gboolean ret = FALSE;

		gnome_config_make_vector(item->exec, &temp_argc, &temp_argv);

                if(temp_argv[0][0] == PATH_SEP)
                        ret = g_file_exists(temp_argv[0]);
                else {
                        char *tryme = gnome_is_program_in_path(temp_argv[0]);
                        if(tryme) {
                                g_free(tryme);
                                ret = TRUE;
                        }
                        else
                                ret = FALSE;
                }
		g_strfreev(temp_argv);
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
 * gnome_desktop_item_get_command:
 * @item: A desktop item
 *
 * Returns: The command associated with the specified 'item'. The returned memory remains owned by the GnomeDesktopItem
 *          and should not be freed.
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
 * Returns: The icon filename associated with the specified 'item'. The returned memory remains owned by the GnomeDesktopItem
 *          and should not be freed.
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
		GList *iter;

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
		GList *iter;

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
 * @attr_name: The language translation for which the comment should be returned.
 *
 * Returns: The value of the attribute 'attr_name' on the 'item'. The
 *          returned memory remains owned by the GnomeDesktopItem and
 *          should * not be freed.
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
 * gnome_desktop_item_get_subitems:
 * @item: A desktop item
 *
 * Returns: A GSList of the items contained under 'item'. The returned value should not be freed.
 *
 */
const GSList *
gnome_desktop_item_get_subitems (const GnomeDesktopItem *item)
{
        g_return_val_if_fail(item, NULL);

        return item->subitems;
}

static void
ditem_add_key(gpointer key, gpointer value, gpointer user_data)
{
        GSList **list = user_data;

        *list = g_slist_prepend(*list, key);
}

/**
 * gnome_desktop_item_get_languages:
 * @item: A desktop item
 *
 * Returns: A GSList of the language name strings for the languages
 * 'item'. The returned value should be freed, but its contents should
 * not be.
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

        return retval;
}

/**
 * gnome_desktop_item_get_attributes:
 * @item: A desktop item
 *
 * Returns: A GSList of the attribute name strings for the attributes of
 * 'item'. The returned value should be freed, but its contents should
 * not be.
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
 * @under: The path that the item's location is relative to
 *
 * This function checks the modification time of the on-disk file to
 * see if it is more recent than the in-memory data.
 *
 * Returns: An enum value that specifies whether the item has changed since being loaded.
 */
GnomeDesktopItemStatus
gnome_desktop_item_get_file_status (GnomeDesktopItem *item, const char *under)
{
        char fnbuf[PATH_MAX];
        struct stat sbuf;
        GnomeDesktopItemStatus retval;

        g_return_val_if_fail(item, GNOME_DESKTOP_ITEM_DISAPPEARED);
        g_return_val_if_fail(item->location, GNOME_DESKTOP_ITEM_DISAPPEARED);

        g_snprintf(fnbuf, sizeof(fnbuf), "%s/%s", under, item->location);

        if(stat(fnbuf, &sbuf))
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
 * gnome_desktop_item_set_name:
 * @item: A desktop item
 * @language: The language for which to set the item name
 * @name: The item's name in the specified 'language'
 *
 */
void
gnome_desktop_item_set_name (GnomeDesktopItem *item, const char *language, const char *name)
{
        gpointer old_val = NULL, old_key = NULL;

        g_return_if_fail(item);

        if(!item->name)
                item->name = g_hash_table_new(g_str_hash, g_str_equal);
        else {
                g_hash_table_lookup_extended(item->name, language, &old_key, &old_val);
        }

        g_hash_table_insert(item->name, g_strdup(language), g_strdup(name));

        g_free(old_key);
        g_free(old_val);
}

/**
 * gnome_desktop_item_set_command:
 * @item: A desktop item
 * @command: A string with the command
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
 * gnome_desktop_item_set_comment:
 * @item: A desktop item
 * @language: The language for which to set the item comment
 * @comment: The item's comment in the specified 'language'
 *
 */
void
gnome_desktop_item_set_comment (GnomeDesktopItem *item, const char *language, const char *comment)
{
        gpointer old_val = NULL, old_key = NULL;
        g_return_if_fail(item);

        if(!item->comment)
                item->comment = g_hash_table_new(g_str_hash, g_str_equal);
        else {
                g_hash_table_lookup_extended(item->comment, language, &old_key, &old_val);
        }

        g_hash_table_insert(item->comment, g_strdup(language), g_strdup(comment));

        g_free(old_key);
        g_free(old_val);
}

/**
 * gnome_desktop_item_set_attribute:
 * @item: A desktop item
 * @attr_name: The name of the attribute
 * @attr_value: The value of the attribute
 *
 */
void
gnome_desktop_item_set_attribute (GnomeDesktopItem *item, const char *attr_name, const char *attr_value)
{
        gpointer old_val = NULL, old_key = NULL;
        g_return_if_fail(item);

        if(!item->other_attributes)
                item->other_attributes = g_hash_table_new(g_str_hash, g_str_equal);
        else {
                g_hash_table_lookup_extended(item->other_attributes, attr_name, &old_key, &old_val);
        }

        g_hash_table_insert(item->other_attributes, g_strdup(attr_name), g_strdup(attr_value));

        g_free(old_key);
        g_free(old_val);
}

/**
 * gnome_desktop_item_set_subitems:
 * @item: A desktop item for a directory
 * @subitems: A GSList of the items to be listed as children of this item. Ownership the 'subitems' list is transferred
 *            from the application to the 'item'.
 */
void
gnome_desktop_item_set_subitems (GnomeDesktopItem *item, GSList *subitems)
{
        g_return_if_fail(item);
        g_return_if_fail(item->item_flags & GNOME_DESKTOP_ITEM_IS_DIRECTORY);

        g_slist_foreach(subitems, (GFunc)gnome_desktop_item_ref, NULL);
        g_slist_foreach(item->subitems, (GFunc)gnome_desktop_item_unref, NULL);
        g_slist_free(item->subitems);
        item->subitems = subitems;
}

/**
 * gnome_desktop_item_set_flags:
 * @item: A desktop item
 * @flags: The flags to be set
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

        item->item_format = fmt;
}

/**
 * gnome_desktop_item_set_location:
 * @item: A desktop item
 * @location: A string specifying the file location of this particular item. Subitems of the specified item are not affected.
 *
 */
void
gnome_desktop_item_set_location (GnomeDesktopItem *item, const char *location)
{
        g_free(item->location);
        item->location = g_strdup(g_basename(location));
}
