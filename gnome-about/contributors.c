/*
 * contributors.c:
 *
 * Copyright (C) 2007 Vincent Untz
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors:
 *	Vincent Untz <vuntz@gnome.org>
 *
 * generate_randomness() comes from gnome-about.c
 */


#include <config.h>

#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <libgnome/gnome-program.h>
#include <libgnome/gnome-util.h>

#include <libgnomevfs/gnome-vfs-async-ops.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include "contributors.h"

static int    num_static_contributors;
static int    num_online_contributors;
static int    num_total_contributors;
static int   *contrib_order = NULL;

#define CONTRIBUTORS_FILE "contributors.list"
#define ONLINE_CONTRIBUTORS_URI "http://api.gnome.org/gnome-about/foundation-members"
static char **static_contributors = NULL;
static char **online_contributors = NULL;
static char  *translated_contributors[] = {
	N_("The Mysterious GEGL"),
	N_("The Squeaky Rubber GNOME"),
	N_("Wanda The GNOME Fish")
};

struct OnlineXfer {
	GnomeVFSAsyncHandle *handle;
	char *file;
} online_xfer_data = { NULL, NULL } ;

static void
generate_randomness (void)
{
	int i;
	int random_number;
	int tmp;
	GRand *generator;

	if (contrib_order != NULL)
		g_free (contrib_order);

	generator = g_rand_new ();

	contrib_order = g_malloc (num_total_contributors * sizeof (int));

	for (i = 0; i < num_total_contributors; i++)
		contrib_order[i]=i;

	for (i = 0; i < num_total_contributors; i++) {
		random_number = g_rand_int_range (generator, i, 
						  num_total_contributors);
		tmp = contrib_order[i];
		contrib_order[i] = contrib_order[random_number];
		contrib_order[random_number] = tmp;
	}

	g_rand_free (generator);
}

const char *
contributors_get (int i)
{
	int real;

	g_assert (contrib_order != NULL);

	real = i % (num_total_contributors);

	if (contrib_order[real] < G_N_ELEMENTS (translated_contributors))
		return _(translated_contributors[contrib_order[real]]);
	else if (contrib_order[real] < G_N_ELEMENTS (translated_contributors)
				       + num_static_contributors)
		return static_contributors[contrib_order[real] -
					   G_N_ELEMENTS (translated_contributors)];
	else if (contrib_order[real] < G_N_ELEMENTS (translated_contributors)
				       + num_static_contributors
				       + num_online_contributors)
		return online_contributors[contrib_order[real]
					   - G_N_ELEMENTS (translated_contributors)
					   - num_static_contributors];
	else
		g_assert_not_reached ();
}


static void
contributors_read_from_file (const char   *file,
			     char       ***contributors,
			     int          *contributors_nb)
{
	char    *contents;
	GError  *error;
	int      i;
	char   **contributors_array;
	GList   *contributors_l;
	GList   *l;

	g_assert (contributors != NULL);
	g_assert (contributors_nb != NULL);
	g_assert (*contributors == NULL);

	error = NULL;
	if (!g_file_get_contents (file, &contents, NULL, &error)) {
		g_printerr ("Cannot read list of contributors: %s\n",
			    error->message);
		g_error_free (error);
		return;
	}

	*contributors_nb = 0;
	*contributors = NULL;

	contributors_l = NULL;
	contributors_array = g_strsplit (contents, "\n", -1);
	/* we could stop here, and directly use contributors_array, but we need
	 * to check that the strings are valid UTF-8 */

	g_free (contents);

	if (contributors_array[0] == NULL ||
	    strcmp (contributors_array[0],
		    "# gnome-about contributors - format 1") != 0) {
		g_strfreev (contributors_array);
		return;
	}

	for (i = 0; contributors_array[i] != NULL; i++) {
		if (!g_utf8_validate (contributors_array[i], -1, NULL) ||
		    contributors_array[i][0] == '\0' ||
		    contributors_array[i][0] == '#')
			continue;

		contributors_l = g_list_prepend (contributors_l,
						 g_strdup (contributors_array[i]));
		(*contributors_nb)++;
	}
	g_strfreev (contributors_array);

	*contributors = g_malloc ((*contributors_nb + 1) * sizeof (char *));

	for (i = 0, l = contributors_l; l != NULL; l = l->next, i++)
		(*contributors)[i] = l->data;
	g_list_free (contributors_l);

	/* we want to use g_strfreev */
	(*contributors)[i] = NULL;
}

static void
contributors_static_read (void)
{
	char *file;

	file = gnome_program_locate_file (NULL,
					  GNOME_FILE_DOMAIN_APP_DATADIR,
					  CONTRIBUTORS_FILE,
					  TRUE, NULL);
	if (!file)
		return;

	num_online_contributors = 0;
	contributors_read_from_file (file,
				     &static_contributors,
				     &num_static_contributors);
	num_total_contributors += num_static_contributors;

	g_free (file);
}

static gboolean
contributors_ensure_dir_exists (const char *dir)
{
	if (g_file_test (dir, G_FILE_TEST_IS_DIR))
		return TRUE;

	if (g_file_test (dir, G_FILE_TEST_EXISTS)) {
		g_printerr ("%s is not a directory.", dir);
		return FALSE;
	}

	if (g_mkdir_with_parents (dir, 0755) != 0) {
		g_warning ("Failed to create directory %s.", dir);
		return FALSE;
	}

	return TRUE;
}

