/* gnome-rr-labeler.c - Utility to label monitors to identify them
 * while they are being configured.
 *
 * Copyright 2008, Novell, Inc.
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
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 * 
 * Author: Federico Mena-Quintero <federico@novell.com>
 */

#define GNOME_DESKTOP_USE_UNSTABLE_API

#include "libgnomeui/gnome-rr-labeler.h"
#include <gtk/gtk.h>

struct _GnomeRRLabeler {
	GObject parent;

	GnomeRRScreen *screen;
};

struct _GnomeRRLabelerClass {
	GObjectClass parent_class;
};

G_DEFINE_TYPE (GnomeRRLabeler, gnome_rr_labeler, G_TYPE_OBJECT);

static void gnome_rr_labeler_finalize (GObject *object);

static void
gnome_rr_labeler_init (GnomeRRLabeler *labeler)
{
	/* nothing */
}

static void
gnome_rr_labeler_class_init (GnomeRRLabelerClass *class)
{
	GObjectClass *object_class;

	object_class = (GObjectClass *) class;

	object_class->finalize = gnome_rr_labeler_finalize;
}

static void
gnome_rr_labeler_finalize (GObject *object)
{
	/* FIXME: unref labeler->screen */
}

GnomeRRLabeler *
gnome_rr_labeler_new (GnomeRRScreen *screen)
{
	GnomeRRLabeler *labeler;

	g_return_val_if_fail (screen != NULL, NULL);

	labeler = g_object_new (GNOME_TYPE_RR_LABELER, NULL);
	labeler->screen = screen;

	return labeler;
}
