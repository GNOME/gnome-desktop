/* -*- Mode: C; c-set-style: linux indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gnome-desktop-item.c - GNOME Desktop File Representation 

   Copyright (C) 1999, 2000 Red Hat Inc.
   Copyright (C) 2001 Sid Vicious
   All rights reserved.

   This file is part of the Gnome Library.

   Developed by Elliot Lee <sopwith@redhat.com> and Sid Vicious
   
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
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnome/gnome-exec.h>
#include <libgnome/gnome-url.h>
#include <locale.h>
#include <popt.h>

#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include "gnome-desktop-item.h"

struct _GnomeDesktopItem {
        int refcount;

	/* all languages used */
	GList *languages;

	GnomeDesktopItemType type;
	
	/* `modified' means that the ditem has been
	 * modified since the last save. */
	gboolean modified;

	/* Keys of the main section only */
	GList *keys;

	GList *sections;

	/* This includes ALL keys, including
	 * other sections, separated by '/' */
	GHashTable *main_hash;

        char *location;

        time_t mtime;
};

/* If mtime is set to this, set_location won't update mtime,
 * this is to be used internally only. */
#define DONT_UPDATE_MTIME ((time_t)-2)

typedef struct {
	char *name;
	GList *keys;
} Section;

typedef enum {
	ENCODING_UNKNOWN,
	ENCODING_UTF8,
	ENCODING_LEGACY_MIXED
} Encoding;

static GnomeDesktopItem * ditem_load (const char *uri,
				      GError **error);
static gboolean ditem_save (GnomeDesktopItem *item,
			    const char *uri,
			    GError **error);

/*
 * GnomeVFS reading utils, that look like the libc buffered io stuff
 */
typedef struct {
	GnomeVFSHandle *handle;
	char *uri;
	gboolean eof;
	char buf[BUFSIZ];
	gsize size;
	gsize pos;
} ReadBuf;

static int
readbuf_getc (ReadBuf *rb)
{
	if (rb->eof)
		return EOF;

	if (rb->size == 0 ||
	    rb->pos == rb->size) {
		GnomeVFSFileSize bytes_read;
		/* FIXME: handle other errors */
		if (gnome_vfs_read (rb->handle,
				    rb->buf,
				    BUFSIZ,
				    &bytes_read) != GNOME_VFS_OK) {
			rb->eof = TRUE;
			return EOF;
		}
		rb->size = bytes_read;
		rb->pos = 0;

		if (rb->size == 0) {
			rb->eof = TRUE;
			return EOF;
		}
	}

	return (int)rb->buf[rb->pos++];
}

/* Note, does not include the trailing \n */
static char *
readbuf_gets (char *buf, gsize bufsize, ReadBuf *rb)
{
	int c;
	gsize pos;

	g_return_val_if_fail (rb != NULL, NULL);

	pos = 0;
	buf[0] = '\0';

	do {
		c = readbuf_getc (rb);
		if (c == EOF ||
		    c == '\n')
			break;
		buf[pos++] = c;
	} while (pos < bufsize-1);

	if (c == EOF &&
	    pos == 0)
		return NULL;

	buf[pos++] = '\0';

	return buf;
}

static ReadBuf *
readbuf_open (const char *uri)
{
	GnomeVFSHandle *handle;
	ReadBuf *rb;

	g_return_val_if_fail (uri != NULL, NULL);

	if (gnome_vfs_open (&handle, uri,
			    GNOME_VFS_OPEN_READ) != GNOME_VFS_OK)
		return NULL;

	rb = g_new0 (ReadBuf, 1);
	rb->handle = handle;
	rb->uri = g_strdup (uri);
	rb->eof = FALSE;
	rb->size = 0;
	rb->pos = 0;

	return rb;
}

static gboolean
readbuf_rewind (ReadBuf *rb)
{
	if (gnome_vfs_seek (rb->handle,
			    GNOME_VFS_SEEK_START, 0) == GNOME_VFS_OK)
		return TRUE;

	gnome_vfs_close (rb->handle);
	rb->handle = NULL;
	if (gnome_vfs_open (&rb->handle, rb->uri,
			    GNOME_VFS_OPEN_READ) == GNOME_VFS_OK)
		return TRUE;

	return FALSE;
}

static void
readbuf_close (ReadBuf *rb)
{
	if (rb->handle != NULL)
		gnome_vfs_close (rb->handle);
	rb->handle = NULL;
	g_free (rb->uri);
	rb->uri = NULL;

	g_free (rb);
}


static GnomeDesktopItemType G_GNUC_CONST
type_from_string (const char *type)
{
	if (type == NULL) {
		return GNOME_DESKTOP_ITEM_TYPE_NULL;
	} else if (strcmp (type, "Application") == 0) {
		return GNOME_DESKTOP_ITEM_TYPE_APPLICATION;
	} else if (strcmp (type, "Link") == 0) {
		return GNOME_DESKTOP_ITEM_TYPE_LINK;
	} else if (strcmp (type, "FSDevice") == 0) {
		return GNOME_DESKTOP_ITEM_TYPE_FSDEVICE;
	} else if (strcmp (type, "MimeType") == 0) {
		return GNOME_DESKTOP_ITEM_TYPE_MIME_TYPE;
	} else if (strcmp (type, "Directory") == 0) {
		return GNOME_DESKTOP_ITEM_TYPE_DIRECTORY;
	} else if (strcmp (type, "Service") == 0) {
		return GNOME_DESKTOP_ITEM_TYPE_SERVICE;
	} else if (strcmp (type, "ServiceType") == 0) {
		return GNOME_DESKTOP_ITEM_TYPE_SERVICE_TYPE;
	} else {
		return GNOME_DESKTOP_ITEM_TYPE_OTHER;
	}
}

/**
 * gnome_desktop_item_new:
 *
 * Creates a GnomeDesktopItem object. The reference count on the returned value is set to '1'.
 *
 * Returns: The new GnomeDesktopItem
 */
GnomeDesktopItem *
gnome_desktop_item_new (void)
{
        GnomeDesktopItem *retval;

        retval = g_new0 (GnomeDesktopItem, 1);

        retval->refcount++;

	retval->main_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
						   (GDestroyNotify) g_free,
						   (GDestroyNotify) g_free);
	
	/* These are guaranteed to be set */
	gnome_desktop_item_set_string (retval,
				       GNOME_DESKTOP_ITEM_NAME,
				       _("No name"));
	gnome_desktop_item_set_string (retval,
				       GNOME_DESKTOP_ITEM_ENCODING,
				       _("UTF-8"));
	gnome_desktop_item_set_string (retval,
				       GNOME_DESKTOP_ITEM_VERSION,
				       _("1.0"));

        return retval;
}

static Section *
dup_section (Section *sec)
{
	GList *li;
	Section *retval = g_new0 (Section, 1);

	retval->name = g_strdup (sec->name);

	retval->keys = g_list_copy (sec->keys);
	for (li = retval->keys; li != NULL; li = li->next)
		li->data = g_strdup (li->data);

	return retval;
}

static void
copy_string_hash (gpointer key, gpointer value, gpointer user_data)
{
	GHashTable *copy = user_data;
	g_hash_table_replace (copy,
			      g_strdup (key),
			      g_strdup (value));
}


/**
 * gnome_desktop_item_copy:
 * @item: The item to be copied
 *
 * Creates a copy of a GnomeDesktopItem.  The new copy has a refcount of 1.
 * Note: Section stack is NOT copied.
 *
 * Returns: The new copy 
 */
GnomeDesktopItem *
gnome_desktop_item_copy (const GnomeDesktopItem *item)
{
	GList *li;
        GnomeDesktopItem *retval;

        g_return_val_if_fail (item != NULL, NULL);
        g_return_val_if_fail (item->refcount > 0, NULL);

        retval = gnome_desktop_item_new ();

	retval->type = item->type;
	retval->modified = item->modified;
	retval->location = g_strdup (item->location);
	retval->mtime = item->mtime;

	/* Keys */
	retval->keys = g_list_copy (item->keys);
	for (li = retval->keys; li != NULL; li = li->next)
		li->data = g_strdup (li->data);

	/* Sections */
	retval->sections = g_list_copy (item->sections);
	for (li = retval->sections; li != NULL; li = li->next)
		li->data = dup_section (li->data);

	retval->main_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
						   (GDestroyNotify) g_free,
						   (GDestroyNotify) g_free);

	g_hash_table_foreach (item->main_hash,
			      copy_string_hash,
			      retval->main_hash);

        return retval;
}

