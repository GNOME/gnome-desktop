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
};

struct _GnomeRRLabelerClass {
	GObjectClass parent_class;
};

G_DEFINE_TYPE (GnomeRRLabeler, gnome_rr_labeler, G_TYPE_OBJECT);
