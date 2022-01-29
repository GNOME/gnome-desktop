/*
 * Copyright (C) 2012,2017,2020 Red Hat, Inc.
 *
 * This file is part of the Gnome Library.
 *
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Authors: Bastien Nocera <hadess@hadess.net>
 */

#include <locale.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-desktop-thumbnail.h>

static gboolean
thumbnail_file (GnomeDesktopThumbnailFactory *factory,
		const char *in_path,
		const char *out_path)
{
	g_autoptr(GdkPixbuf) pixbuf = NULL;
	g_autofree char *content_type = NULL;
	g_autoptr(GFile) file = NULL;
	g_autofree char *path = NULL;
	g_autofree char *uri = NULL;
	g_autoptr(GError) error = NULL;

	file = g_file_new_for_commandline_arg (in_path);
	path = g_file_get_path (file);
	if (!path) {
		g_warning ("Could not get path for %s", in_path);
		return FALSE;
	}

	content_type = g_content_type_guess (path, NULL, 0, NULL);
	uri = g_file_get_uri (file);
	pixbuf = gnome_desktop_thumbnail_factory_generate_thumbnail (factory, uri, content_type, NULL, &error);

	if (error) {
		g_warning ("gnome_desktop_thumbnail_factory_generate_thumbnail() failed to generate a thumbnail for %s: %s", in_path, error->message);
		return FALSE;
	}

	if (!gdk_pixbuf_save (pixbuf, out_path, "png", NULL, NULL)) {
		g_warning ("gdk_pixbuf_save failed for %s", in_path);
		return FALSE;
	}

	return TRUE;
}

int main (int argc, char **argv)
{
	g_autoptr(GnomeDesktopThumbnailFactory) factory = NULL;
	g_autoptr(GTimer) timer = NULL;
	gint num_iterations = 1;
	gboolean shared_factory = FALSE;
	gboolean show_timer = FALSE;
	char **filenames = NULL;
	int ret = 0;
	g_autoptr(GOptionContext) option_context = NULL;
	g_autoptr(GError) error = NULL;
	const GOptionEntry options[] = {
		{ "shared-factory", 's', 0, G_OPTION_ARG_NONE, &shared_factory, "Whether to share the Thumbnail Factory (default: off)", NULL },
		{ "num-iterations", 'n', 0, G_OPTION_ARG_INT, &num_iterations, "Number of times to run thumbnail operation (default: 1)", NULL },
		{ "show-timer", 't', 0, G_OPTION_ARG_NONE, &show_timer, "Whether to show time statistics for operations", NULL },
		{ G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, "[INPUT FILE] [OUTPUT FILE]", NULL },
		{ NULL}
	};
	int i;
	gdouble first_iter_elapsed = 0.0;
	gdouble following_elapsed = 0.0;

	setlocale (LC_ALL, "");
	option_context = g_option_context_new ("");
	g_option_context_add_main_entries (option_context, options, NULL);

	ret = g_option_context_parse (option_context, &argc, &argv, &error);
	if (!ret) {
		g_print ("Failed to parse arguments: %s", error->message);
		return EXIT_FAILURE;
	}

	if (filenames == NULL ||
	    g_strv_length (filenames) != 2 ||
	    num_iterations < 1) {
		g_autofree char *help = NULL;
		help = g_option_context_get_help (option_context, TRUE, NULL);
		g_print ("%s", help);
		return 1;
	}

	timer = g_timer_new ();

	for (i = 0; i < num_iterations; i++) {
		g_timer_start (timer);

		if (factory == NULL)
			factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE);

		if (!thumbnail_file (factory, filenames[0], filenames[1]))
			return 1;

		if (i == 0)
			first_iter_elapsed = g_timer_elapsed (timer, NULL);
		else
			following_elapsed += g_timer_elapsed (timer, NULL);

		if (!shared_factory)
			g_clear_object (&factory);
	}

	if (show_timer) {
		if (num_iterations == 1) {
			g_print ("Elapsed time: %d msec\n",
				 (int) (first_iter_elapsed * 1000));
		} else if (num_iterations > 1) {
			g_print ("First iteration: %d msec\n",
				 (int) (first_iter_elapsed * 1000));
			g_print ("Average time: %d msec (%d iterations)\n",
				 (int) (following_elapsed * 1000 / (num_iterations - 1)),
				 num_iterations - 1);
		} else {
			g_assert_not_reached ();
		}
	}

	return 0;
}
