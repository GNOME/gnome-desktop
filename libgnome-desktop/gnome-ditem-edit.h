/*  -*- Mode: C; c-set-style: linux; indent-tabs-mode: nil; c-basic-offset: 8 -*-

   Copyright (C) 1999 Free Software Foundation
   All rights reserved.
    
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
   Boston, MA 02111-1307, USA.

   GnomeDItemEdit Developers: Havoc Pennington, based on code by John Ellis
*/
/*
  @NOTATION@
*/

/******************** NOTE: this is an object, not a widget.
 ********************       You must supply a GtkNotebook.
 The reason for this is that you might want this in a property box, 
 or in your own notebook. Look at the test program at the bottom 
 of gnome-dentry-edit.c for a usage example.
 */

#ifndef GNOME_DITEM_EDIT_H
#define GNOME_DITEM_EDIT_H

#include <gtk/gtk.h>
#include "libgnome/gnome-defs.h"
#include "libgnome/gnome-ditem.h"

BEGIN_GNOME_DECLS

typedef struct _GnomeDItemEdit        GnomeDItemEdit;
typedef struct _GnomeDItemEditPrivate GnomeDItemEditPrivate;
typedef struct _GnomeDItemEditClass   GnomeDItemEditClass;

#define GNOME_TYPE_DITEM_EDIT            (gnome_ditem_edit_get_type ())
#define GNOME_DITEM_EDIT(obj)            (GTK_CHECK_CAST ((obj), GNOME_TYPE_DITEM_EDIT, GnomeDItemEdit))
#define GNOME_DITEM_EDIT_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GNOME_TYPE_DITEM_EDIT, GnomeDItemEditClass))
#define GNOME_IS_DITEM_EDIT(obj)         (GTK_CHECK_TYPE ((obj), GNOME_TYPE_DITEM_EDIT))
#define GNOME_IS_DITEM_EDIT_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_DITEM_EDIT))
#define GNOME_DITEM_EDIT_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS ((obj), GNOME_TYPE_DITEM_EDIT, GnomeDItemEditClass))

struct _GnomeDItemEdit {
        GtkObject object;
  
	/*< private >*/
	GnomeDItemEditPrivate *_priv;
};

struct _GnomeDItemEditClass {
        GtkObjectClass parent_class;

        /* Any information changed */
        void (* changed)         (GnomeDItemEdit * gee);
        /* These two more specific signals are provided since they 
           will likely require a display update */
        /* The icon in particular has changed. */
        void (* icon_changed)    (GnomeDItemEdit * gee);
        /* The name of the item has changed. */
        void (* name_changed)    (GnomeDItemEdit * gee);
};

guint             gnome_ditem_edit_get_type     (void) G_GNUC_CONST;

/*create a new ditem and get the children using the below functions 
  or use the utility new_notebook below*/
GtkObject *       gnome_ditem_edit_new          (void);

/* the main page */
GtkWidget *       gnome_ditem_edit_child1	(GnomeDItemEdit  *dee);
/* the DND page */
GtkWidget *       gnome_ditem_edit_child2	(GnomeDItemEdit  *dee);
/* the advanced page */
GtkWidget *       gnome_ditem_edit_child3	(GnomeDItemEdit  *dee);

/* Create a new edit in this notebook - appends two pages to the 
   notebook. */
GtkObject *       gnome_ditem_edit_new_notebook (GtkNotebook      *notebook);
void              gnome_ditem_edit_clear        (GnomeDItemEdit   *dee);



/* The GnomeDItemEdit does not store a ditem, and it does not keep
   track of the location field of GnomeDesktopItem which will always
   be NULL. */

/* Make the display reflect ditem at path */
void              gnome_ditem_edit_load_file    (GnomeDItemEdit   *dee,
                                                 const gchar      *path);

/* Copy the contents of this ditem into the display */
void              gnome_ditem_edit_set_ditem    (GnomeDItemEdit   *dee,
                                                 GnomeDesktopItem *ditem);

/* Generate a ditem based on the contents of the display */
GnomeDesktopItem *gnome_ditem_edit_get_ditem    (GnomeDItemEdit   *dee);



/* Return an allocated string, you need to g_free it. */
gchar *           gnome_ditem_edit_get_icon     (GnomeDItemEdit   *dee);
gchar *           gnome_ditem_edit_get_name     (GnomeDItemEdit   *dee);



/* These are accessor functions for the widgets that make up the
   GnomeDItemEdit editting areas. */
GtkWidget *       gnome_ditem_edit_get_name_entry    (GnomeDItemEdit   *dee);
GtkWidget *       gnome_ditem_edit_get_comment_entry (GnomeDItemEdit   *dee);
GtkWidget *       gnome_ditem_edit_get_exec_entry    (GnomeDItemEdit   *dee);
GtkWidget *       gnome_ditem_edit_get_tryexec_entry (GnomeDItemEdit   *dee);
GtkWidget *       gnome_ditem_edit_get_doc_entry     (GnomeDItemEdit   *dee);
GtkWidget *       gnome_ditem_edit_get_icon_entry    (GnomeDItemEdit   *dee);
GtkWidget *       gnome_ditem_edit_get_wmtitles_entry(GnomeDItemEdit   *dee);

GtkWidget *       gnome_ditem_edit_get_simple_dnd_toggle(GnomeDItemEdit *dee);
GtkWidget *       gnome_ditem_edit_get_file_drop_entry(GnomeDItemEdit  *dee);
GtkWidget *       gnome_ditem_edit_get_single_file_drop_toggle(GnomeDItemEdit *dee);
GtkWidget *       gnome_ditem_edit_get_url_drop_entry(GnomeDItemEdit  *dee);
GtkWidget *       gnome_ditem_edit_get_single_url_drop_toggle(GnomeDItemEdit *dee);

END_GNOME_DECLS
   
#endif /* GNOME_DITEM_EDIT_H */




