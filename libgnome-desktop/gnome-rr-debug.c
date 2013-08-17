/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Richard Hughes <richard@hughsie.com>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#define GNOME_DESKTOP_USE_UNSTABLE_API

#include <gdk/gdkx.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <libgnome-desktop/gnome-rr.h>

int
main (int argc, char *argv[])
{
	GError *error = NULL;
	GnomeRROutput **outputs;
	GnomeRRScreen *screen;
	gsize len = 0;
	const guint8 *result = NULL;
	guint i;

	gtk_init (&argc, &argv);
	screen = gnome_rr_screen_new (gdk_screen_get_default (), &error);
	if (screen == NULL) {
		g_warning ("failed to get screen: %s", error->message);
		g_error_free (error);
		goto out;
	}
	outputs = gnome_rr_screen_list_outputs (screen);
	for (i = 0; outputs[i] != NULL; i++) {
		g_print ("[%s]\n", gnome_rr_output_get_name (outputs[i]));
		g_print ("\tconnected: %i\n", 1);
		g_print ("\tbuilt-in: %i\n", gnome_rr_output_is_builtin_display (outputs[i]));
		g_print ("\tprimary: %i\n", gnome_rr_output_get_is_primary (outputs[i]));
		g_print ("\tid: %i\n", gnome_rr_output_get_id (outputs[i]));

		/* get EDID (first try) */
                result = gnome_rr_output_get_edid_data (outputs[i], &len);
		if (result != NULL) {
			g_print ("\tedid: %" G_GSIZE_FORMAT " bytes [%i:%i:%i:%i]\n",
				 len, result[0], result[1],
				 result[2], result[3]);
		}
	}
out:
	g_object_unref (screen);
	return 0;
}
