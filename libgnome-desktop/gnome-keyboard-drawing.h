/*
 * Copyright (C) 2006 Sergey V. Udaltsov <svu@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _GNOME_KEYBOARD_DRAWING_H
#define _GNOME_KEYBOARD_DRAWING_H

#ifndef GNOME_DESKTOP_USE_UNSTABLE_API
#error    This is unstable API. You must define GNOME_DESKTOP_USE_UNSTABLE_API before including gnome-xkb-info.h
#endif

#include <gtk/gtk.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XKBgeom.h>
#include <libxklavier/xklavier.h>

G_BEGIN_DECLS

#define GNOME_TYPE_KEYBOARD_DRAWING (gnome_keyboard_drawing_get_type ())
#define GNOME_KEYBOARD_DRAWING(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_KEYBOARD_DRAWING, GnomeKeyboardDrawing))
#define GNOME_KEYBOARD_DRAWING_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_TYPE_KEYBOARD_DRAWING, GnomeKeyboardDrawingClass))
#define GNOME_IS_KEYBOARD_DRAWING(obj) G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_TYPE_KEYBOARD_DRAWING)

typedef struct _GnomeKeyboardDrawing GnomeKeyboardDrawing;
typedef struct _GnomeKeyboardDrawingClass GnomeKeyboardDrawingClass;

GType gnome_keyboard_drawing_get_type (void) G_GNUC_CONST;

GtkWidget *gnome_keyboard_drawing_new (void);

void gnome_keyboard_drawing_set_layout   (GnomeKeyboardDrawing *kbdrawing,
                                          const char           *layout,
                                          const char           *variant);

void gnome_keyboard_drawing_print (GnomeKeyboardDrawing *drawing,
                                   GtkWindow            *parent_window,
                                   const gchar          *description);

G_END_DECLS

#endif	/* #ifndef _GNOME_KEYBOARD_DRAWING_H */