static int
contributors_async_xfer_progress (GnomeVFSAsyncHandle      *handle,
				  GnomeVFSXferProgressInfo *progress_info,
				  gpointer                  data)
{
	struct OnlineXfer *xfer_data = data;

	switch (progress_info->status) {
	case GNOME_VFS_XFER_PROGRESS_STATUS_OK:
		if (progress_info->phase != GNOME_VFS_XFER_PHASE_COMPLETED)
			return 1;
		break;
	case GNOME_VFS_XFER_PROGRESS_STATUS_VFSERROR:
		return GNOME_VFS_XFER_ERROR_ACTION_ABORT;
	case GNOME_VFS_XFER_PROGRESS_STATUS_OVERWRITE:
		return GNOME_VFS_XFER_OVERWRITE_ACTION_REPLACE;
	case GNOME_VFS_XFER_PROGRESS_STATUS_DUPLICATE:
		return 0;
	default:
		g_warning ("Unknown GnomeVFSXferProgressStatus %d",
			   progress_info->status);
		return 0;
	}

	/* we're done with the xfer */

	num_online_contributors = 0;
	contributors_read_from_file (xfer_data->file,
				     &online_contributors,
				     &num_online_contributors);
	num_total_contributors += num_online_contributors;

	/* include the new contributors in the data we'll give */
	generate_randomness ();

	xfer_data->handle = NULL;
	g_free (xfer_data->file);
	xfer_data->file = NULL;

	return 0;
}

static void
contributors_online_read (void)
{
	char *dir;
	char *file;
	gboolean need_to_download;

	//TODO: remove this when we get our api.gnome.org page
	return;

	dir = g_build_filename (gnome_user_dir_get (), "gnome-about", NULL);

	if (!contributors_ensure_dir_exists (dir)) {
		g_free (dir);
		return;
	}

	file = g_build_filename (dir, CONTRIBUTORS_FILE, NULL);
	g_free (dir);

	need_to_download = TRUE;
	if (g_file_test (file, G_FILE_TEST_EXISTS)) {
		struct stat stat_data;

		if (g_stat (file, &stat_data) == 0) {
			GDate *validity_date;
			GDate *modified;

			validity_date = g_date_new ();
			modified = g_date_new ();

			g_date_set_time_t (validity_date, time (NULL));
			g_date_subtract_days (validity_date, 14);
			g_date_set_time_t (modified, stat_data.st_mtime);

			if (g_date_compare (modified, validity_date) >= 0)
				need_to_download = FALSE;

			g_date_free (modified);
			g_date_free (validity_date);
		} else {
			g_printerr ("Cannot stat %s: %s", file, strerror (errno));
		}

		if (need_to_download)
			g_unlink (file);
	}

	if (need_to_download) {
		char  *dest_file;
		GList *src_list;
		GList *dest_list;
		GnomeVFSURI *src_uri;
		GnomeVFSURI *dest_uri;

		src_uri = gnome_vfs_uri_new (ONLINE_CONTRIBUTORS_URI);
		src_list = g_list_append (NULL, src_uri);

		dest_file = gnome_vfs_get_uri_from_local_path (file);
		dest_uri = gnome_vfs_uri_new (dest_file);
		dest_list = g_list_append (NULL, dest_uri);
		g_free (dest_file);

		online_xfer_data.file = g_strdup (file);

		gnome_vfs_async_xfer (&online_xfer_data.handle,
				      src_list, dest_list,
				      GNOME_VFS_XFER_DEFAULT,
				      GNOME_VFS_XFER_ERROR_MODE_ABORT,
				      GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE,
				      GNOME_VFS_PRIORITY_DEFAULT,
				      contributors_async_xfer_progress,
				      &online_xfer_data,
				      NULL, NULL);

		g_list_free (src_list);
		g_list_free (dest_list);
		gnome_vfs_uri_unref (src_uri);
		gnome_vfs_uri_unref (dest_uri);
	} else {
		num_online_contributors = 0;
		contributors_read_from_file (file,
					     &online_contributors,
					     &num_online_contributors);
		num_total_contributors += num_online_contributors;
	}

	g_free (file);
}

void
contributors_free (void)
{
	if (contrib_order)
		g_free (contrib_order);
	contrib_order = NULL;

	if (static_contributors)
		g_strfreev (static_contributors);
	static_contributors = NULL;

	if (online_xfer_data.handle)
		gnome_vfs_async_cancel (online_xfer_data.handle);
	online_xfer_data.handle = NULL;

	if (online_xfer_data.file)
		g_free (online_xfer_data.file);
	online_xfer_data.file = NULL;

	if (online_contributors)
		g_strfreev (online_contributors);
	online_contributors = NULL;
}

void
contributors_init (void)
{
	num_static_contributors = 0;
	num_online_contributors = 0;
	num_total_contributors = G_N_ELEMENTS (translated_contributors);

	contributors_static_read ();
	contributors_online_read ();

	generate_randomness ();
}
