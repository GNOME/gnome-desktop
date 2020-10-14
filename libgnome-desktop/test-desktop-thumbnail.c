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

static gboolean
thumbnail_file (GnomeDesktopThumbnailFactory *factory,
		const char *in_path,
		const char *out_path)
{
	GdkPixbuf *pixbuf;
	char *content_type;
	g_autoptr(GFile) file = NULL;
	g_autofree char *path = NULL;
	g_autofree char *uri = NULL;

	file = g_file_new_for_commandline_arg (in_path);
	path = g_file_get_path (file);
	if (!path) {
		g_warning ("Could not get path for %s", in_path);
		return FALSE;
	}

	content_type = g_content_type_guess (path, NULL, 0, NULL);
	uri = g_file_get_uri (file);
	pixbuf = gnome_desktop_thumbnail_factory_generate_thumbnail (factory, uri, content_type);
	g_free (content_type);

	if (pixbuf == NULL) {
		g_warning ("gnome_desktop_thumbnail_factory_generate_thumbnail() failed to generate a thumbnail for %s", in_path);
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
	GnomeDesktopThumbnailFactory *factory;

	if (argc != 3) {
		g_print ("Usage: %s [INPUT FILE] [OUTPUT FILE]\n", argv[0]);
		return 1;
	}

	factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE);
	if (!thumbnail_file (factory, argv[1], argv[2]))
		return 1;

	return 0;
}
