/*
 * Copyright (C) 2012,2017 Red Hat, Inc.
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

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-desktop-thumbnail.h>
#include <gtk/gtk.h>

int main (int argc, char **argv)
{
	GdkPixbuf *pixbuf;
	GnomeDesktopThumbnailFactory *factory;
	GtkWidget *window, *image;
	char *content_type;

	gtk_init (&argc, &argv);

	if (argc < 2) {
		g_print ("Usage: %s FILE...\n", argv[0]);
		return 1;
	}

	content_type = g_content_type_guess (argv[1], NULL, 0, NULL);
	factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE);
	pixbuf = gnome_desktop_thumbnail_factory_generate_thumbnail (factory, argv[1], content_type);
	g_free (content_type);

	if (pixbuf == NULL) {
		g_warning ("gnome_desktop_thumbnail_factory_generate_thumbnail() failed to generate a thumbnail for %s", argv[1]);
		return 1;
	}

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	image = gtk_image_new_from_pixbuf (pixbuf);
	gtk_container_add (GTK_CONTAINER (window), image);
	gtk_widget_show_all (window);

	gtk_main ();

	return 0;
}
