/* -*- Mode: C; c-set-style: linux indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gnome-desktop-item.c - GNOME Desktop File Representation 

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
#include <gnome-desktop/gnome-desktop-item.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnome/gnome-exec.h>
#include <libgnome/gnome-url.h>
#include <locale.h>
#include <popt.h>

#include <bonobo/bonobo-property-bag-client.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo/bonobo-exception.h>

#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs-uri.h>

#include "bonobo-config-ditem.h"
#include "gnome-desktop-i18n.h"

#define PATH_SEP '/'
#define PATH_SEP_STR "/"

struct _GnomeDesktopItem {
        int refcount;

	Bonobo_ConfigDatabase db;
	GNOME_Desktop_Entry *entry;
	GHashTable *name, *comment, *attributes;

	/* `modified' means that the ditem has been modified since the last save. */
	gboolean modified;

	/* `dirty' means that the `entry' has changed and thus the hash tables
	 * must be updated. */
	gboolean dirty;

        char *location;

        time_t mtime;

        GnomeDesktopItemFlags item_flags;
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

/**
 * gnome_desktop_item_copy:
 * @item: The item to be copied
 *
 * Creates a copy of a GnomeDesktopItem.  The new copy has a refcount of 1.
 *
 * Returns: The new copy 
 */
GnomeDesktopItem *
gnome_desktop_item_copy (GnomeDesktopItem *item)
{
        GnomeDesktopItem *retval;

        g_return_val_if_fail (item, NULL);

        retval = gnome_desktop_item_new ();

        retval->item_flags = item->item_flags;

	/* g_strdup handles NULLs correctly */
        retval->location = g_strdup (item->location);

        return retval;
}

static void
ditem_update (GnomeDesktopItem *ditem)
{
	gulong i;

	if (!ditem->dirty)
		return;

	if (ditem->name)
		g_hash_table_destroy (ditem->name);
	ditem->name = g_hash_table_new_full (g_str_hash, g_str_equal,
					     (GDestroyNotify) g_free,
					     (GDestroyNotify) NULL);

	if (ditem->comment)
		g_hash_table_destroy (ditem->comment);
	ditem->comment = g_hash_table_new_full (g_str_hash, g_str_equal,
						(GDestroyNotify) g_free,
						(GDestroyNotify) NULL);

	if (ditem->attributes)
		g_hash_table_destroy (ditem->attributes);
	ditem->attributes = g_hash_table_new_full (g_str_hash, g_str_equal,
						   (GDestroyNotify) g_free,
						   (GDestroyNotify) NULL);

	for (i = 0; i < ditem->entry->Name._length; i++) {
		GNOME_LocalizedString *current = &ditem->entry->Name._buffer [i];

		g_hash_table_insert (ditem->name, g_strdup (current->locale), current);
	}

	for (i = 0; i < ditem->entry->Comment._length; i++) {
		GNOME_LocalizedString *current = &ditem->entry->Comment._buffer [i];

		g_hash_table_insert (ditem->comment, g_strdup (current->locale), current);
	}

	for (i = 0; i < ditem->entry->Attributes._length; i++) {
		Bonobo_Pair *current = &ditem->entry->Attributes._buffer [i];

		g_hash_table_insert (ditem->comment, g_strdup (current->name), current);
	}

	ditem->dirty = FALSE;
}

