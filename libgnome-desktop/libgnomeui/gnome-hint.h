/* gnome-hint.h - 

   This file is part of the Gnome Library.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
   
   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#ifndef __GNOME_HINT_H__
#define __GNOME_HINT_H__

#ifndef GNOME_DISABLE_DEPRECATED

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <glib.h>
#include <gtk/gtk.h>

#define GNOME_TYPE_HINT			(gnome_hint_get_type ())
#define GNOME_HINT(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_HINT, GnomeHint))
#define GNOME_HINT_CLASS(klass)		(G_TYPE_CHECK_CAST ((klass), GNOME_TYPE_HINT))
#define GNOME_IS_HINT(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_TYPE_HINT))
#define GNOME_IS_HINT_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_HINT))
#define GNOME_HINT_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GNOME_TYPE_HINT, GnomeHintClass))

typedef struct _GnomeHint		GnomeHint;
typedef struct _GnomeHintClass		GnomeHintClass;
typedef struct _GnomeHintPrivate	GnomeHintPrivate;

struct _GnomeHint
{
	GtkDialog parent_instance;

	GnomeHintPrivate *_priv;
};

struct _GnomeHintClass
{
	GtkDialogClass parent_class;
};

GType gnome_hint_get_type (void);
GtkWidget * gnome_hint_new (const gchar *hintfile,
			    const gchar *title,
			    const gchar *background_image,
			    const gchar *logo_image,
			    const gchar *startupkey);

#ifdef __cplusplus
}
#endif

#endif /* GNOME_DISABLE_DEPRECATED */

#endif
