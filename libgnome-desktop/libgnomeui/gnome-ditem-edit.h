/*  -*- Mode: C; c-set-style: linux; indent-tabs-mode: nil; c-basic-offset: 8 -*-

   Copyright (C) 1999 Free Software Foundation
    
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

/******************** NOTE: this is an object, not a widget.
 ********************       You must supply a GtkNotebook.
 The reason for this is that you might want this in a property box, 
 or in your own notebook. Look at the test program at the bottom 
 of gnome-dentry-edit.c for a usage example.
 */

#ifndef GNOME_DENTRY_EDIT_H
#define GNOME_DENTRY_EDIT_H

#include <gtk/gtk.h>
#include "libgnome/gnome-defs.h"
#include "libgnome/gnome-ditem.h"

BEGIN_GNOME_DECLS

typedef struct _GnomeDItemEdit GnomeDItemEdit;
typedef struct _GnomeDItemEditClass GnomeDItemEditClass;

#define GNOME_DITEM_EDIT(obj)          GTK_CHECK_CAST (obj, gnome_ditem_edit_get_type (), GnomeDItemEdit)
#define GNOME_DITEM_EDIT_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gnome_ditem_edit_get_type (), GnomeDItemEditClass)
#define GNOME_IS_DITEM_EDIT(obj)       GTK_CHECK_TYPE (obj, gnome_ditem_edit_get_type ())

struct _GnomeDItemEdit {
        GtkObject object;
  
        /*semi public entries, you should however use macros to get these*/
        GtkWidget *child1;
        GtkWidget *child2;
  
        /* Remaining fields are private - if you need them, 
           please add an accessor function. */

        GtkWidget *name_entry;
        GtkWidget *comment_entry;
        GtkWidget *exec_entry;
        GtkWidget *tryexec_entry;
        GtkWidget *doc_entry;

        GtkWidget *type_combo;

        GtkWidget *terminal_button;  

        GtkWidget *icon_entry;
  
        GtkWidget *translations;
        GtkWidget *transl_lang_entry;
        GtkWidget *transl_name_entry;
        GtkWidget *transl_comment_entry;
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

#define gnome_ditem_edit_child1(d) (GNOME_DITEM_EDIT(d)->child1)
#define gnome_ditem_edit_child2(d) (GNOME_DITEM_EDIT(d)->child2)

guint             gnome_ditem_edit_get_type     (void);

/*create a new ditem and get the children using the below macros
  or use the utility new_notebook below*/
GtkObject *       gnome_ditem_edit_new          (void);


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
   GnomeDItemEdit widget. */
GtkWidget *       gnome_ditem_get_name_entry    (GnomeDItemEdit   *dee);
GtkWidget *       gnome_ditem_get_comment_entry (GnomeDItemEdit   *dee);
GtkWidget *       gnome_ditem_get_exec_entry    (GnomeDItemEdit   *dee);
GtkWidget *       gnome_ditem_get_tryexec_entry (GnomeDItemEdit   *dee);
GtkWidget *       gnome_ditem_get_doc_entry     (GnomeDItemEdit   *dee);
GtkWidget *       gnome_ditem_get_icon_entry    (GnomeDItemEdit   *dee);

END_GNOME_DECLS
   
#endif /* GNOME_DITEM_EDIT_H */




