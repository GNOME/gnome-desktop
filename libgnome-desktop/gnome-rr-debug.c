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

static void
print_output (GnomeRROutput *output, const char *message)
{
	gsize len = 0;
	const guint8 *result = NULL;
	int width_mm, height_mm;

	g_print ("[%s]", gnome_rr_output_get_name (output));
	if (message)
		g_print (" (%s)", message);
	g_print ("\n");
	g_print ("\tconnected: %i\n", 1);
	g_print ("\tbuiltin (laptop): %i\n", gnome_rr_output_is_builtin_display (output));
	g_print ("\tprimary: %i\n", gnome_rr_output_get_is_primary (output));
	g_print ("\tid: %i\n", gnome_rr_output_get_id (output));
	gnome_rr_output_get_physical_size (output, &width_mm, &height_mm);
	g_print ("\tdimensions: %ix%i\n", width_mm, height_mm);
	g_print ("\tbacklight: %i (min step: %i)\n",
		 gnome_rr_output_get_backlight (output),
		 gnome_rr_output_get_min_backlight_step (output));


	/* get EDID */
        result = gnome_rr_output_get_edid_data (output, &len);
	if (result != NULL) {
		char *vendor, *product, *serial;

		g_print ("\tedid: %" G_GSIZE_FORMAT " bytes [%i:%i:%i:%i]\n",
			 len, result[0], result[1],
			 result[2], result[3]);
		gnome_rr_output_get_ids_from_edid (output, &vendor, &product, &serial);
		g_print ("\tvendor: '%s' product: '%s' serial: '%s'\n",
			 vendor, product, serial);
		g_free (vendor);
		g_free (product);
		g_free (serial);
	}

	g_print ("\n");
}

static void
screen_changed (GnomeRRScreen *screen, gpointer user_data)
{
	g_print ("Screen changed\n");
}

static void
output_disconnected (GnomeRRScreen *screen, GnomeRROutput *output, gpointer user_data)
{
	print_output (output, "disconnected");
}

static void
output_connected (GnomeRRScreen *screen, GnomeRROutput *output, gpointer user_data)
{
	print_output (output, "connected");
}

/**
 * main:
 **/
int
main (int argc, char *argv[])
{
	GError *error = NULL;
	GnomeRROutput **outputs;
	GnomeRRScreen *screen;
	guint i;

	gtk_init (&argc, &argv);
	screen = gnome_rr_screen_new (gdk_screen_get_default (), &error);
	if (screen == NULL) {
		g_warning ("failed to get screen: %s", error->message);
		g_error_free (error);
		goto out;
	}
	outputs = gnome_rr_screen_list_outputs (screen);
	for (i = 0; outputs[i] != NULL; i++)
		print_output (outputs[i], NULL);

	g_signal_connect (screen, "changed", G_CALLBACK (screen_changed), NULL);
	g_signal_connect (screen, "output-disconnected", G_CALLBACK (output_disconnected), NULL);
	g_signal_connect (screen, "output-connected", G_CALLBACK (output_connected), NULL);

	gtk_main ();

out:
	g_object_unref (screen);
	return 0;
}