static GnomeDesktopItem *
ditem_load (const char *data_file,
	    GnomeDesktopItemLoadFlags flags,
	    GnomeDesktopItemFlags item_flags)
{
        GnomeDesktopItem *retval;
	Bonobo_ConfigDatabase db;
	CORBA_Environment ev;
	CORBA_any *any;

	db = bonobo_config_ditem_new (data_file);
	if (db == CORBA_OBJECT_NIL)
		return NULL;

	CORBA_exception_init (&ev);
	any = bonobo_pbclient_get_value (db, "/Desktop Entry", TC_GNOME_Desktop_Entry, &ev);
	if (BONOBO_EX (&ev)) {
		g_warning (G_STRLOC " (%s): %s", data_file, bonobo_exception_get_text (&ev));
		CORBA_exception_free (&ev);
		return NULL;
	}
	CORBA_exception_free (&ev);

	retval = gnome_desktop_item_new ();
	retval->db = db;
	retval->entry = any->_value;
	retval->dirty = TRUE;

        retval->item_flags = item_flags;

	ditem_update (retval);

	if ((flags & GNOME_DESKTOP_ITEM_LOAD_ONLY_IF_EXISTS) && (retval->entry->TryExec[0] != '\0')) {
		char *tryme = gnome_is_program_in_path (retval->entry->TryExec);
		if (tryme == NULL)
			goto errout;
		g_free (tryme);
	}

	if (retval->entry->Terminal)
		retval->item_flags |= GNOME_DESKTOP_ITEM_RUN_IN_TERMINAL;
	else
		retval->item_flags &= (~GNOME_DESKTOP_ITEM_RUN_IN_TERMINAL);

        return retval;

 errout:
        gnome_desktop_item_unref (retval);
        return NULL;
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
gnome_desktop_item_new_from_file (const char *file,
				  GnomeDesktopItemLoadFlags flags)
{
        GnomeDesktopItem *retval;
        char subfn[PATH_MAX];
        GnomeDesktopItemFlags item_flags;
        struct stat sbuf;

        g_return_val_if_fail (file, NULL);

#ifdef DI_DEBUG
        /*  g_print("Loading file %s\n", file); */
#endif

        if (stat(file, &sbuf) != 0)
                return NULL;

        /* Step one - figure out what type of file this is */
        flags = 0;
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

	retval = ditem_load (subfn, flags, item_flags);

        if (retval == NULL)
                goto out;

	/* make the 'Type' partly sane */
        if(item_flags & GNOME_DESKTOP_ITEM_IS_DIRECTORY) {
		/* perhaps a little ineficent way to ensure this
		   is the correct type */
		retval->entry->Type = GNOME_Desktop_ENTRY_TYPE_DIRECTORY;
		retval->modified = TRUE;
	} else {
		/* this is not a directory so sanitize it by killing the
		   type altogether if type happens to be "Directory" */
		if(retval->entry->Type == GNOME_Desktop_ENTRY_TYPE_DIRECTORY) {
			retval->entry->Type = GNOME_Desktop_ENTRY_TYPE_UNKNOWN;
			retval->modified = TRUE;
		}
	}

	if(subfn[0] == '/')
		retval->location = g_strdup(subfn);
	else {
		char *curdir = g_get_current_dir();
		retval->location = g_concat_dir_and_file(curdir,subfn);
		g_free(curdir);
	}
        retval->mtime = sbuf.st_mtime;

 out:
        return retval;
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
	CORBA_Environment ev;
	BonoboArg *arg;

        g_return_val_if_fail (item, FALSE);
        g_return_val_if_fail (item->location, FALSE);

	/* first we setup the new location if we need to */
	if (under) {
		Bonobo_ConfigDatabase db;

		g_free (item->location);
		item->location = g_strdup (under);
		item->modified = TRUE;

		db = bonobo_config_ditem_new (item->location);
		if (db == CORBA_OBJECT_NIL)
			return FALSE;

		bonobo_object_release_unref (item->db, NULL);
		item->db = db;
	}

	if (!item->modified)
		return TRUE;

	CORBA_exception_init (&ev);
	arg = bonobo_arg_new (TC_GNOME_Desktop_Entry);
	arg->_value = item->entry;
	arg->_release = FALSE;
	bonobo_pbclient_set_value (item->db, "/Desktop Entry", arg, &ev);
	if (BONOBO_EX (&ev)) {
		g_warning (G_STRLOC ": save failed: %s", bonobo_exception_get_text (&ev));
		bonobo_arg_release (arg);
		CORBA_exception_free (&ev);
		return FALSE;
	}

	bonobo_arg_release (arg);
	CORBA_exception_free (&ev);

	CORBA_exception_init (&ev);
	Bonobo_ConfigDatabase_sync (item->db, &ev);
	if (BONOBO_EX (&ev)) {
		g_warning (G_STRLOC ": save failed: %s", bonobo_exception_get_text (&ev));
		CORBA_exception_free (&ev);
		return FALSE;
	}
	CORBA_exception_free (&ev);

	item->modified = FALSE;
        item->mtime = time(NULL);

	return TRUE;
}

/**
 * gnome_desktop_item_ref:
 * @item: A desktop item
 *
 * Description: Increases the reference count of the specified item.
 *
 * Returns: the newly referenced @item
 */
GnomeDesktopItem *
gnome_desktop_item_ref (GnomeDesktopItem *item)
{
        g_return_val_if_fail (item != NULL, NULL);

        item->refcount++;

	return item;
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
        g_return_if_fail (item != NULL);

        item->refcount--;

        if(item->refcount != 0)
                return;

        g_free(item->location);

	/* just for sanity */
	memset (item, 0, sizeof (GnomeDesktopItem));

        g_free (item);
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
ditem_execute(GnomeDesktopItem *item, int appargc, const char *appargv[], int argc, const char *argv[], gboolean replacekdefiles)
{
        char **real_argv;
        int real_argc;
        int i, j, ret;
	char **term_argv = NULL;
	int term_argc = 0;
	GSList *tofree = NULL;

        g_return_val_if_fail (item, -1);

        if(!argv)
                argc = 0;


	/* check the type, if there is one set */
	if(item->entry->Type != GNOME_Desktop_ENTRY_TYPE_APPLICATION) {
		/* FIXME: ue GError */
		/* we are not an application so just exit
		   with an error */
		g_warning(_("Attempting to launch a non-application"));
		return -1;
	}

	if(item->item_flags & GNOME_DESKTOP_ITEM_RUN_IN_TERMINAL) {
		/* FIXME: add temrinal options from desktop file */
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

	/* FIXME: this ought to be standards compliant */
	/* replace the standard %? thingies */
	if(replacekdefiles) {
		replace_percentsign(real_argc, real_argv, "%f", NULL, &tofree, TRUE);
		replace_percentsign(real_argc, real_argv, "%F", NULL, &tofree, TRUE);
		replace_percentsign(real_argc, real_argv, "%u", NULL, &tofree, TRUE);
	}
	replace_percentsign(real_argc, real_argv, "%d", g_getenv("PWD"), &tofree, FALSE);
#if 0
	replace_percentsign(real_argc, real_argv, "%m", 
			    gnome_desktop_item_get_attribute(item,"MiniIcon"), &tofree, FALSE);
#endif
	replace_percentsign(real_argc, real_argv, "%c", 
			    gnome_desktop_item_get_local_comment(item), &tofree, FALSE);

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
gnome_desktop_item_launch (GnomeDesktopItem *item, int argc, const char **argv)
{
	const char **temp_argv = NULL;
	int temp_argc = 0;
	char *the_exec;
	int ret;

        g_return_val_if_fail (item, -1);

	/* This is a URL, so launch it as a url */
	if (item->entry->Type == GNOME_Desktop_ENTRY_TYPE_URL) {
		if (item->entry->URL[0] != '\0') {
			if (gnome_url_show (item->entry->URL))
				return 0;
			else
				return -1;
		/* Gnome panel used to put this in Exec */
		} else if (item->entry->Exec[0] != '\0') {
			if (gnome_url_show (item->entry->Exec))
				return 0;
			else
				return -1;
		} else {
			return -1;
		}
		/* FIXME: use GError */
	}

	if(item->entry->Exec[0] == '\0') {
		/* FIXME: use GError */
		g_warning(_("Trying to execute an item with no 'Exec'"));
		return -1;
	}


	/* make a new copy and get rid of spaces */
	the_exec = g_alloca(strlen(item->entry->Exec)+1);
	strcpy(the_exec,item->entry->Exec);

	if(!strip_the_amp(the_exec))
		return -1;

	/* we use a popt function as it does exactly what we want to do and
	   gnome already uses popt */
	if(poptParseArgvString(item->entry->Exec, &temp_argc, &temp_argv) != 0) {
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
static int G_GNUC_UNUSED
ditem_exec_single_file (GnomeDesktopItem *item, const char *exec, char *file)
{
	const char **temp_argv = NULL;
	int temp_argc = 0;
	char *the_exec;
	GSList *tofree = NULL;
	int ret;

        g_return_val_if_fail (item, -1);
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
static int G_GNUC_UNUSED
ditem_exec_multiple_files (GnomeDesktopItem *item, const char *exec,
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

        g_return_val_if_fail (item, -1);
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
gnome_desktop_item_drop_uri_list (GnomeDesktopItem *item,
				  GList *uri_list)
{
	GList *li;
	gboolean G_GNUC_UNUSED any_urls = FALSE;
	GList *file_list = NULL;
	int ret = -1;

	g_return_val_if_fail (item != NULL, -1);
	g_return_val_if_fail (uri_list != NULL, -1);

	/* How could you drop something on a URL entry, that would
	 * be bollocks */
	if (item->entry->Type == GNOME_Desktop_ENTRY_TYPE_URL) {
		/*FIXME: use a GError */
		return -1;
	}

	/* FIXME: we should really test if this was a compliant entry and there
	 * were no %f %u or %F or whatnot, we shouldn't allow a drop!,
	 * in that case I suppose, we should do that during load and add a
	 * %F or something */

#if 0
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
#endif

	/* figure out if we have any urls in the dropped files,
	   this also builds a list of all the files */
	for(li=uri_list; li; li=li->next) {
		file_list = g_list_prepend(file_list, g_strdup (li->data));
	}
	if(file_list)
		file_list = g_list_reverse(file_list);

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
gnome_desktop_item_exists (GnomeDesktopItem *item)
{
        g_return_val_if_fail (item, FALSE);

	if(item->entry->TryExec[0] != '\0') {
                char *tryme;

		tryme = gnome_is_program_in_path(item->entry->TryExec);
		if(tryme) {
			g_free(tryme);
			return TRUE;
		}
		else
			return FALSE;
	}

        if(item->entry->Exec) {
		const char **temp_argv = NULL;
		int temp_argc = 0;
		gboolean ret = FALSE;

		/* we use a popt function as it does exactly what we want to
		   do and gnome already uses popt */
		if(poptParseArgvString(item->entry->Exec,
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
gnome_desktop_item_get_flags (GnomeDesktopItem *item)
{
        g_return_val_if_fail (item, 0);

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
GNOME_Desktop_EntryType
gnome_desktop_item_get_type (GnomeDesktopItem *item)
{
        g_return_val_if_fail (item, GNOME_Desktop_ENTRY_TYPE_UNKNOWN);

        return item->entry->Type;
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
 * Returns: The command associated with the specified 'item'.
 */
gchar *
gnome_desktop_item_get_command (GnomeDesktopItem *item)
{
        g_return_val_if_fail (item, NULL);

        return g_strdup (item->entry->Exec);
}

/**
 * gnome_desktop_item_get_command:
 * @item: A desktop item
 *
 * Returns: The TryExec field associated with the specified 'item'.
 */
gchar *
gnome_desktop_item_get_tryexec (GnomeDesktopItem *item)
{
        g_return_val_if_fail (item, NULL);

	if (item->entry->TryExec [0] == '\0')
		return NULL;
	else
		return g_strdup (item->entry->TryExec);
}

/**
 * gnome_desktop_item_get_icon_path:
 * @item: A desktop item
 *
 * Returns: The icon filename associated with the specified 'item'.
 */
gchar *
gnome_desktop_item_get_icon_path (GnomeDesktopItem *item)
{
        g_return_val_if_fail (item, NULL);

        return g_strdup (item->entry->Icon);
}

static void
insert_localized_string (GNOME_LocalizedStringList *list, const gchar *language, const gchar *text)
{
	GNOME_LocalizedString *cur_val, *last_val;
	gulong length, i;

	if (!language) language = "C";

	length = list->_length;
	/* This is how much has been allocated for the list. */
	if (list->_maximum == 0)
		list->_maximum = length;

	if (!text)
		return;

	length++;
	if (length >= list->_maximum) {
		GNOME_LocalizedString *new_list;

		new_list = CORBA_sequence_GNOME_LocalizedString_allocbuf (length);
		for (i = 0; i < list->_length; i++) {
			GNOME_LocalizedString *c = &list->_buffer [i];

			new_list [i].locale = CORBA_string_dup (c->locale);
			new_list [i].text = CORBA_string_dup (c->text);
		}

		if (list->_release)
			CORBA_free (list->_buffer);

		list->_buffer = new_list;
		list->_length = list->_maximum = length;
		list->_release = TRUE;
	}

	cur_val = &list->_buffer [length-1];
	cur_val->locale = CORBA_string_dup (language);
	cur_val->text = CORBA_string_dup (text);
}

static void
insert_attribute (Bonobo_PropertySet *list, const gchar *name, const BonoboArg *value)
{
	Bonobo_Pair *cur_val, *last_val;
	gulong length, i;

	length = list->_length;
	/* This is how much has been allocated for the list. */
	if (list->_maximum == 0)
		list->_maximum = length;

	if (!value)
		return;

	length++;
	if (length >= list->_maximum) {
		Bonobo_Pair *new_list;

		new_list = CORBA_sequence_Bonobo_Pair_allocbuf (length);
		for (i = 0; i < list->_length; i++) {
			Bonobo_Pair *c = &list->_buffer [i];

			new_list [i].name = CORBA_string_dup (c->name);
			CORBA_any__copy (&new_list [i].value, &c->value);
		}

		if (list->_release)
			CORBA_free (list->_buffer);

		list->_buffer = new_list;
		list->_length = list->_maximum = length;
		list->_release = TRUE;
	}

	cur_val = &list->_buffer [length-1];
	cur_val->name = CORBA_string_dup (name);
	CORBA_any__copy (&cur_val->value, value);
}


/**
 * gnome_desktop_item_get_name:
 * @item: A desktop item
 * @language: The language translation for which the name should be returned.
 *            If NULL is passed, it defaults to "C".
 *
 * Returns: The human-readable name for the specified 'item', in the
 *          specified 'language'. The returned memory remains owned by
 *          the GnomeDesktopItem and should not be freed.
 * */
gchar *
gnome_desktop_item_get_name (GnomeDesktopItem *item, const char *language)
{
	GNOME_LocalizedString *string;

        g_return_val_if_fail (item, NULL);

	ditem_update (item);

	string = g_hash_table_lookup (item->name, language ? language : "C");
	if (string)
		return g_strdup (string->text);

	return NULL;
}

/**
 * gnome_desktop_item_get_comment:
 * @item: A desktop item
 * @language: The language translation for which the comment should be returned.
 *            If NULL is passed, it defaults to "C".
 *
 * Returns: The human-readable comment for the specified 'item', in the
 *          specified 'language'.
 *
 */
gchar *
gnome_desktop_item_get_comment (GnomeDesktopItem *item, const char *language)
{
	GNOME_LocalizedString *string;

        g_return_val_if_fail (item, NULL);

	ditem_update (item);

	string = g_hash_table_lookup (item->comment, language ? language : "C");
	if (string)
		return g_strdup (string->text);

	return NULL;
}

/**
 * gnome_desktop_item_get_local_name:
 * @item: A desktop item
 *
 * Returns: The human-readable name for the specified @item, in
 *          the user's preferred locale or a fallback if
 *          necessary. If no name is set in any relevant locale,
 *          NULL is returned.
 * */

gchar *
gnome_desktop_item_get_local_name (GnomeDesktopItem *item)
{
	gchar *name, *p;
	GList *list, *c;

	g_return_val_if_fail (item, NULL);

	list = gnome_desktop_i18n_get_language_list ("LC_MESSAGES");

	for (c = list; c; c = c->next) {
		const gchar *locale = c->data;

		name = gnome_desktop_item_get_name (item, locale);
		if (name) {
			g_list_free (list);
			return name;
		}
	}

	return NULL;
}

/**
 * gnome_desktop_item_get_local_comment:
 * @item: A desktop item
 *
 * Returns: The human-readable comment for the specified @item, in
 *          the user's preferred locale or a fallback if
 *          necessary. If no comment exists then NULL is returned.
 * */
gchar *
gnome_desktop_item_get_local_comment (GnomeDesktopItem *item)
{
	gchar *comment;
	GList *list, *c;

	g_return_val_if_fail (item, NULL);

	list = gnome_desktop_i18n_get_language_list ("LC_MESSAGES");

	for (c = list; c; c = c->next) {
		const gchar *locale = c->data;

		comment = gnome_desktop_item_get_comment (item, locale);
		if (comment) {
			g_list_free (list);
			return comment;
		}
	}

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
BonoboArg *
gnome_desktop_item_get_attribute (GnomeDesktopItem *item, const char *attr_name)
{
	Bonobo_Pair *pair;

        g_return_val_if_fail (item, NULL);
        g_return_val_if_fail (attr_name, NULL);

	ditem_update (item);

	pair = g_hash_table_lookup (item->attributes, attr_name);
	if (pair)
		return bonobo_arg_copy (&pair->value);

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
 * order.
 *
 */
GSList *
gnome_desktop_item_get_order (GnomeDesktopItem *item)
{
	gulong i;
	GSList *retval = NULL;

        g_return_val_if_fail (item, NULL);

	for (i = 0; i < item->entry->SortOrder._length; i++) {
		const gchar *text = item->entry->SortOrder._buffer [i];

		retval = g_slist_prepend (retval, g_strdup (text));
	}

        return g_slist_reverse (retval);
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
gnome_desktop_item_get_languages(GnomeDesktopItem *item)
{
        GSList *retval = NULL;
	gulong i;

        g_return_val_if_fail (item, NULL);

	for (i = 0; i < item->entry->Name._length; i++) {
		GNOME_LocalizedString *current;

		current = &item->entry->Name._buffer [i];
		if (!current->locale)
			continue;

		ditem_add_key (current->locale, NULL, &retval);
	}

	for (i = 0; i < item->entry->Comment._length; i++) {
		GNOME_LocalizedString *current;

		current = &item->entry->Comment._buffer [i];
		if (!current->locale)
			continue;

		ditem_add_key_test (current->locale, NULL, &retval);
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
 * should be freed with g_slist_free.
 *
 */
GSList *
gnome_desktop_item_get_attributes (GnomeDesktopItem *item)
{
        GSList *retval = NULL;
	gulong i;

        g_return_val_if_fail (item, NULL);

	for (i = 0; i < item->entry->Attributes._length; i++) {
		Bonobo_Pair *pair = &item->entry->Attributes._buffer [i];

		retval = g_slist_prepend (retval, g_strdup (pair->name));
	}

        return retval;
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

        g_return_val_if_fail (item, GNOME_DESKTOP_ITEM_DISAPPEARED);
        g_return_val_if_fail (item->location, GNOME_DESKTOP_ITEM_DISAPPEARED);

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
 * Returns: The file location associated with 'item'.
 *
 */
char *
gnome_desktop_item_get_location (GnomeDesktopItem *item)
{
        return g_strdup (item->location);
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
	if (item->entry->Name._release)
		CORBA_free (item->entry->Name._buffer);
	item->entry->Name._buffer = NULL;
	item->entry->Name._release = FALSE;
	item->entry->Name._length = 0;
	item->entry->Name._maximum = 0;
	item->modified = TRUE;
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
	GNOME_LocalizedString *string;

	ditem_update (item);
	item->modified = TRUE;

	string = g_hash_table_lookup (item->name, language ? language : "C");
	if (string) {
		CORBA_free (string->text);
		string->text = CORBA_string_dup (name);
		return;
	}

	insert_localized_string (&item->entry->Name, language, name);
	item->dirty = TRUE;
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
gnome_desktop_item_set_type (GnomeDesktopItem *item, GNOME_Desktop_EntryType type)
{
        g_return_if_fail (item);

	item->entry->Type = type;
	item->modified = TRUE;
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
        g_return_if_fail (item);

	CORBA_free(item->entry->Exec);
	item->entry->Exec = CORBA_string_dup(command);
	item->modified = TRUE;
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
        g_return_if_fail (item);

	CORBA_free (item->entry->Icon);
	item->entry->Icon = CORBA_string_dup (icon_path);
	item->modified = TRUE;
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
	if (item->entry->Comment._release)
		CORBA_free (item->entry->Comment._buffer);
	item->entry->Comment._buffer = NULL;
	item->entry->Comment._release = FALSE;
	item->entry->Comment._length = 0;
	item->entry->Comment._maximum = 0;
	item->modified = TRUE;
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
	GNOME_LocalizedString *string;

	ditem_update (item);
	item->modified = TRUE;

	string = g_hash_table_lookup (item->comment, language ? language : "C");
	if (string) {
		CORBA_free (string->text);
		string->text = CORBA_string_dup (comment);
		return;
	}

	insert_localized_string (&item->entry->Comment, language, comment);
	item->dirty = TRUE;
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
				  const BonoboArg *attr_value)
{
	Bonobo_Pair *this;

	ditem_update (item);
	item->modified = TRUE;

	this = g_hash_table_lookup (item->attributes, attr_name);
	if (this) {
		CORBA_any__copy (&this->value, attr_value);
		return;
	}

	insert_attribute (&item->entry->Attributes, attr_name, attr_value);
	item->dirty = TRUE;
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
	gulong length, i;

        g_return_if_fail (item);
        g_return_if_fail (item->item_flags & GNOME_DESKTOP_ITEM_IS_DIRECTORY);

	length = g_slist_length (order);
	if (item->entry->SortOrder._release)
		CORBA_free (item->entry->SortOrder._buffer);
	item->entry->SortOrder._buffer = CORBA_sequence_CORBA_string_allocbuf (length);
	item->entry->SortOrder._length = length;
	item->entry->SortOrder._maximum = length;
	item->entry->SortOrder._release = TRUE;

	i = 0;
	for(li = order; li; li = li->next) {
		item->entry->SortOrder._buffer [i] = CORBA_string_dup (li->data);
		i++;
	}
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
        g_return_if_fail (item);

        item->item_flags = flags;
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