static void
read_sort_order (GnomeDesktopItem *item, const char *dir)
{
	char *file;
	char buf[BUFSIZ];
	GString *str;
	ReadBuf *rb;

	file = g_build_filename (dir, ".order", NULL);
	rb = readbuf_open (file);
	g_free (file);

	if (rb == NULL)
		return;

	str = NULL;
	while (readbuf_gets (buf, sizeof (buf), rb) != NULL) {
		if (str == NULL)
			str = g_string_new (buf);
		else
			g_string_append (str, buf);
		g_string_append_c (str, ';');
	}
	readbuf_close (rb);
	if (str != NULL) {
		gnome_desktop_item_set_string (item, GNOME_DESKTOP_ITEM_SORT_ORDER,
					       str->str);
		g_string_free (str, TRUE);
	}
}

static GnomeDesktopItem *
make_fake_directory (const char *dir)
{
	GnomeDesktopItem *item;
	char *file;

	item = gnome_desktop_item_new ();
	gnome_desktop_item_set_entry_type (item,
					   GNOME_DESKTOP_ITEM_TYPE_DIRECTORY);
	file = g_build_filename (dir, ".directory", NULL);
	item->mtime = DONT_UPDATE_MTIME; /* it doesn't exist, we know that */
	gnome_desktop_item_set_location (item, file);
	item->mtime = 0;
	g_free (file);

	read_sort_order (item, dir);
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
				  GnomeDesktopItemLoadFlags flags,
				  GError **error)
{
        GnomeDesktopItem *retval;
	char *uri;

        g_return_val_if_fail (file != NULL, NULL);

	if (g_path_is_absolute (file)) {
		uri = gnome_vfs_get_uri_from_local_path (file);
	} else {
		char *cur = g_get_current_dir ();
		char *full = g_build_filename (cur, file, NULL);
		g_free (cur);
		uri = gnome_vfs_get_uri_from_local_path (full);
		g_free (full);
	}

	retval = gnome_desktop_item_new_from_uri (uri, flags, error);

	g_free (uri);

	return retval;
}

static char *
get_dirname (const char *uri)
{
	GnomeVFSURI *vfsuri = gnome_vfs_uri_new (uri);
	char *dirname;

	if (vfsuri == NULL)
		return NULL;
	dirname = gnome_vfs_uri_extract_dirname (vfsuri);
	gnome_vfs_uri_unref (vfsuri);
	return dirname;
}

/**
 * gnome_desktop_item_new_from_uri:
 * @url: GnomeVFSURI to load the GnomeDesktopItem from
 * @flags: Flags to influence the loading process
 *
 * This function loads 'file' and turns it into a GnomeDesktopItem.
 * If 'file' is a directory, it loads all the items under that
 * directory as subitems of the directory's GnomeDesktopItem.
 *
 * Returns: The newly loaded item.
 */
GnomeDesktopItem *
gnome_desktop_item_new_from_uri (const char *uri,
				 GnomeDesktopItemLoadFlags flags,
				 GError **error)
{
	GnomeDesktopItem *retval;
	char *subfn, *dir;
	struct stat sbuf;
	GnomeVFSFileInfo *info;
	time_t mtime = 0;

	g_return_val_if_fail (uri != NULL, NULL);

	info = gnome_vfs_file_info_new ();

	if (gnome_vfs_get_file_info (uri, info,
				     GNOME_VFS_FILE_INFO_DEFAULT) != GNOME_VFS_OK) {
		g_set_error (error,
			     /* FIXME: better errors */
			     GNOME_DESKTOP_ITEM_ERROR,
			     GNOME_DESKTOP_ITEM_ERROR_CANNOT_OPEN,
			     _("Error reading file '%s': %s"),
			     uri, strerror (errno));

		gnome_vfs_file_info_unref (info);

                return NULL;
	}

	if (info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MTIME)
		mtime = info->mtime;
	else
		mtime = 0;

	if (info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_TYPE &&
	    info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
                subfn = g_build_filename (uri, ".directory", NULL);
		gnome_vfs_file_info_clear (info);
		if (gnome_vfs_get_file_info (subfn, info,
					     GNOME_VFS_FILE_INFO_DEFAULT) != GNOME_VFS_OK) {
			gnome_vfs_file_info_unref (info);
			g_free (subfn);
			return make_fake_directory (uri);
		}

		if (info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MTIME)
			mtime = info->mtime;
		else
			mtime = 0;
	} else {
                subfn = g_strdup (uri);
	}

	gnome_vfs_file_info_clear (info);

	retval = ditem_load (subfn, error);

        if (retval == NULL) {
		gnome_vfs_file_info_unref (info);
		g_free (subfn);
		return NULL;
	}

	if (flags & GNOME_DESKTOP_ITEM_LOAD_ONLY_IF_EXISTS &&
	    ! gnome_desktop_item_exists (retval)) {
		gnome_vfs_file_info_unref (info);
		gnome_desktop_item_unref (retval);
		g_free (subfn);
		return NULL;
	}

        retval->mtime = DONT_UPDATE_MTIME;
	gnome_desktop_item_set_location (retval, subfn);
        retval->mtime = sbuf.st_mtime;

	dir = get_dirname (retval->location);
	if (dir != NULL) {
		read_sort_order (retval, dir);
		g_free (dir);
	}

	gnome_vfs_file_info_unref (info);
	g_free (subfn);

        return retval;
}

/**
 * gnome_desktop_item_save:
 * @item: A desktop item
 * @under: A new uri (location) for this #GnomeDesktopItem
 * @force: Save even if it wasn't modified
 * @error: #GError return
 *
 * Writes the specified item to disk.  If the 'under' is NULL, the original
 * location is used.  It sets the location of this entry to point to the
 * new location.
 *
 * Returns: boolean. %TRUE if the file was saved, %FALSE otherwise
 */
