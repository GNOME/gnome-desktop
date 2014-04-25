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
#include <X11/extensions/Xrandr.h>
#include <X11/Xatom.h>

/**
 * get_property:
 **/
static guint8 *
get_property (Display *dpy,
	      RROutput output,
	      Atom atom,
	      gsize *len)
{
	unsigned char *prop;
	int actual_format;
	unsigned long nitems, bytes_after;
	Atom actual_type;
	guint8 *result = NULL;

	XRRGetOutputProperty (dpy, output, atom,
			      0, 100, False, False,
			      AnyPropertyType,
			      &actual_type, &actual_format,
			      &nitems, &bytes_after, &prop);

	if (actual_type == XA_INTEGER && actual_format == 8) {
		result = g_memdup (prop, nitems);
		if (len)
			*len = nitems;
	}
	XFree (prop);
	return result;
}

static void
print_output (GnomeRROutput *output, const char *message)
{
	Atom edid_atom;
	Display *display;
	gsize len = 0;
	guint8 *result = NULL;

	display = GDK_SCREEN_XDISPLAY (gdk_screen_get_default ());

	g_print ("[%s]", gnome_rr_output_get_name (output));
	if (message)
		g_print (" (%s)", message);
	g_print ("\n");
	g_print ("\tconnected: %i\n", gnome_rr_output_is_connected (output));
	g_print ("\tlaptop: %i\n", gnome_rr_output_is_laptop (output));
	g_print ("\tprimary: %i\n", gnome_rr_output_get_is_primary (output));
	g_print ("\tid: %i\n", gnome_rr_output_get_id (output));

	/* get EDID (first try) */
	edid_atom = XInternAtom (display, "EDID", FALSE);
	result = get_property (display,
			       gnome_rr_output_get_id (output),
			       edid_atom,
			       &len);
	if (result != NULL) {
		g_print ("\tedid: %" G_GSIZE_FORMAT " bytes [%i:%i:%i:%i]\n",
			 len, result[0], result[1],
			 result[2], result[3]);
		g_free (result);
	}

	/* get EDID (second try) */
	edid_atom = XInternAtom (display, "EDID_DATA", FALSE);
	result = get_property (display,
			       gnome_rr_output_get_id (output),
			       edid_atom,
			       &len);
	if (result != NULL) {
		g_print ("\tedid2: %" G_GSIZE_FORMAT " bytes [%i:%i:%i:%i]\n",
			 len, result[0], result[1],
			 result[2], result[3]);
		g_free (result);
	}
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
