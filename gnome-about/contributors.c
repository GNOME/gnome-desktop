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

#include <glib.h>
#include <glib/gi18n.h>

#include <libgnome/gnome-program.h>

#include "contributors.h"

static int    num_static_contributors;
static int    num_online_contributors;
static int    num_total_contributors;
static int   *contrib_order = NULL;

#define CONTRIBUTORS_FILE "contributors.list"
static char **static_contributors = NULL;
static char **online_contributors = NULL;
static char  *translated_contributors[] = {
	N_("The Mysterious GEGL"),
	N_("The Squeaky Rubber GNOME"),
	N_("Wanda The GNOME Fish")
};

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
	if (!g_file_get_contents (CONTRIBUTORS_FILE, &contents, NULL, &error)) {
		g_printerr ("Cannot read list of contributors: %s\n",
			    error->message);
		g_error_free (error);
		return;
	}

	*contributors_nb = 0;
	contributors_l = NULL;
	contributors_array = g_strsplit (contents, "\n", -1);
	/* we could stop here, and directly use contributors_array, but we need
	 * to check that the strings are valid UTF-8 */

	g_free (contents);

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

static void
contributors_online_read (void)
{
	/* TODO: implement downloading and caching the list of Foundation
	 * members */
	num_online_contributors = 0;
	num_total_contributors += num_online_contributors;
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