gboolean
gnome_desktop_item_save (GnomeDesktopItem *item,
			 const char *under,
			 gboolean force,
			 GError **error)
{
	const char *uri;

	if (under == NULL &&
	    ! force &&
	    ! item->modified)
		return TRUE;

	if (under == NULL)
		uri = item->location;
	else
		uri = under;

	if (uri == NULL) {
		g_set_error (error,
			     GNOME_DESKTOP_ITEM_ERROR,
			     GNOME_DESKTOP_ITEM_ERROR_NO_FILENAME,
			     _("No filename to save to"));
		return FALSE;
	}

	if ( ! ditem_save (item, uri, error))
		return FALSE;

	item->modified = FALSE;
        item->mtime = time (NULL);

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

static void
free_section (gpointer data, gpointer user_data)
{
	Section *section = data;

	g_free (section->name);
	section->name = NULL;

	g_list_foreach (section->keys, (GFunc)g_free, NULL);
	g_list_free (section->keys);
	section->keys = NULL;

	g_free (section);
}

/**
 * gnome_desktop_item_unref:
 * @item: A desktop item
 *
 * Decreases the reference count of the specified item, and destroys the item if there are no more references left.
 */
void
gnome_desktop_item_unref (GnomeDesktopItem *item)
{
        g_return_if_fail (item != NULL);
        g_return_if_fail (item->refcount > 0);

        item->refcount--;

        if(item->refcount != 0)
                return;

	g_list_foreach (item->languages, (GFunc)g_free, NULL);
	g_list_free (item->languages);
	item->languages = NULL;

	g_list_foreach (item->keys, (GFunc)g_free, NULL);
	g_list_free (item->keys);
	item->keys = NULL;

	g_list_foreach (item->sections, free_section, NULL);
	g_list_free (item->sections);
	item->sections = NULL;

	g_hash_table_destroy (item->main_hash);
	item->main_hash = NULL;

        g_free (item->location);
        item->location = NULL;

        g_free (item);
}

static Section *
find_section (GnomeDesktopItem *item, const char *section)
{
	GList *li;
	Section *sec;

	if (section == NULL)
		return NULL;
	if (strcmp (section, "Desktop Entry") == 0)
		return NULL;

	for (li = item->sections; li != NULL; li = li->next) {
		sec = li->data;
		if (strcmp (sec->name, section) == 0)
			return sec;
	}

	sec = g_new0 (Section, 1);
	sec->name = g_strdup (section);
	sec->keys = NULL;

	item->sections = g_list_append (item->sections, sec);

	/* Don't mark the item modified, this is just an empty section,
	 * it won't be saved even */

	return sec;
}

static Section *
section_from_key (GnomeDesktopItem *item, const char *key)
{
	char *p;
	char *name;
	Section *sec;

	if (key == NULL)
		return NULL;

	p = strchr (key, '/');
	if (p == NULL)
		return NULL;

	name = g_strndup (key, p - key);

	sec = find_section (item, name);

	g_free (name);

	return sec;
}

static const char *
key_basename (const char *key)
{
	char *p = strrchr (key, '/');
	if (p != NULL)
		return p+1;
	else
		return key;
}


static const char *
lookup (const GnomeDesktopItem *item, const char *key)
{
	return g_hash_table_lookup (item->main_hash, key);
}

static const char *
lookup_locale (const GnomeDesktopItem *item, const char *key, const char *locale)
{
	if (locale == NULL ||
	    strcmp (locale, "C") == 0) {
		return lookup (item, key);
	} else {
		const char *ret;
		char *full = g_strdup_printf ("%s[%s]", key, locale);
		ret = lookup (item, full);
		g_free (full);
		return ret;
	}
}

static const char *
lookup_best_locale (const GnomeDesktopItem *item, const char *key)
{
	const GList *list = gnome_i18n_get_language_list ("LC_MESSAGES");
	while (list != NULL) {
		const char *ret;
		const char *locale = list->data;

		ret = lookup_locale (item, key, locale);
		if (ret != NULL)
			return ret;

		list = list->next;
	}

	return NULL;
}

static void
set (GnomeDesktopItem *item, const char *key, const char *value)
{
	Section *sec = section_from_key (item, key);

	if (sec != NULL) {
		if (value != NULL) {
			if (g_hash_table_lookup (item->main_hash, key) == NULL)
				sec->keys = g_list_append
					(sec->keys,
					 g_strdup (key_basename (key)));

			g_hash_table_replace (item->main_hash,
					      g_strdup (key),
					      g_strdup (value));
		} else {
			GList *list = g_list_find_custom
				(sec->keys, key_basename (key),
				 (GCompareFunc)strcmp);
			if (list != NULL) {
				g_free (list->data);
				sec->keys =
					g_list_delete_link (sec->keys, list);
			}
			g_hash_table_remove (item->main_hash, key);
		}
	} else {
		if (value != NULL) {
			if (g_hash_table_lookup (item->main_hash, key) == NULL)
				item->keys = g_list_append (item->keys,
							    g_strdup (key));

			g_hash_table_replace (item->main_hash, 
					      g_strdup (key),
					      g_strdup (value));
		} else {
			GList *list = g_list_find_custom
				(item->keys, key, (GCompareFunc)strcmp);
			if (list != NULL) {
				g_free (list->data);
				item->keys =
					g_list_delete_link (item->keys, list);
			}
			g_hash_table_remove (item->main_hash, key);
		}
	}
	item->modified = TRUE;
}

static void
set_locale (GnomeDesktopItem *item, const char *key,
	    const char *locale, const char *value)
{
	if (locale == NULL ||
	    strcmp (locale, "C") == 0) {
		set (item, key, value);
	} else {
		char *full = g_strdup_printf ("%s[%s]", key, locale);
		set (item, full, value);
		g_free (full);
	}
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
	if (string == NULL)
		string = "";
	for (i = 0; i < argc; i++) {
		char *arg = argv[i];
		char *p = strstr (arg,ps);
		char *s;
		size_t start, string_len, ps_len;
		if (p == NULL)
			continue;
		string_len = strlen (string);
		ps_len = strlen (ps);
		s = g_new (char, strlen (arg) - ps_len + string_len + 1);
		start = p - arg;
		strncpy (s, arg, start);
		strcpy (&s[start], string);
		strcpy (&s[start+string_len], &arg[start+ps_len]);
		argv[i] = s;
		*tofree = g_slist_prepend (*tofree, s);
		ret = TRUE;
		if (only_once)
			return ret;
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

        g_return_val_if_fail (item, -1);

        if(!argv)
                argc = 0;


	/* check the type, if there is one set */
	if(item->type != GNOME_DESKTOP_ITEM_TYPE_APPLICATION) {
		/* FIXME: ue GError */
		/* we are not an application so just exit
		   with an error */
		g_warning(_("Attempting to launch a non-application"));
		return -1;
	}

	if(gnome_desktop_item_get_boolean (item, "Terminal")) {
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
			    gnome_desktop_item_get_string(item,"MiniIcon"), &tofree, FALSE);
#endif
	replace_percentsign(real_argc, real_argv, "%c", 
			    gnome_desktop_item_get_localestring(item, "Comment"), &tofree, FALSE);

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
strip_the_amp (char *exec)
{
	size_t exec_len;

	g_strstrip (exec);
	if (*exec == '\0')
		return FALSE;

	exec_len = strlen (exec);
	/* kill any trailing '&' */
	if (exec[exec_len-1] == '&') {
		exec[exec_len-1] = '\0';
		g_strchomp (exec);
	}

	/* can't exactly launch an empty thing */
	if (*exec == '\0')
		return FALSE;

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
gnome_desktop_item_launch (const GnomeDesktopItem *item,
			   int argc,
			   char **argv,
			   GError **error)
{
	const char **temp_argv = NULL;
	int temp_argc = 0;
	const char *exec;
	char *the_exec;
	int ret;

	exec = gnome_desktop_item_get_string (item, "Exec");
	/* This is a URL, so launch it as a url */
	if (item->type == GNOME_DESKTOP_ITEM_TYPE_LINK) {
		const char *url;
		url = gnome_desktop_item_get_string (item, "URL");
		if (url && url[0] != '\0') {
			if (gnome_url_show (url, NULL))
				return 0;
			else
				return -1;
		/* Gnome panel used to put this in Exec */
		} else if (exec && exec[0] != '\0') {
			if (gnome_url_show (exec, NULL))
				return 0;
			else
				return -1;
		} else {
			return -1;
		}
		/* FIXME: use GError */
	}

	if(exec[0] == '\0') {
		/* FIXME: use GError */
		g_warning(_("Trying to execute an item with no 'Exec'"));
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
gnome_desktop_item_drop_uri_list (const GnomeDesktopItem *item,
				  GList *uri_list,
				  GError **error)
{
	/* FIXME: */
#if 0
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
#endif
	return -1;
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
	const char *try_exec;
	const char *exec;

        g_return_val_if_fail (item != NULL, FALSE);

	try_exec = lookup (item, GNOME_DESKTOP_ITEM_TRY_EXEC);

	if (try_exec != NULL) {
                char *tryme;

		tryme = gnome_is_program_in_path (try_exec);
		if (tryme != NULL) {
			g_free (tryme);
			/* We will try the application itself
			 * next if applicable */
		} else {
			return FALSE;
		}
	}

	if (item->type == GNOME_DESKTOP_ITEM_TYPE_APPLICATION) {
		int argc;
		char **argv;
		const char *exe;

		exec = lookup (item, GNOME_DESKTOP_ITEM_EXEC);
		if (exec == NULL)
			return FALSE;

		if ( ! g_shell_parse_argv (exec, &argc, &argv, NULL))
			return FALSE;

		if (argc < 1) {
			g_strfreev (argv);
			return FALSE;
		}

		exe = argv[0];

		if (exe[0] == G_DIR_SEPARATOR &&
		    access (argv[0], X_OK) != 0) {
			g_strfreev (argv);
			return FALSE;
		}

		if (exe[0] != G_DIR_SEPARATOR) {
			char *tryme = gnome_is_program_in_path (exe);
			if (tryme == NULL) {
				g_strfreev (argv);
				return FALSE;
			}
			g_free (tryme);
		}

		g_strfreev (argv);
	}

	return TRUE;
}

/**
 * gnome_desktop_item_get_entry_type:
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
GnomeDesktopItemType
gnome_desktop_item_get_entry_type (const GnomeDesktopItem *item)
{
	g_return_val_if_fail (item != NULL, 0);
	g_return_val_if_fail (item->refcount > 0, 0);

	return item->type;
}

void
gnome_desktop_item_set_entry_type (GnomeDesktopItem *item,
				   GnomeDesktopItemType type)
{
	g_return_if_fail (item != NULL);
	g_return_if_fail (item->refcount > 0);

	item->type = type;

	switch (type) {
	case GNOME_DESKTOP_ITEM_TYPE_NULL:
		set (item, GNOME_DESKTOP_ITEM_TYPE, NULL);
		break;
	case GNOME_DESKTOP_ITEM_TYPE_APPLICATION:
		set (item, GNOME_DESKTOP_ITEM_TYPE, "Application");
		break;
	case GNOME_DESKTOP_ITEM_TYPE_LINK:
		set (item, GNOME_DESKTOP_ITEM_TYPE, "Link");
		break;
	case GNOME_DESKTOP_ITEM_TYPE_FSDEVICE:
		set (item, GNOME_DESKTOP_ITEM_TYPE, "FSDevice");
		break;
	case GNOME_DESKTOP_ITEM_TYPE_MIME_TYPE:
		set (item, GNOME_DESKTOP_ITEM_TYPE, "MimeType");
		break;
	case GNOME_DESKTOP_ITEM_TYPE_DIRECTORY:
		set (item, GNOME_DESKTOP_ITEM_TYPE, "Directory");
		break;
	case GNOME_DESKTOP_ITEM_TYPE_SERVICE:
		set (item, GNOME_DESKTOP_ITEM_TYPE, "Service");
		break;
	case GNOME_DESKTOP_ITEM_TYPE_SERVICE_TYPE:
		set (item, GNOME_DESKTOP_ITEM_TYPE, "ServiceType");
		break;
	default:
		break;
	}
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
gnome_desktop_item_get_file_status (const GnomeDesktopItem *item)
{
        struct stat sbuf;
        GnomeDesktopItemStatus retval;

        g_return_val_if_fail (item != NULL, GNOME_DESKTOP_ITEM_DISAPPEARED);
        g_return_val_if_fail (item->refcount > 0, GNOME_DESKTOP_ITEM_DISAPPEARED);

        if (item->location == NULL ||
	    stat (item->location, &sbuf) != 0)
                return GNOME_DESKTOP_ITEM_DISAPPEARED;

	if (sbuf.st_mtime > item->mtime)
		retval = GNOME_DESKTOP_ITEM_CHANGED;
	else
		retval = GNOME_DESKTOP_ITEM_UNCHANGED;

        return retval;
}

/**
 * gnome_desktop_item_get_icon:
 * @item: A desktop item
 *
 * Description:  This function goes and looks for the icon file.  If the icon
 * is not set as an absolute filename, this will look for it in the standard places.
 * If it can't find the icon, it will return %NULL
 *
 * Returns: A newly allocated string
 */
char *
gnome_desktop_item_get_icon (const GnomeDesktopItem *item)
{
	const char *icon;

        g_return_val_if_fail (item != NULL, NULL);
        g_return_val_if_fail (item->refcount > 0, NULL);

	icon = gnome_desktop_item_get_string (item, GNOME_DESKTOP_ITEM_ICON);

	if (icon == NULL) {
		return NULL;
	} else if (icon[0] == G_DIR_SEPARATOR) {
		if (g_file_test (icon, G_FILE_TEST_EXISTS)) {
			return g_strdup (icon);
		} else {
			return NULL;
		}
	} else {
		char *full = gnome_program_locate_file (NULL,
							GNOME_FILE_DOMAIN_PIXMAP,
							icon,
							TRUE /* only_if_exists */,
							NULL /* ret_locations */);

		/* FIXME: Maybe check elsewhere, this could be from KDE
		 * or some such */
		return full;
	}
}

/**
 * gnome_desktop_item_get_location:
 * @item: A desktop item
 *
 * Returns: The file location associated with 'item'.
 *
 */
const char *
gnome_desktop_item_get_location (const GnomeDesktopItem *item)
{
        g_return_val_if_fail (item != NULL, NULL);
        g_return_val_if_fail (item->refcount > 0, NULL);

        return item->location;
}

/**
 * gnome_desktop_item_set_location:
 * @item: A desktop item
 * @location: A uri string specifying the file location of this particular item.
 *
 * Set's the 'location' uri of this item.
 *
 * Returns:
 */
void
gnome_desktop_item_set_location (GnomeDesktopItem *item, const char *location)
{
        g_return_if_fail (item != NULL);
        g_return_if_fail (item->refcount > 0);

	if (item->location != NULL &&
	    location != NULL &&
	    strcmp (item->location, location) == 0)
		return;

        g_free (item->location);
	item->location = g_strdup (location);

	/* This is ugly, but useful internally */
	if (item->mtime != DONT_UPDATE_MTIME) {
		item->mtime = 0;

		if (item->location) {
			GnomeVFSFileInfo *info;
			GnomeVFSResult    res;

			info = gnome_vfs_file_info_new ();

			res = gnome_vfs_get_file_info (item->location, info,
						       GNOME_VFS_FILE_INFO_DEFAULT);

			if (res == GNOME_VFS_OK &&
			    info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MTIME)
				item->mtime = info->mtime;

			gnome_vfs_file_info_unref (info);
		}
	}

	/* Make sure that save actually saves */
	item->modified = TRUE;
}

/*
 * Reading/Writing different sections, NULL is the standard section
 */

gboolean
gnome_desktop_item_attr_exists (const GnomeDesktopItem *item,
				const char *attr)
{
	g_return_val_if_fail (item != NULL, FALSE);
	g_return_val_if_fail (item->refcount > 0, FALSE);
	g_return_val_if_fail (attr != NULL, FALSE);

	return lookup (item, attr) != NULL;
}

/*
 * String type
 */
const char *
gnome_desktop_item_get_string (const GnomeDesktopItem *item,
			       const char *attr)
{
	g_return_val_if_fail (item != NULL, NULL);
	g_return_val_if_fail (item->refcount > 0, NULL);
	g_return_val_if_fail (attr != NULL, NULL);

	return lookup (item, attr);
}

void
gnome_desktop_item_set_string (GnomeDesktopItem *item,
			       const char *attr,
			       const char *value)
{
	g_return_if_fail (item != NULL);
	g_return_if_fail (item->refcount > 0);
	g_return_if_fail (attr != NULL);

	set (item, attr, value);

	if (strcmp (attr, GNOME_DESKTOP_ITEM_TYPE) == 0)
		item->type = type_from_string (value);
}

/*
 * LocaleString type
 */
const char *
gnome_desktop_item_get_localestring (const GnomeDesktopItem *item,
				     const char *attr)
{
	g_return_val_if_fail (item != NULL, NULL);
	g_return_val_if_fail (item->refcount > 0, NULL);
	g_return_val_if_fail (attr != NULL, NULL);

	return lookup_best_locale (item, attr);
}

const char *
gnome_desktop_item_get_localestring_lang (const GnomeDesktopItem *item,
					  const char *attr,
					  const char *language)
{
	g_return_val_if_fail (item != NULL, NULL);
	g_return_val_if_fail (item->refcount > 0, NULL);
	g_return_val_if_fail (attr != NULL, NULL);

	return lookup_locale (item, attr, language);
}

GList *
gnome_desktop_item_get_languages (const GnomeDesktopItem *item,
				  const char *attr)
{
	GList *li;
	GList *list = NULL;

	g_return_val_if_fail (item != NULL, NULL);
	g_return_val_if_fail (item->refcount > 0, NULL);

	for (li = item->languages; li != NULL; li = li->next) {
		char *language = li->data;
		if (attr == NULL ||
		    lookup_locale (item, attr, language) != NULL) {
			list = g_list_prepend (list, language);
		}
	}

	return g_list_reverse (list);
}

static const char *
get_language (void)
{
	const GList *list = gnome_i18n_get_language_list ("LC_MESSAGES");
	while (list != NULL) {
		const char *lang = list->data;
		/* find first without encoding */
		if (strchr (lang, '.') == NULL)
			return lang;
	}
	return NULL;
}

void
gnome_desktop_item_set_localestring (GnomeDesktopItem *item,
				     const char *attr,
				     const char *value)
{
	g_return_if_fail (item != NULL);
	g_return_if_fail (item->refcount > 0);
	g_return_if_fail (attr != NULL);

	set_locale (item, attr, get_language (), value);
}

void
gnome_desktop_item_set_localestring_lang (GnomeDesktopItem *item,
					  const char *attr,
					  const char *language,
					  const char *value)
{
	g_return_if_fail (item != NULL);
	g_return_if_fail (item->refcount > 0);
	g_return_if_fail (attr != NULL);

	set_locale (item, attr, language, value);
}

void
gnome_desktop_item_clear_localestring (GnomeDesktopItem *item,
				       const char *attr)
{
	GList *li;
	GList *list = NULL;

	g_return_if_fail (item != NULL);
	g_return_if_fail (item->refcount > 0);
	g_return_if_fail (attr != NULL);

	for (li = item->languages; li != NULL; li = li->next) {
		char *language = li->data;
		set_locale (item, attr, language, NULL);
	}
	set (item, attr, NULL);
}

/*
 * Strings, Regexps types
 */

char **
gnome_desktop_item_get_strings (const GnomeDesktopItem *item,
				const char *attr)
{
	const char *value;

	g_return_val_if_fail (item != NULL, NULL);
	g_return_val_if_fail (item->refcount > 0, NULL);
	g_return_val_if_fail (attr != NULL, NULL);

	value = lookup (item, attr);
	if (value == NULL)
		return NULL;

	/* FIXME: there's no way to escape semicolons apparently */
	return g_strsplit (value, ";", -1);
}

void
gnome_desktop_item_set_strings (GnomeDesktopItem *item,
				const char *attr,
				char **strings)
{
	char *str, *str2;

	g_return_if_fail (item != NULL);
	g_return_if_fail (item->refcount > 0);
	g_return_if_fail (attr != NULL);

	str = g_strjoinv (";", strings);
	str2 = g_strconcat (str, ";");
	/* FIXME: there's no way to escape semicolons apparently */
	set (item, attr, str2);
	g_free (str);
	g_free (str2);
}

/*
 * Boolean type
 */
gboolean
gnome_desktop_item_get_boolean (const GnomeDesktopItem *item,
				const char *attr)
{
	const char *value;

	g_return_val_if_fail (item != NULL, FALSE);
	g_return_val_if_fail (item->refcount > 0, FALSE);
	g_return_val_if_fail (attr != NULL, FALSE);

	value = lookup (item, attr);
	if (value == NULL)
		return FALSE;

	return (value[0] == 'T' ||
		value[0] == 't' ||
		value[0] == 'Y' ||
		value[0] == 'y' ||
		atoi (value) != 0);
}

void
gnome_desktop_item_set_boolean (GnomeDesktopItem *item,
				const char *attr,
				gboolean value)
{
	g_return_if_fail (item != NULL);
	g_return_if_fail (item->refcount > 0);
	g_return_if_fail (attr != NULL);

	set (item, attr, value ? "true" : "false");
}

/*
* Clearing attributes
 */
void
gnome_desktop_item_clear_section (GnomeDesktopItem *item,
				  const char *section)
{
	Section *sec;
	GList *li;

	g_return_if_fail (item != NULL);
	g_return_if_fail (item->refcount > 0);

	sec = find_section (item, section);

	if (sec == NULL) {
		for (li = item->keys; li != NULL; li = li->next) {
			g_hash_table_remove (item->main_hash, li->data);
			g_free (li->data);
			li->data = NULL;
		}
		g_list_free (item->keys);
		item->keys = NULL;
	} else {
		for (li = sec->keys; li != NULL; li = li->next) {
			char *key = li->data;
			char *full = g_strdup_printf ("%s/%s",
						      sec->name, key);
			g_hash_table_remove (item->main_hash, full);
			g_free (full);
			g_free (key);
			li->data = NULL;
		}
		g_list_free (sec->keys);
		sec->keys = NULL;
	}
	item->modified = TRUE;
}

/************************************************************
 * Parser:                                                  *
 ************************************************************/

static gboolean G_GNUC_CONST
standard_is_boolean (const char * key)
{
	static GHashTable *bools = NULL;

	if (bools == NULL) {
		bools = g_hash_table_new (g_str_hash, g_str_equal);
		g_hash_table_insert (bools,
				     GNOME_DESKTOP_ITEM_NO_DISPLAY,
				     GNOME_DESKTOP_ITEM_NO_DISPLAY);
		g_hash_table_insert (bools,
				     GNOME_DESKTOP_ITEM_HIDDEN,
				     GNOME_DESKTOP_ITEM_HIDDEN);
		g_hash_table_insert (bools,
				     GNOME_DESKTOP_ITEM_TERMINAL,
				     GNOME_DESKTOP_ITEM_TERMINAL);
		g_hash_table_insert (bools,
				     GNOME_DESKTOP_ITEM_READ_ONLY,
				     GNOME_DESKTOP_ITEM_READ_ONLY);
	}

	return g_hash_table_lookup (bools, key) != NULL;
}

static gboolean G_GNUC_CONST
standard_is_numeric (const char *key)
{
	if (strcmp (key, GNOME_DESKTOP_ITEM_VERSION) == 0)
		return TRUE;
	else
		return FALSE;
}

static gboolean G_GNUC_CONST
standard_is_strings (const char *key)
{
	static GHashTable *strings = NULL;

	if (strings == NULL) {
		strings = g_hash_table_new (g_str_hash, g_str_equal);
		g_hash_table_insert (strings,
				     GNOME_DESKTOP_ITEM_FILE_PATTERN,
				     GNOME_DESKTOP_ITEM_FILE_PATTERN);
		g_hash_table_insert (strings,
				     GNOME_DESKTOP_ITEM_ACTIONS,
				     GNOME_DESKTOP_ITEM_ACTIONS);
		g_hash_table_insert (strings,
				     GNOME_DESKTOP_ITEM_MIME_TYPE,
				     GNOME_DESKTOP_ITEM_MIME_TYPE);
		g_hash_table_insert (strings,
				     GNOME_DESKTOP_ITEM_PATTERNS,
				     GNOME_DESKTOP_ITEM_PATTERNS);
		g_hash_table_insert (strings,
				     GNOME_DESKTOP_ITEM_SORT_ORDER,
				     GNOME_DESKTOP_ITEM_SORT_ORDER);
	}

	return g_hash_table_lookup (strings, key) != NULL;
}

/* If no need to cannonize, returns NULL */
static char *
cannonize (const char *key, const char *value)
{
	if (standard_is_boolean (key)) {
		if (value[0] == 'T' ||
		    value[0] == 't' ||
		    value[0] == 'Y' ||
		    value[0] == 'y' ||
		    atoi (value) != 0) {
			return g_strdup ("true");
		} else {
			return g_strdup ("false");
		}
	} else if (standard_is_numeric (key)) {
		double num = 0.0;
		sscanf (value, "%lf", &num);
		return g_strdup_printf ("%g", num);
	} else if (standard_is_strings (key)) {
		int len = strlen (value);
		if (len == 0 || value[len-1] != ';') {
			return g_strconcat (value, ";", NULL);
		}
	}
	return NULL;
}


static char *
decode_string_and_dup (const char *s)
{
	char *p = g_malloc (strlen (s) + 1);
	char *q = p;

	do {
		if (*s == '\\'){
			switch (*(++s)){
			case 'n':
				*p++ = '\n';
				break;
			case '\\':
				*p++ = '\\';
				break;
			case 'r':
				*p++ = '\r';
				break;
			default:
				*p++ = '\\';
				*p++ = *s;
				break;
			}
		} else {
			*p++ = *s;
		}
	} while (*s++);

	return q;
}

static char *
escape_string_and_dup (const char *s)
{
	char *return_value, *p;
	const char *q;
	int len = 0;

	if (s == NULL)
		return g_strdup("");
	
	q = s;
	while (*q){
		len++;
		if (*q == '\n' || *q == '\\' || *q == '\r' || *q == '\0')
			len++;
		q++;
	}
	return_value = p = (char *) g_malloc (len + 1);
	do {
		switch (*s){
		case '\n':
			*p++ = '\\';
			*p++ = 'n';
			break;
		case '\r':
			*p++ = '\\';
			*p++ = 'r';
			break;
		case '\\':
			*p++ = '\\';
			*p++ = '\\';
			break;
		default:
			*p++ = *s;
		}
	} while (*s++);
	return return_value;
}

static gboolean
check_locale (const char *locale)
{
	GIConv cd = g_iconv_open ("UTF-8", locale);
	if ((GIConv)-1 == cd)
		return FALSE;
	g_iconv_close (cd);
	return TRUE;
}

static void
insert_locales (GHashTable *encodings, char *enc, ...)
{
	va_list args;
	char *s;

	va_start (args, enc);
	for (;;) {
		s = va_arg (args, char *);
		if (s == NULL)
			break;
		g_hash_table_insert (encodings, s, enc);
	}
	va_end (args);
}

/* make a standard conversion table from the desktop standard spec */
static GHashTable *
init_encodings (void)
{
	GHashTable *encodings = g_hash_table_new (g_str_hash, g_str_equal);

	/* "C" is plain ascii */
	insert_locales (encodings, "ASCII", "C", NULL);

	insert_locales (encodings, "ARMSCII-8", "by", NULL);
	insert_locales (encodings, "BIG5", "zh_TW", NULL);
	insert_locales (encodings, "CP1251", "be", "bg", NULL);
	if (check_locale ("EUC-CN")) {
		insert_locales (encodings, "EUC-CN", "zh_CN", NULL);
	} else {
		insert_locales (encodings, "GB2312", "zh_CN", NULL);
	}
	insert_locales (encodings, "EUC-JP", "ja", NULL);
	insert_locales (encodings, "EUC-KR", "ko", NULL);
	/*insert_locales (encodings, "GEORGIAN-ACADEMY", NULL);*/
	insert_locales (encodings, "GEORGIAN-PS", "ka", NULL);
	insert_locales (encodings, "ISO-8859-1", "br", "ca", "da", "de", "en", "es", "eu", "fi", "fr", "gl", "it", "nl", "wa", "no", "pt", "pt", "sv", NULL);
	insert_locales (encodings, "ISO-8859-2", "cs", "hr", "hu", "pl", "ro", "sk", "sl", "sq", "sr", NULL);
	insert_locales (encodings, "ISO-8859-3", "eo", NULL);
	insert_locales (encodings, "ISO-8859-5", "mk", "sp", NULL);
	insert_locales (encodings, "ISO-8859-7", "el", NULL);
	insert_locales (encodings, "ISO-8859-9", "tr", NULL);
	insert_locales (encodings, "ISO-8859-13", "lt", "lv", "mi", NULL);
	insert_locales (encodings, "ISO-8859-14", "ga", "cy", NULL);
	insert_locales (encodings, "ISO-8859-15", "et", NULL);
	insert_locales (encodings, "KOI8-R", "ru", NULL);
	insert_locales (encodings, "KOI8-U", "uk", NULL);
	if (check_locale ("TCVN-5712")) {
		insert_locales (encodings, "TCVN-5712", "vi", NULL);
	} else {
		insert_locales (encodings, "TCVN", "vi", NULL);
	}
	insert_locales (encodings, "TIS-620", "th", NULL);
	/*insert_locales (encodings, "VISCII", NULL);*/

	return encodings;
}

static const char *
get_encoding_from_locale (const char *locale)
{
	char lang[3];
	const char *encoding;
	static GHashTable *encodings = NULL;

	if (locale == NULL)
		return NULL;

	/* if locale includes encoding, use it */
	encoding = strchr (locale, '.');
	if (encoding != NULL) {
		return encoding+1;
	}

	if (encodings == NULL)
		encodings = init_encodings ();

	/* first try the entire locale (at this point ll_CC) */
	encoding = g_hash_table_lookup (encodings, locale);
	if (encoding != NULL)
		return encoding;

	/* Try just the language */
	strncpy (lang, locale, 2);
	lang[2] = '\0';
	return g_hash_table_lookup (encodings, lang);
}

static Encoding
get_encoding (const char *uri, ReadBuf *rb)
{
	int c;
	gboolean old_kde = FALSE;
#if 0
	gchar *contents = NULL;
#endif
	
	while ((c = readbuf_getc (rb)) != EOF) {
		/* find starts of lines */
		if (c == '\n' || c == '\r') {
			char buf[256];
			int i = 0;
			while ((c = readbuf_getc (rb)) != EOF) {
				if (c == '\n' ||
				    c == '\r' ||
				    i >= sizeof(buf) - 1)
					break;
				buf[i++] = c;
			}
			buf[i++] = '\0';
			if (strcmp (GNOME_DESKTOP_ITEM_ENCODING "=UTF-8", buf) == 0) {
				return ENCODING_UTF8;
			} else if (strcmp (GNOME_DESKTOP_ITEM_ENCODING "=Legacy-Mixed", buf) == 0) {
				return ENCODING_LEGACY_MIXED;
			} else if (strncmp (GNOME_DESKTOP_ITEM_ENCODING "=", buf,
					    strlen (GNOME_DESKTOP_ITEM_ENCODING "=")) == 0) {
				/* According to the spec we're not supposed
				 * to read a file like this */
				return ENCODING_UNKNOWN;
			}
			if (strcmp ("[KDE Desktop Entry]", buf) == 0) {
				old_kde = TRUE;
				/* don't break yet, we still want to support
				 * Encoding even here */
			}
		}
	}

	if (old_kde)
		return ENCODING_LEGACY_MIXED;

	/* try to guess by location */
	if (strstr (uri, "gnome/apps/") != NULL) {
		/* old gnome */
		return ENCODING_LEGACY_MIXED;
	}

#if 0
	/* FIXME: fix for gnome-vfs */
	/* A dillema, new KDE files are in UTF-8 but have no Encoding
	 * info, at this time we really can't tell.  The best thing to
	 * do right now is to just assume UTF-8 if the whole file
	 * validates as utf8 I suppose */

	g_file_get_contents (file, &contents, NULL, NULL);
	if (contents == NULL) /* Ooooooops ! */
		return ENCODING_UNKNOWN;

	if (g_utf8_validate (contents, -1, NULL)) {
		g_free (contents);
		return ENCODING_UTF8;
	} else {
		g_free (contents);
		return ENCODING_LEGACY_MIXED;
	}
#endif
}

static char *
decode_string (const char *value, Encoding encoding, const char *locale)
{
	char *retval = NULL;

	/* if legacy mixed, then convert */
	if (locale != NULL && encoding == ENCODING_LEGACY_MIXED) {
		const char *char_encoding = get_encoding_from_locale (locale);
		char *utf8_string;
		if (char_encoding == NULL)
			return NULL;
		if (strcmp (char_encoding, "ASCII") == 0) {
			return decode_string_and_dup (value);
		}
		utf8_string = g_convert (value, -1, "UTF-8", char_encoding,
					NULL, NULL, NULL);
		if (utf8_string == NULL)
			return NULL;
		retval = decode_string_and_dup (utf8_string);
		g_free (utf8_string);
		return retval;
	/* if utf8, then validate */
	} else if (locale != NULL && encoding == ENCODING_UTF8) {
		if ( ! g_utf8_validate (value, -1, NULL))
			/* invalid utf8, ignore this key */
			return NULL;
		return decode_string_and_dup (value);
	} else {
		/* Meaning this is not a localized string */
		return decode_string_and_dup (value);
	}
}

static char *
snarf_locale_from_key (const char *key)
{
	const char *brace;
	char *locale, *p;

	brace = strchr (key, '[');
	if (brace == NULL)
		return NULL;

	locale = g_strdup (brace + 1);
	if (*locale == '\0') {
		g_free (locale);
		return NULL;
	}
	p = strchr (locale, ']');
	if (p == NULL) {
		g_free (locale);
		return NULL;
	}
	*p = '\0';
	return locale;
}

static void
insert_key (GnomeDesktopItem *item,
	    Section *cur_section,
	    Encoding encoding,
	    const char *key,
	    const char *value,
	    gboolean old_kde)
{
	char *k;
	char *val;

	/* we always store everything in UTF-8 */
	if (cur_section == NULL &&
	    strcmp (key, GNOME_DESKTOP_ITEM_ENCODING) == 0) {
		k = g_strdup (key);
		val = g_strdup ("UTF-8");
	} else {
		char *locale = snarf_locale_from_key (key);
		val = decode_string (value, encoding, locale);

		/* Ignore this key, it's whacked */
		if (val == NULL) {
			g_free (locale);
			return;
		}

		/* For old KDE entries, we can also split by a comma
		 * on sort order, so convert to semicolons */
		if (old_kde &&
		    cur_section == NULL &&
		    strcmp (key, GNOME_DESKTOP_ITEM_SORT_ORDER) == 0 &&
		    strchr (val, ';') == NULL) {
			int i;
			for (i = 0; val[i] != '\0'; i++) {
				if (val[i] == ',')
					val[i] = ';';
			}
		}

		/* Check some types, not perfect, but catches a lot
		 * of things */
		if (cur_section == NULL) {
			char *cannon = cannonize (key, val);
			if (cannon != NULL) {
				g_free (val);
				val = cannon;
			}
		}

		k = g_strdup (key);

		/* Take care of the language part */
		if (locale != NULL &&
		    strcmp (locale, "C") == 0) {
			char *p;
			/* Whack C locale */
			p = strchr (k, '[');
			*p = '\0';
			g_free (locale);
		} else if (locale != NULL) {
			char *p, *brace;

			/* Whack the encoding and modifier part */
			p = strchr (locale, '.');
			if (p != NULL)
				*p = '\0';
			p = strchr (locale, '@');
			if (p != NULL)
				*p = '\0';

			if (g_list_find_custom (item->languages, locale,
						(GCompareFunc)strcmp) == NULL) {
				item->languages = g_list_prepend
					(item->languages, locale);
			} else {
				g_free (locale);
			}

			/* Whack encoding and modifier from encoding in the key */ 
			brace = strchr (k, '[');
			p = strchr (brace, '.');
			if (p != NULL) {
				*p = ']';
				*(p+1) = '\0';
			}
			p = strchr (brace, '@');
			if (p != NULL) {
				*p = ']';
				*(p+1) = '\0';
			}
		}
	}


	if (cur_section == NULL) {
		/* only add to list if we haven't seen it before */
		if (g_hash_table_lookup (item->main_hash, k) == NULL) {
			item->keys = g_list_prepend (item->keys,
						     g_strdup (k));
		}
		/* later duplicates override earlier ones */
		g_hash_table_replace (item->main_hash, k, val);
	} else {
		char *full = g_strdup_printf
			("%s/%s",
			 cur_section->name, k);
		/* only add to list if we haven't seen it before */
		if (g_hash_table_lookup (item->main_hash, full) == NULL) {
			cur_section->keys =
				g_list_prepend (cur_section->keys, k);
		}
		/* later duplicates override earlier ones */
		g_hash_table_replace (item->main_hash,
				      full, val);
	}
}

static void
setup_type (GnomeDesktopItem *item, const char *filename)
{
	const char *type = g_hash_table_lookup (item->main_hash,
						GNOME_DESKTOP_ITEM_TYPE);
	if (type == NULL && filename != NULL) {
		char *base = g_path_get_basename (filename);
		if (base != NULL &&
		    strcmp (base, ".directory") == 0) {
			/* This gotta be a directory */
			g_hash_table_replace (item->main_hash,
					      g_strdup (GNOME_DESKTOP_ITEM_TYPE), 
					      g_strdup ("Directory"));
			item->keys = g_list_prepend
				(item->keys, g_strdup (GNOME_DESKTOP_ITEM_TYPE));
			item->type = GNOME_DESKTOP_ITEM_TYPE_DIRECTORY;
		} else {
			item->type = GNOME_DESKTOP_ITEM_TYPE_NULL;
		}
		g_free (base);
	} else {
		item->type = type_from_string (type);
	}
}

/* fallback to find something suitable for C locale */
static char *
try_english_key (GnomeDesktopItem *item, const char *key)
{
	char *str;
	char *locales[] = { "en_US", "en_GB", "en_AU", "en", NULL };
	int i;

	str = NULL;
	for (i = 0; locales[i] != NULL && str == NULL; i++) {
		str = g_strdup (lookup_locale (item, key, locales[i]));
	}
	if (str != NULL) {
		/* We need a 7-bit ascii string, so whack all
		 * above 127 chars */
		guchar *p;
		for (p = (guchar *)str; *p != '\0'; p++) {
			if (*p > 127)
				*p = '?';
		}
	}
	return str;
}


static void
sanitize (GnomeDesktopItem *item, const char *file)
{
	const char *type;

	type = lookup (item, GNOME_DESKTOP_ITEM_TYPE);

	/* understand old gnome style url exec thingies */
	if (type != NULL && strcmp (type, "URL") == 0) {
		const char *exec = lookup (item, GNOME_DESKTOP_ITEM_EXEC);
		set (item, GNOME_DESKTOP_ITEM_TYPE, "Link");
		if (exec != NULL) {
			/* Note, this must be in this order */
			set (item, GNOME_DESKTOP_ITEM_URL, exec);
			set (item, GNOME_DESKTOP_ITEM_EXEC, NULL);
		}
	}

	/* we make sure we have Name, Encoding and Version */
	if (lookup (item, GNOME_DESKTOP_ITEM_NAME) == NULL) {
		char *name = try_english_key (item, GNOME_DESKTOP_ITEM_NAME);
		/* If no name, use the basename */
		if (name == NULL)
			name = g_path_get_basename (file);
		g_hash_table_replace (item->main_hash,
				      g_strdup (GNOME_DESKTOP_ITEM_NAME), 
				      g_path_get_basename (file));
		item->keys = g_list_prepend
			(item->keys, g_strdup (GNOME_DESKTOP_ITEM_NAME));
	}
	if (lookup (item, GNOME_DESKTOP_ITEM_ENCODING) == NULL) {
		/* We store everything in UTF-8 so write that down */
		g_hash_table_replace (item->main_hash,
				      g_strdup (GNOME_DESKTOP_ITEM_ENCODING), 
				      g_strdup ("UTF-8"));
		item->keys = g_list_prepend
			(item->keys, g_strdup (GNOME_DESKTOP_ITEM_ENCODING));
	}
	if (lookup (item, GNOME_DESKTOP_ITEM_VERSION) == NULL) {
		/* this is the version that we follow, so write it down */
		g_hash_table_replace (item->main_hash,
				      g_strdup (GNOME_DESKTOP_ITEM_VERSION), 
				      g_strdup ("1.0"));
		item->keys = g_list_prepend
			(item->keys, g_strdup (GNOME_DESKTOP_ITEM_VERSION));
	}
}

enum {
	FirstBrace,
	OnSecHeader,
	IgnoreToEOL,
	IgnoreToEOLFirst,
	KeyDef,
	KeyDefOnKey,
	KeyValue
};

static GnomeDesktopItem *
ditem_load (const char *uri,
	    GError **error)
{
	ReadBuf *rb;
	int state;
	char CharBuffer [1024];
	char *next = CharBuffer;
	int c;
	Encoding encoding;
	GnomeDesktopItem *item;
	Section *cur_section = NULL;
	char *key = NULL;
	gboolean old_kde = FALSE;

	rb = readbuf_open (uri);
	
	if (rb == NULL) {
		g_set_error (error,
			     /* FIXME: better errors */
			     GNOME_DESKTOP_ITEM_ERROR,
			     GNOME_DESKTOP_ITEM_ERROR_CANNOT_OPEN,
			     _("Error reading file '%s': %s"),
			     uri, strerror (errno));
		return NULL;
	}

	encoding = get_encoding (uri, rb);
	if (encoding == ENCODING_UNKNOWN) {
		readbuf_close (rb);
		/* spec says, don't read this file */
		g_set_error (error,
			     GNOME_DESKTOP_ITEM_ERROR,
			     GNOME_DESKTOP_ITEM_ERROR_UNKNOWN_ENCODING,
			     _("Unknown encoding of: %s"),
			     uri);
		return NULL;
	}

	/* Rewind since get_encoding goes through the file */
	if ( ! readbuf_rewind (rb)) {
		readbuf_close (rb);
		/* spec says, don't read this file */
		g_set_error (error,
			     GNOME_DESKTOP_ITEM_ERROR,
			     GNOME_DESKTOP_ITEM_ERROR_CANNOT_OPEN,
			     _("Error rewinding file '%s': %s"),
			     uri);
		return NULL;
	}

	item = gnome_desktop_item_new ();
	item->modified = FALSE;

	/* Note: location and mtime are filled in by the new_from_file
	 * function since it has those values */

#define OVERFLOW (next == &CharBuffer [sizeof(CharBuffer)-1])
	
	state = FirstBrace;
	while ((c = readbuf_getc (rb)) != EOF) {
		if (c == '\r')		/* Ignore Carriage Return */
			continue;
		
		switch (state) {

		case OnSecHeader:
			if (c == ']' || OVERFLOW) {
				*next = '\0';
				next = CharBuffer;

				/* keys were inserted in reverse */
				if (cur_section != NULL &&
				    cur_section->keys != NULL) {
					cur_section->keys = g_list_reverse
						(cur_section->keys);
				}
				if (strcmp (CharBuffer,
					    "KDE Desktop Entry") == 0) {
					/* Main section */
					cur_section = NULL;
					old_kde = TRUE;
				} else if (strcmp (CharBuffer,
						   "Desktop Entry") == 0) {
					/* Main section */
					cur_section = NULL;
				} else {
					cur_section = g_new0 (Section, 1);
					cur_section->name =
						g_strdup (CharBuffer);
					cur_section->keys = NULL;
					item->sections = g_list_prepend
						(item->sections, cur_section);
				}
				state = IgnoreToEOL;
			} else {
				*next++ = c;
			}
			break;

		case IgnoreToEOL:
		case IgnoreToEOLFirst:
			if (c == '\n'){
				if (state == IgnoreToEOLFirst)
					state = FirstBrace;
				else
					state = KeyDef;
				next = CharBuffer;
			}
			break;

		case FirstBrace:
		case KeyDef:
		case KeyDefOnKey:
			if (c == '#') {
				if (state == FirstBrace)
					state = IgnoreToEOLFirst;
				else
					state = IgnoreToEOL;
				break;
			}

			if (c == '[' && state != KeyDefOnKey){
				state = OnSecHeader;
				next = CharBuffer;
				g_free (key);
				key = NULL;
				break;
			}
			/* On first pass, don't allow dangling keys */
			if (state == FirstBrace)
				break;
	    
			if ((c == ' ' && state != KeyDefOnKey) || c == '\t')
				break;
	    
			if (c == '\n' || OVERFLOW) { /* Abort Definition */
				next = CharBuffer;
				state = KeyDef;
                                break;
                        }
	    
			if (c == '=' || OVERFLOW){
				*next = '\0';

				g_free (key);
				key = g_strdup (CharBuffer);

				state = KeyValue;
				next = CharBuffer;
			} else {
				*next++ = c;
				state = KeyDefOnKey;
			}
			break;

		case KeyValue:
			if (OVERFLOW || c == '\n'){
				*next = '\0';

				insert_key (item, cur_section, encoding,
					    key, CharBuffer, old_kde);

				g_free (key);
				key = NULL;

				state = (c == '\n') ? KeyDef : IgnoreToEOL;
				next = CharBuffer;
			} else {
				*next++ = c;
			}
			break;

		} /* switch */
	
	} /* while ((c = getc_unlocked (f)) != EOF) */
	if (c == EOF && state == KeyValue) {
		*next = '\0';

		insert_key (item, cur_section, encoding,
			    key, CharBuffer, old_kde);

		g_free (key);
		key = NULL;
	}

	readbuf_close (rb);

#undef OVERFLOW

	/* keys were inserted in reverse */
	if (cur_section != NULL &&
	    cur_section->keys != NULL) {
		cur_section->keys = g_list_reverse (cur_section->keys);
	}
	/* keys were inserted in reverse */
	item->keys = g_list_reverse (item->keys);
	/* sections were inserted in reverse */
	item->sections = g_list_reverse (item->sections);

	/* Here we do a little bit of sanitazition */

	/* sanitize some things */
	sanitize (item, uri);

	/* make sure that we set up the type */
	setup_type (item, uri);

	return item;
}

static void vfs_printf (GnomeVFSHandle *handle,
			const char *format, ...) G_GNUC_PRINTF (2, 3);

static void
vfs_printf (GnomeVFSHandle *handle, const char *format, ...)
{
    va_list args;
    gchar *s;
    GnomeVFSFileSize bytes_written;

    va_start (args, format);
    s = g_strdup_vprintf (format, args);
    va_end (args);

    /* FIXME: what about errors */
    gnome_vfs_write (handle, s, strlen (s), &bytes_written);

    g_free (s);
}

static void 
dump_section (GnomeDesktopItem *item, GnomeVFSHandle *handle, Section *section)
{
	GList *li;

	vfs_printf (handle, "[%s]\n", section->name);
	for (li = section->keys; li != NULL; li = li->next) {
		const char *key = li->data;
		char *full = g_strdup_printf ("%s/%s", section->name, key);
		const char *value = g_hash_table_lookup (item->main_hash, full);
		if (value != NULL) {
			char *val = escape_string_and_dup (value);
			vfs_printf (handle, "%s=%s\n", key, val);
			g_free (val);
		}
		g_free (full);
	}
}

static gboolean
ditem_save (GnomeDesktopItem *item, const char *uri, GError **error)
{
	GList *li;
	GnomeVFSHandle *handle;

	if (gnome_vfs_open (&handle, uri, 
			    GNOME_VFS_OPEN_WRITE) != GNOME_VFS_OK) {
		g_set_error (error,
			     /* FIXME: better errors */
			     GNOME_DESKTOP_ITEM_ERROR,
			     GNOME_DESKTOP_ITEM_ERROR_CANNOT_OPEN,
			     _("Error writing file '%s': %s"),
			     uri, strerror (errno));
		return FALSE;
	}

	vfs_printf (handle, "[Desktop Entry]\n");
	for (li = item->keys; li != NULL; li = li->next) {
		const char *key = li->data;
		const char *value = g_hash_table_lookup (item->main_hash, key);
		if (value != NULL) {
			char *val = escape_string_and_dup (value);
			vfs_printf (handle, "%s=%s\n", key, val);
			g_free (val);
		}
	}

	if (item->sections != NULL)
		vfs_printf (handle, "\n");

	for (li = item->sections; li != NULL; li = li->next) {
		Section *section = li->data;

		/* Don't write empty sections */
		if (section->keys == NULL)
			continue;

		dump_section (item, handle, section);

		if (li->next != NULL)
			vfs_printf (handle, "\n");
	}

	gnome_vfs_close (handle);

	return TRUE;
}

static gpointer
_gnome_desktop_item_copy (gpointer boxed)
{
	return gnome_desktop_item_copy (boxed);
}

static void
_gnome_desktop_item_free (gpointer boxed)
{
	gnome_desktop_item_unref (boxed);
}

GType
gnome_desktop_item_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		type = g_boxed_type_register_static ("GnomeDesktopItem",
						     _gnome_desktop_item_copy,
						     _gnome_desktop_item_free);
	}

	return type;
}

GQuark
gnome_desktop_item_error_quark (void)
{
	static GQuark q = 0;
	if (q == 0)
		q = g_quark_from_static_string ("g-file-error-quark");

	return q;
}
