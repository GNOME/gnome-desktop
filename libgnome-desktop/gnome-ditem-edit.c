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
/*
  @NOTATION@
*/

#include <config.h>

#include <ctype.h>
#include <string.h>
#include <gtk/gtk.h>

#include <libgnome/gnome-macros.h>
#define GNOME_EXPLICIT_TRANSLATION_DOMAIN GETTEXT_PACKAGE
#include <libgnome/gnome-i18n.h>

#include <libgnomeui/gnome-uidefs.h>
#include <libgnomeui/gnome-icon-entry.h>

#include "gnome-desktop-item.h"
#include "gnome-ditem-edit.h"

struct _GnomeDItemEditPrivate {
	/* we keep a ditem around, since we can never have absolutely
	   everything in the display so we load a file, or get a ditem,
	   sync the display and ref the ditem */
	GnomeDesktopItem *ditem;
	gboolean ui_dirty; /* TRUE if something got changed, and ditem
			    * was not yet synced */

	gboolean directory_only; /* always force a directory only entry */

	GtkWidget *child1;
	GtkWidget *child2;

        GtkWidget *name_entry;
        GtkWidget *comment_entry;
        GtkWidget *exec_label;
        GtkWidget *exec_entry;
        GtkWidget *tryexec_label;
        GtkWidget *tryexec_entry;
        GtkWidget *doc_entry;

        GtkWidget *type_label;
        GtkWidget *type_combo;

        GtkWidget *terminal_button;  

        GtkWidget *icon_entry;
  
        GtkWidget *translations;
        GtkWidget *transl_lang_entry;
        GtkWidget *transl_name_entry;
        GtkWidget *transl_comment_entry;
};

static void gnome_ditem_edit_class_init   (GnomeDItemEditClass *klass);
static void gnome_ditem_edit_instance_init(GnomeDItemEdit      *dee);

static void gnome_ditem_edit_destroy      (GtkObject           *object);
static void gnome_ditem_edit_finalize     (GObject             *object);

static void gnome_ditem_edit_sync_display (GnomeDItemEdit      *dee);
static void gnome_ditem_edit_sync_ditem   (GnomeDItemEdit      *dee);

static void gnome_ditem_edit_changed      (GnomeDItemEdit      *dee);
static void gnome_ditem_edit_icon_changed (GnomeDItemEdit      *dee);
static void gnome_ditem_edit_name_changed (GnomeDItemEdit      *dee);

enum {
        CHANGED,
        ICON_CHANGED,
        NAME_CHANGED,
        LAST_SIGNAL
};

static gint ditem_edit_signals[LAST_SIGNAL] = { 0 };

/* The following defines the get_type */

GNOME_CLASS_BOILERPLATE (GnomeDItemEdit, gnome_ditem_edit,
			 GtkNotebook, GTK_TYPE_NOTEBOOK)



static void
gnome_ditem_edit_class_init (GnomeDItemEditClass *klass)
{
        GtkObjectClass *object_class;
        GObjectClass *gobject_class;
        GnomeDItemEditClass * ditem_edit_class;

        ditem_edit_class = (GnomeDItemEditClass*) klass;

        object_class = (GtkObjectClass*) klass;
        gobject_class = (GObjectClass*) klass;

        ditem_edit_signals[CHANGED] =
                g_signal_new ("changed",
                                G_TYPE_FROM_CLASS (object_class),
                                G_SIGNAL_RUN_LAST,
                                G_STRUCT_OFFSET (GnomeDItemEditClass, changed),
                                NULL,
                                NULL,
                                g_cclosure_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);

        ditem_edit_signals[ICON_CHANGED] =
                g_signal_new ("icon_changed",
                                G_TYPE_FROM_CLASS (object_class),
                                G_SIGNAL_RUN_LAST,
                                G_STRUCT_OFFSET (GnomeDItemEditClass, 
                                                   icon_changed),
                                NULL,
                                NULL,
                                g_cclosure_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);

        ditem_edit_signals[NAME_CHANGED] =
                g_signal_new ("name_changed",
                                G_TYPE_FROM_CLASS (object_class),
                                G_SIGNAL_RUN_LAST,
                                G_STRUCT_OFFSET (GnomeDItemEditClass, 
                                                   name_changed),
                                NULL,
                                NULL,
                                g_cclosure_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);

        object_class->destroy = gnome_ditem_edit_destroy;
        gobject_class->finalize = gnome_ditem_edit_finalize;
        ditem_edit_class->changed = NULL;

	/* Always get translations in UTF-8, needed if library is used
	 * outside of gnome-core */
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
}

enum {
	ALL_TYPES,
	ONLY_DIRECTORY,
	ALL_EXCEPT_DIRECTORY
};

static void
setup_combo (GnomeDItemEdit *dee,
	     int             type,
	     const char     *extra)
{
	GList *types = NULL;

	switch (type) {
	case ONLY_DIRECTORY:
		types = g_list_prepend (types, "Directory");
		break;
	default: 
		types = g_list_prepend (types, "Application");

		if (type != ALL_EXCEPT_DIRECTORY)
			types = g_list_prepend (types, "Directory");

		types = g_list_prepend (types, "Link");
		types = g_list_prepend (types, "FSDevice");
		types = g_list_prepend (types, "MimeType");
		types = g_list_prepend (types, "Service");
		types = g_list_prepend (types, "ServiceType");
		break;
	}

	if (extra != NULL)
		types = g_list_prepend (types, (char *) extra);

	g_assert (types != NULL);

	gtk_combo_set_popdown_strings (
		GTK_COMBO (dee->_priv->type_combo), types);

	g_list_free (types);
}

static void 
table_attach_entry (GtkTable  *table,
		    GtkWidget *entry,
		    int        left,
		    int        right,
		    int        top,
		    int        bottom)
{
        gtk_table_attach (
		table, entry, left, right, top, bottom,
		GTK_EXPAND | GTK_FILL | GTK_SHRINK, GTK_FILL,
		0, 0);
}

static void
table_attach_label (GtkTable  *table,
		    GtkWidget *label,
		    int        left,
		    int        right,
		    int        top,
		    int        bottom)
{
        gtk_table_attach(
		table, label, left, right, top, bottom,
		GTK_FILL, GTK_FILL, 0, 0);
}

static void 
table_attach_list(GtkTable * table, GtkWidget * w,
		  gint l, gint r, gint t, gint b)
{
        gtk_table_attach(table, w, l, r, t, b,
                         GTK_EXPAND | GTK_FILL | GTK_SHRINK,
                         GTK_EXPAND | GTK_FILL | GTK_SHRINK,
                         0, 0);
}

static GtkWidget *
label_new (const char *text)
{
        GtkWidget *label;

        label = gtk_label_new (text);
        gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
        return label;
}

/* A hack! */
static void
type_combo_changed (GnomeDItemEdit *dee)
{
	const char *type;
        type = gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (dee->_priv->type_combo)->entry));
	if (type != NULL &&
	    strcmp (type, "Link") == 0 /* URL */)
		gtk_label_set_text (GTK_LABEL (dee->_priv->type_label),
				    _("URL:"));
	else
		gtk_label_set_text (GTK_LABEL (dee->_priv->type_label),
				    _("Type:"));
}

static void
fill_easy_page (GnomeDItemEdit *dee,
		GtkWidget      *table)
{
	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *hbox;
	GtkWidget *align;
	GtkWidget *combo;
	GtkWidget *icon_entry;
	GtkWidget *check_button;

	label = label_new (_("Name:"));
	table_attach_label (GTK_TABLE (table), label, 0, 1, 0, 1);

	entry = gtk_entry_new ();
	table_attach_entry (GTK_TABLE (table), entry, 1, 2, 0, 1);

	g_signal_connect_object (entry, "changed",
				 G_CALLBACK (gnome_ditem_edit_changed),
				 dee, G_CONNECT_SWAPPED);		

	g_signal_connect_object (entry, "changed",
				 G_CALLBACK (gnome_ditem_edit_name_changed),
				 dee, G_CONNECT_SWAPPED);
	dee->_priv->name_entry = entry;

	label = label_new (_("Comment:"));
	table_attach_label (GTK_TABLE (table), label, 0, 1, 1, 2);

	entry = gtk_entry_new ();
	table_attach_entry (GTK_TABLE (table), entry, 1, 2, 1, 2);

	g_signal_connect_object (entry, "changed",
				 G_CALLBACK (gnome_ditem_edit_changed),
                                 dee, G_CONNECT_SWAPPED);
	dee->_priv->comment_entry = entry;

	label = label_new (_("Command:"));
	table_attach_label (GTK_TABLE (table), label, 0, 1, 2, 3);
	dee->_priv->exec_label = label;

	entry = gtk_entry_new ();
	table_attach_entry (GTK_TABLE (table), entry, 1, 2, 2, 3);
 
	g_signal_connect_object (entry, "changed",
				 G_CALLBACK (gnome_ditem_edit_changed),
				 dee, G_CONNECT_SWAPPED);
	dee->_priv->exec_entry = entry;

	label = label_new (_("Type:"));
	table_attach_label (GTK_TABLE (table), label, 0, 1, 3, 4);
	dee->_priv->type_label = label;

	dee->_priv->type_combo = combo = gtk_combo_new ();
	setup_combo (dee, ALL_TYPES, NULL);

	gtk_combo_set_value_in_list (GTK_COMBO (combo), FALSE, TRUE);
	table_attach_entry (GTK_TABLE (table), combo, 1, 2, 3, 4);

	g_signal_connect_object (GTK_COMBO (combo)->entry, "changed",
				 G_CALLBACK (gnome_ditem_edit_changed),
				 dee, G_CONNECT_SWAPPED);	
	g_signal_connect_object (GTK_COMBO (combo)->entry, "changed",
				 G_CALLBACK (type_combo_changed),
				 dee, G_CONNECT_SWAPPED);
	dee->_priv->type_combo = combo;

	label = label_new (_("Icon:"));
	table_attach_label (GTK_TABLE (table), label, 0, 1, 4, 5);

	hbox = gtk_hbox_new (FALSE, GNOME_PAD_BIG);
	gtk_table_attach (GTK_TABLE (table), hbox, 1, 2, 4, 5,
			  GTK_EXPAND | GTK_FILL, 0, 0, 0); 

        icon_entry = gnome_icon_entry_new (
			"desktop-icon", _("Browse icons"));

        g_signal_connect_swapped (icon_entry, "changed",
				  G_CALLBACK (gnome_ditem_edit_changed), dee);
        g_signal_connect_swapped (icon_entry, "changed",
                                  G_CALLBACK (gnome_ditem_edit_icon_changed), dee);
        gtk_box_pack_start (
		GTK_BOX (hbox), icon_entry, FALSE, FALSE, 0);
        dee->_priv->icon_entry = icon_entry;

	align = gtk_alignment_new (0.0, 0.5, 0.0, 0.0);
	gtk_box_pack_start (GTK_BOX (hbox), align, FALSE, FALSE, 0);

        check_button = gtk_check_button_new_with_label (_("Run in Terminal"));
        g_signal_connect_swapped (check_button, "clicked",
				  G_CALLBACK (gnome_ditem_edit_changed), dee);

        gtk_container_add (GTK_CONTAINER (align), check_button);
        dee->_priv->terminal_button = check_button;

}

static GtkTreeIter*
return_iter_nth_row(GtkTreeView  *tree_view,
                    GtkTreeModel *tree_model,
                    GtkTreeIter  *iter,
                    gint         increment,
                    gint         row)
{
        GtkTreePath *current_path;
        gboolean row_expanded;
        gboolean children_present;
        gboolean next_iter;
        gboolean parent_iter;
        gboolean parent_next_iter;

        current_path = gtk_tree_model_get_path (tree_model, iter);

        if (increment == row)
            return iter;

        row_expanded = gtk_tree_view_row_expanded (tree_view, current_path);
        gtk_tree_path_free (current_path);
        children_present = gtk_tree_model_iter_children (tree_model, iter, iter);
        next_iter = gtk_tree_model_iter_next (tree_model, iter);
        parent_iter = gtk_tree_model_iter_parent (tree_model, iter, iter);
        parent_next_iter = gtk_tree_model_iter_next (tree_model, iter);

        if ((row_expanded && children_present) ||
            (next_iter) ||
            (parent_iter && parent_next_iter)){
               return return_iter_nth_row (tree_view, tree_model, iter,
                      ++increment, row);
        }

        return NULL;
}

static void
set_iter_nth_row (GtkTreeView *tree_view,
                  GtkTreeIter *iter,
                  gint        row)
{
        GtkTreeModel *tree_model;

        tree_model = gtk_tree_view_get_model (tree_view);
        gtk_tree_model_get_iter_root (tree_model, iter);
        iter = return_iter_nth_row (tree_view, tree_model, iter, 0 , row);
}

static void
translations_select_row (GtkTreeView    *cl,
			 int             row,
			 int             column,
			 GdkEvent       *event,
			 GnomeDItemEdit *dee)
{
        char *lang;
        char *name;
        char *comment;
        GtkTreeIter iter;
        GtkTreeModel *model;
        GValue value = {0, };	

        model = gtk_tree_view_get_model (GTK_TREE_VIEW (cl));	
        set_iter_nth_row (cl,&iter,row);

        gtk_tree_model_get_value (model, &iter, 0, &value);
        lang = g_strdup (g_value_get_string (&value));

        gtk_tree_model_get_value (model, &iter, 1, &value);
        name = g_strdup (g_value_get_string (&value));

        gtk_tree_model_get_value (model, &iter, 2, &value);
        comment = g_strdup (g_value_get_string (&value));
        g_value_unset (&value);

        gtk_entry_set_text(
		GTK_ENTRY (dee->_priv->transl_lang_entry), lang);
        gtk_entry_set_text (
		GTK_ENTRY (dee->_priv->transl_name_entry), name);
        gtk_entry_set_text(
		GTK_ENTRY (dee->_priv->transl_comment_entry), comment);
	
        g_free (lang);
        g_free (comment);
        g_free (name);
}

static int 
count_rows (GtkTreeView *view)
{
        int rows = 0;
        GtkTreeModel *model;
        GtkTreeIter iter;

        model = gtk_tree_view_get_model (view);
        gtk_tree_model_get_iter_root (model, &iter);

        while (gtk_tree_model_iter_next (model, &iter))
             rows++;

        return rows;
}

static void
translations_add (GtkWidget      *button,
		  GnomeDItemEdit *dee)
{
        int i = 0;
        int number_of_rows = 0;
        const char *lang;
        const char *name;
        const char *comment;
        const char *text[3];
        const GList *language_list;
        const char *curlang;
        GtkTreeView *tree;
        GtkTreeIter iter;
        GtkTreeModel *model;

        tree = GTK_TREE_VIEW (dee->_priv->translations);
        model = gtk_tree_view_get_model (tree);

        lang     = gtk_entry_get_text (GTK_ENTRY (dee->_priv->transl_lang_entry));
        name    = gtk_entry_get_text (GTK_ENTRY (dee->_priv->transl_name_entry));
        comment = gtk_entry_get_text (GTK_ENTRY (dee->_priv->transl_comment_entry));
  
        g_assert (lang != NULL && name != NULL && comment != NULL);
	
        lang = g_strstrip (g_strdup (lang));
	
        /*we are setting the current language so set the easy page entries*/
        /*FIXME: do the opposite as well!, but that's not that crucial*/
        language_list = gnome_i18n_get_language_list("LC_ALL");
        curlang = language_list ? language_list->data : NULL;
        if ((curlang && strcmp(curlang,lang)==0) ||
            ((!curlang || strcmp(curlang,"C")==0) && !*lang)) {
                gtk_entry_set_text(GTK_ENTRY(dee->_priv->name_entry),name);
                gtk_entry_set_text(GTK_ENTRY(dee->_priv->comment_entry),comment);
        }
        gtk_tree_model_get_iter_root (model, &iter);
        number_of_rows = count_rows (tree);
        for (i=0;i <number_of_rows;i++){
                char *s;
                GValue value = {0, };
                gtk_tree_model_get_value (model,&iter,0, &value);
                s = g_strdup (g_value_get_string (&value));
                g_value_unset (&value);

                if (!strcmp (lang, s)) {
                        gtk_list_store_set (GTK_LIST_STORE (model), &iter,
					    1, name,2, comment, -1);
                        g_signal_emit (dee, ditem_edit_signals [CHANGED], 0);
                        g_free (s);
                        return;
                }

                gtk_tree_model_iter_next (model, &iter);
                g_free (s);
        }
        text[0]=lang;
        text[1]=name;
        text[2]=comment;
        gtk_list_store_append (GTK_LIST_STORE(model), &iter);
        gtk_list_store_set (GTK_LIST_STORE(model),&iter, 0, text[0],1,text[1],2,text[2],-1);
        g_signal_emit(G_OBJECT(dee), ditem_edit_signals[CHANGED], 0);
}

static void
translations_remove (GtkWidget      *button,
		     GnomeDItemEdit *dee)
{
        GtkTreeView *view;
        GtkTreeSelection *selection;
        GtkTreeModel *model;
        GtkTreeIter iter;

        view = GTK_TREE_VIEW (dee->_priv->translations);
        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

        /* gtk_tree_selection_get_selected will return selected node if
	 * selection is set to GTK_SELECTION_SINGLE
	 */

        /* just return if nothing selected */
        if (!gtk_tree_selection_get_selected (selection, &model, &iter))
               return;

        gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
        g_signal_emit(G_OBJECT(dee), ditem_edit_signals[CHANGED], 0);
}
 
static void
fill_advanced_page (GnomeDItemEdit *dee,
		    GtkWidget      *page)
{
        GtkWidget         *label;
        GtkWidget         *button;
        GtkWidget         *box;
        const char        *transl [3];
        GtkCellRenderer   *renderer;
        GtkTreeViewColumn *column;
	GtkListStore      *model;

        dee->_priv->tryexec_label = label = label_new(_("Try this before using:"));
        table_attach_label(GTK_TABLE(page),label, 0, 1, 0, 1);

        dee->_priv->tryexec_entry = gtk_entry_new();
        table_attach_entry(GTK_TABLE(page),dee->_priv->tryexec_entry, 1, 2, 0, 1);
        g_signal_connect_swapped(G_OBJECT(dee->_priv->tryexec_entry), 
                                  "changed",
                                  G_CALLBACK(gnome_ditem_edit_changed),
                                  G_OBJECT(dee));

        label = label_new(_("Documentation:"));
        table_attach_label(GTK_TABLE(page),label, 0, 1, 1, 2);

        dee->_priv->doc_entry = gtk_entry_new();
        gtk_entry_set_max_length (GTK_ENTRY(dee->_priv->doc_entry), 255);        
        table_attach_entry(GTK_TABLE(page),dee->_priv->doc_entry, 1, 2, 1, 2);
        g_signal_connect_swapped(G_OBJECT(dee->_priv->doc_entry), 
                                  "changed",
                                  G_CALLBACK(gnome_ditem_edit_changed),
                                  G_OBJECT(dee));

        label = gtk_label_new(_("Name/Comment translations:"));
        table_attach_label(GTK_TABLE(page),label, 0, 2, 2, 3);
  
        transl [0] = _("Language");
        transl [1] = _("Name");
        transl [2] = _("Comment");

	model = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
        dee->_priv->translations =
		gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
	g_object_unref (model);

        renderer = gtk_cell_renderer_text_new ();	

        column = gtk_tree_view_column_new_with_attributes (transl [0], renderer, NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (dee->_priv->translations), column);

        column = gtk_tree_view_column_new_with_attributes (transl [1], renderer, NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (dee->_priv->translations), column);

        column = gtk_tree_view_column_new_with_attributes (transl [2], renderer, NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (dee->_priv->translations), column);

        gtk_tree_view_columns_autosize (GTK_TREE_VIEW (dee->_priv->translations));

        box = gtk_scrolled_window_new (NULL, NULL);
        gtk_widget_set_size_request (box, 0, 120);
        gtk_container_add (GTK_CONTAINER (box),dee->_priv->translations);
        table_attach_list (GTK_TABLE (page), box, 0, 2, 5, 6);

        gtk_tree_view_set_headers_clickable (
		GTK_TREE_VIEW (dee->_priv->translations), FALSE);
        g_signal_connect (dee->_priv->translations, "select_row",
			  G_CALLBACK (translations_select_row), dee);

        box = gtk_hbox_new(FALSE,GNOME_PAD_SMALL);
        table_attach_entry(GTK_TABLE(page),box, 0, 2, 3, 4);
  
        dee->_priv->transl_lang_entry = gtk_entry_new();
        gtk_box_pack_start(GTK_BOX(box),dee->_priv->transl_lang_entry,FALSE,FALSE,0);
        gtk_widget_set_size_request(dee->_priv->transl_lang_entry,50,0);

        dee->_priv->transl_name_entry = gtk_entry_new();
        gtk_box_pack_start(GTK_BOX(box),dee->_priv->transl_name_entry,TRUE,TRUE,0);

        dee->_priv->transl_comment_entry = gtk_entry_new();
        gtk_box_pack_start(GTK_BOX(box),dee->_priv->transl_comment_entry,TRUE,TRUE,0);

	g_signal_connect_swapped (G_OBJECT (dee->_priv->transl_name_entry), "changed",
				  G_CALLBACK(gnome_ditem_edit_changed), dee);

	g_signal_connect_swapped (G_OBJECT(dee->_priv->transl_comment_entry), "changed",
				  G_CALLBACK (gnome_ditem_edit_changed), dee);

        box = gtk_hbox_new(FALSE,GNOME_PAD_SMALL);
        table_attach_entry(GTK_TABLE(page),box, 0, 2, 4, 5);
  
        button = gtk_button_new_with_label(_("Add/Set"));
        gtk_box_pack_start(GTK_BOX(box),button,FALSE,FALSE,0);
        g_signal_connect(G_OBJECT(button),"clicked",
                           G_CALLBACK(translations_add),
                           dee);
        button = gtk_button_new_with_label(_("Remove"));
        gtk_box_pack_start(GTK_BOX(box),button,FALSE,FALSE,0);
        g_signal_connect(G_OBJECT(button),"clicked",
                           G_CALLBACK(translations_remove),
                           dee);
}

static GtkWidget *
make_page (void)
{
        GtkWidget * frame, * page;

        frame = gtk_frame_new (NULL);
        gtk_container_set_border_width (GTK_CONTAINER(frame), GNOME_PAD_SMALL);

        page = gtk_table_new (5, 2, FALSE);
        gtk_container_set_border_width (GTK_CONTAINER (page), GNOME_PAD_SMALL);
        gtk_table_set_row_spacings (GTK_TABLE (page), GNOME_PAD_SMALL);
        gtk_table_set_col_spacings (GTK_TABLE (page), GNOME_PAD_SMALL);

        gtk_container_add (GTK_CONTAINER (frame), page);

        return frame;
} 

static void
gnome_ditem_edit_instance_init (GnomeDItemEdit *dee)
{
	GtkWidget *child1, *child2;
	dee->_priv = g_new0(GnomeDItemEditPrivate, 1);

        child1 = make_page();
        fill_easy_page (dee, GTK_BIN (child1)->child);
        gtk_widget_show_all (child1);

        child2 = make_page();
        fill_advanced_page (dee, GTK_BIN(child2)->child);
        gtk_widget_show_all (child2);

        gtk_notebook_append_page (GTK_NOTEBOOK (dee), 
                                  child1,
				  gtk_label_new (_("Basic")));

        gtk_notebook_append_page (GTK_NOTEBOOK (dee), 
                                  child2,
				  gtk_label_new (_("Advanced")));

	dee->_priv->child1 = child1;
	dee->_priv->child2 = child2;

	/* FIXME: There needs to be a way to edit ALL keys/sections */
}


/**
 * gnome_ditem_edit_new
 *
 * Description: Creates a new #GnomeDItemEdit widget.  A widget
 * for the purpose of editing #GnomeDesktopItems
 *
 * Returns: Newly-created #GnomeDItemEdit widget.
 */
GtkWidget *
gnome_ditem_edit_new (void)
{
        GnomeDItemEdit * dee;

        dee = g_object_new (gnome_ditem_edit_get_type(), NULL);

        return GTK_WIDGET (dee);
}

static void
gnome_ditem_edit_destroy (GtkObject *object)
{
        GnomeDItemEdit *de;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GNOME_IS_DITEM_EDIT (object));

	/* remember, destroy can be run multiple times! */

        de = GNOME_DITEM_EDIT (object);

	if (de->_priv->ditem != NULL)
		gnome_desktop_item_unref (de->_priv->ditem);
	de->_priv->ditem = NULL; /* just for sanity */

	GNOME_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

static void
gnome_ditem_edit_finalize (GObject *object)
{
        GnomeDItemEdit *de;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GNOME_IS_DITEM_EDIT (object));

        de = GNOME_DITEM_EDIT (object);

	g_free(de->_priv);
	de->_priv = NULL;

	GNOME_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

/* set sensitive for directory/other type of a ditem */
static void
gnome_ditem_set_directory_sensitive (GnomeDItemEdit *dee,
				     gboolean is_directory)
{
	/* XXX: hack, evil, and such */
	if (dee->_priv->directory_only)
		is_directory = TRUE;

	gtk_widget_set_sensitive (dee->_priv->exec_label, ! is_directory);
	gtk_widget_set_sensitive (dee->_priv->exec_entry, ! is_directory);
	gtk_widget_set_sensitive (dee->_priv->type_label, ! is_directory);
	gtk_widget_set_sensitive (dee->_priv->type_combo, ! is_directory);
	gtk_widget_set_sensitive (dee->_priv->terminal_button, ! is_directory);
	gtk_widget_set_sensitive (dee->_priv->tryexec_label, ! is_directory);
	gtk_widget_set_sensitive (dee->_priv->tryexec_entry, ! is_directory);
}

/* Conform display to ditem */
static void
gnome_ditem_edit_sync_display (GnomeDItemEdit *dee)
{
        GList *i18n_list, *li;
	GnomeDesktopItemType type;
        const gchar* cs;
        char* tmpstr;
	GnomeDesktopItem *ditem;
        GtkTreeModel *model; 
        GtkTreeIter iter;
        g_return_if_fail (dee != NULL);
        g_return_if_fail (GNOME_IS_DITEM_EDIT (dee));

	ditem = dee->_priv->ditem;

	if (ditem == NULL) {
		gnome_ditem_edit_clear (dee);
		return;
	}

	type = gnome_desktop_item_get_entry_type (ditem);
	if (type == GNOME_DESKTOP_ITEM_TYPE_DIRECTORY ||
	    dee->_priv->directory_only) {
		gnome_ditem_set_directory_sensitive (dee, TRUE);
		setup_combo (dee, ONLY_DIRECTORY, NULL);
	} else {
		const char *extra = NULL;
		gnome_ditem_set_directory_sensitive (dee, FALSE);
		if (type == GNOME_DESKTOP_ITEM_TYPE_OTHER) {
			extra = gnome_desktop_item_get_string
				(ditem, GNOME_DESKTOP_ITEM_TYPE);
		}
		setup_combo (dee, ALL_EXCEPT_DIRECTORY, extra);
	}

        cs = gnome_desktop_item_get_localestring
		(ditem, GNOME_DESKTOP_ITEM_NAME);
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->name_entry), 
                           cs ? cs : "");

        cs = gnome_desktop_item_get_localestring
		(ditem, GNOME_DESKTOP_ITEM_COMMENT);
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->comment_entry),
                           cs ? cs : "");

	if (type == GNOME_DESKTOP_ITEM_TYPE_LINK) {
		cs = gnome_desktop_item_get_string (ditem,
						    GNOME_DESKTOP_ITEM_URL);
	} else {
		cs = gnome_desktop_item_get_string (ditem,
						    GNOME_DESKTOP_ITEM_EXEC);
	}
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->exec_entry), cs ? cs : "");

        cs = gnome_desktop_item_get_string (ditem,
					    GNOME_DESKTOP_ITEM_TRY_EXEC);
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->tryexec_entry), 
                           cs ? cs : "");

        tmpstr = gnome_desktop_item_get_icon (ditem);
	gnome_icon_entry_set_filename (GNOME_ICON_ENTRY (dee->_priv->icon_entry), cs);
	g_free (tmpstr);

        cs = gnome_desktop_item_get_string (ditem, "DocPath"); /* FIXME check name */
        gtk_entry_set_text (GTK_ENTRY (dee->_priv->doc_entry), cs ? cs : "");

        cs = gnome_desktop_item_get_string (ditem, GNOME_DESKTOP_ITEM_TYPE);
        gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (dee->_priv->type_combo)->entry),
			    cs ? cs : "");

        gtk_toggle_button_set_active
		(GTK_TOGGLE_BUTTON (dee->_priv->terminal_button),
		 gnome_desktop_item_get_boolean (ditem,
						 GNOME_DESKTOP_ITEM_TERMINAL));

        /*set the names and comments from our i18n list*/
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (dee->_priv->translations));
        gtk_list_store_clear (GTK_LIST_STORE (model));
        i18n_list = gnome_desktop_item_get_languages (ditem, NULL);
        for (li = i18n_list; li != NULL; li = li->next) {
                const char *text[3];
                const gchar* lang = li->data;
                cs = lang;
                text[0] = cs ? cs : "";
                cs = gnome_desktop_item_get_localestring_lang
			(ditem, GNOME_DESKTOP_ITEM_NAME, lang);
                text[1] = cs ? cs : "";
                cs = gnome_desktop_item_get_localestring_lang
			(ditem, GNOME_DESKTOP_ITEM_COMMENT, lang);
                text[2] = cs ? cs : "";
                gtk_list_store_append (GTK_LIST_STORE(model), &iter);
                gtk_list_store_set (GTK_LIST_STORE(model),&iter, 0, 
                                    text[0],1,text[1],2,text[2],-1);
        }
        g_list_free (i18n_list);

	/* clear the entries for add/remove */
        gtk_entry_set_text (GTK_ENTRY (dee->_priv->transl_lang_entry), "");
        gtk_entry_set_text (GTK_ENTRY (dee->_priv->transl_name_entry), "");
        gtk_entry_set_text (GTK_ENTRY (dee->_priv->transl_comment_entry), "");

	/* ui can't be dirty, I mean, damn we just synced it from the ditem */
	dee->_priv->ui_dirty = FALSE;
}

static const char *
get_language (void)
{
	const GList *list = gnome_i18n_get_language_list ("LC_MESSAGES");
	while (list != NULL) {
		const char *lang = list->data;
		/* find first without encoding */
		if (strchr (lang, '.') == NULL)
			return lang;
	}
	return NULL;
}

/* Conform ditem to display */
static void
gnome_ditem_edit_sync_ditem (GnomeDItemEdit *dee)
{
        const gchar * text;
        gchar *file;
	GnomeDesktopItem *ditem;
        int i,number_of_rows = 0; 
        GtkTreeView *view;

        GtkTreeModel *model;
        GtkTreeIter iter;

        g_return_if_fail (dee != NULL);
        g_return_if_fail (GNOME_IS_DITEM_EDIT(dee));

	ditem = dee->_priv->ditem;

	if (ditem == NULL) {
		ditem = gnome_desktop_item_new ();
		dee->_priv->ditem = ditem;
	}

        text = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(dee->_priv->type_combo)->entry));
        gnome_desktop_item_set_string (ditem, GNOME_DESKTOP_ITEM_TYPE, text);

	/* hack really */
	if (text != NULL &&
	    strcmp (text, "Link") == 0) {
		text = gtk_entry_get_text(GTK_ENTRY(dee->_priv->exec_entry));
		gnome_desktop_item_set_string (ditem, GNOME_DESKTOP_ITEM_URL, text);
	} else {
		text = gtk_entry_get_text(GTK_ENTRY(dee->_priv->exec_entry));
		gnome_desktop_item_set_string (ditem, GNOME_DESKTOP_ITEM_EXEC, text);
	}

        text = gtk_entry_get_text(GTK_ENTRY(dee->_priv->tryexec_entry));
        gnome_desktop_item_set_string (ditem, GNOME_DESKTOP_ITEM_TRY_EXEC, text);
  
	file = gnome_icon_entry_get_filename (GNOME_ICON_ENTRY (dee->_priv->icon_entry));
        gnome_desktop_item_set_string (ditem, GNOME_DESKTOP_ITEM_ICON, file);
	g_free (file);

        text = gtk_entry_get_text (GTK_ENTRY (dee->_priv->doc_entry));
        gnome_desktop_item_set_string (ditem, "DocPath", text);

        if (GTK_TOGGLE_BUTTON (dee->_priv->terminal_button)->active)
		gnome_desktop_item_set_boolean (ditem,
						GNOME_DESKTOP_ITEM_TERMINAL,
						TRUE);
        else
		gnome_desktop_item_set_boolean (ditem,
						GNOME_DESKTOP_ITEM_TERMINAL,
						FALSE);

	gnome_desktop_item_clear_localestring (ditem,
					       GNOME_DESKTOP_ITEM_NAME);
	gnome_desktop_item_clear_localestring (ditem,
					       GNOME_DESKTOP_ITEM_COMMENT);
        view = GTK_TREE_VIEW (dee->_priv->translations);
        number_of_rows = count_rows (view);	
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (dee->_priv->translations));
        gtk_tree_model_get_iter_root (model, &iter);
        for (i = 0; i < number_of_rows; i++) {
                char *lang, *name, *comment;
                GValue value = {0, };

                gtk_tree_model_get_value (model,&iter,0, &value);
                lang = g_strdup (g_value_get_string (&value));

                gtk_tree_model_get_value (model,&iter,1, &value); 
                name = g_strdup (g_value_get_string (&value));;

                gtk_tree_model_get_value (model,&iter,2, &value);
                comment = g_strdup (g_value_get_string (&value));;
                g_value_unset (&value);

                if(!name && !comment){
                        g_free (lang);
                        continue;
                }
                if (lang == NULL)
                        lang = g_strdup(get_language());

                g_assert (lang != NULL);
                
                gnome_desktop_item_set_localestring_lang
			(ditem, GNOME_DESKTOP_ITEM_NAME, lang, name);
                gnome_desktop_item_set_localestring_lang
			(ditem, GNOME_DESKTOP_ITEM_NAME, lang, comment);

                g_free (name);
                g_free (comment);
                g_free (lang);
                gtk_tree_model_iter_next (model, &iter);
        }

	/* This always overrides the above in case of conflict */
        text = gtk_entry_get_text(GTK_ENTRY(dee->_priv->name_entry));
        gnome_desktop_item_set_localestring (ditem, GNOME_DESKTOP_ITEM_NAME, text);

        text = gtk_entry_get_text(GTK_ENTRY(dee->_priv->comment_entry));
        gnome_desktop_item_set_localestring (ditem, GNOME_DESKTOP_ITEM_COMMENT, text);

	/* ui not dirty any more, we just synced it */
	dee->_priv->ui_dirty = FALSE;
}

/**
 * gnome_ditem_edit_load_uri
 * @dee: #GnomeDItemEdit object to work with
 * @uri: file to load into the editting areas
 *
 * Description: Load a .desktop file and update the editting areas
 * of the object accordingly.
 *
 * Returns:  %TRUE if successful, %FALSE otherwise
 */
gboolean
gnome_ditem_edit_load_uri (GnomeDItemEdit *dee,
			   const gchar *uri,
			   GError **error)
{
        GnomeDesktopItem * newentry;

        g_return_val_if_fail (dee != NULL, FALSE);
        g_return_val_if_fail (GNOME_IS_DITEM_EDIT (dee), FALSE);
        g_return_val_if_fail (uri != NULL, FALSE);

        newentry = gnome_desktop_item_new_from_uri (uri, 0, error);

        if (newentry != NULL) {
		if (dee->_priv->ditem != NULL)
			gnome_desktop_item_unref (dee->_priv->ditem);
		dee->_priv->ditem = newentry;
		dee->_priv->ui_dirty = TRUE;
                gnome_ditem_edit_sync_display (dee);
		return TRUE;
        } else {
                return FALSE;
        }
}

/**
 * gnome_ditem_edit_set_ditem
 * @dee: #GnomeDItemEdit object to work with
 * @ditem: #GnomeDesktopItem to use
 *
 * Description: Set the ditem edit UI to this item.  This makes a copy
 * internally so do not worry about modifying this item later yourself.
 * Note that since the entire item is stored, any hidden fields will be
 * preserved when you later get it with #gnome_ditem_edit_get_ditem.
 *
 * Returns:
 */
void
gnome_ditem_edit_set_ditem (GnomeDItemEdit *dee,
                           const GnomeDesktopItem *ditem)
{
        g_return_if_fail (dee != NULL);
        g_return_if_fail (GNOME_IS_DITEM_EDIT (dee));
        g_return_if_fail (ditem != NULL);

	if (dee->_priv->ditem != NULL)
		gnome_desktop_item_unref (dee->_priv->ditem);
	dee->_priv->ditem = gnome_desktop_item_copy (ditem);
	dee->_priv->ui_dirty = TRUE;
        gnome_ditem_edit_sync_display (dee);
}

/**
 * gnome_ditem_edit_get_ditem
 * @dee: #GnomeDItemEdit object to work with
 *
 * Description: Get the current status of the editting areas
 * as a #GnomeDesktopItem structure.  It will give you a pointer
 * to the internal structure.  If you wish to modify it,
 * make a copy of it with #gnome_desktop_item_copy
 *
 * Returns: a pointer to the internal #GnomeDesktopItem structure.
 */
GnomeDesktopItem *
gnome_ditem_edit_get_ditem (GnomeDItemEdit *dee)
{
        g_return_val_if_fail (dee != NULL, NULL);
        g_return_val_if_fail (GNOME_IS_DITEM_EDIT (dee), NULL);

	if (dee->_priv->ditem == NULL) {
		dee->_priv->ditem = gnome_desktop_item_new ();
		dee->_priv->ui_dirty = TRUE;
	}
	if (dee->_priv->ui_dirty)
		gnome_ditem_edit_sync_ditem (dee);
	return dee->_priv->ditem;
}

/**
 * gnome_ditem_edit_clear
 * @dee: #GnomeDItemEdit object to work with
 *
 * Description: Clear the editting areas.  And unref any
 * stored #GnomeDesktopItem.
 *
 * Returns:
 */
void
gnome_ditem_edit_clear (GnomeDItemEdit *dee)
{
        GtkTreeModel *model;
        g_return_if_fail (dee != NULL);
        g_return_if_fail (GNOME_IS_DITEM_EDIT (dee));

	if (dee->_priv->ditem != NULL)
		gnome_desktop_item_unref (dee->_priv->ditem);
	dee->_priv->ditem = NULL;
	dee->_priv->ui_dirty = TRUE;

        gtk_entry_set_text(GTK_ENTRY(dee->_priv->name_entry), "");
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->comment_entry),"");
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->exec_entry), "");  
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->tryexec_entry), "");
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->doc_entry), "");
        gnome_icon_entry_set_filename (GNOME_ICON_ENTRY (dee->_priv->icon_entry), "");
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->transl_lang_entry), "");
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->transl_name_entry), "");
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->transl_comment_entry), "");
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (dee->_priv->translations));
        gtk_list_store_clear (GTK_LIST_STORE (model));
	/* set everything to non-directory type which means any type */
	gnome_ditem_set_directory_sensitive (dee, FALSE);

	/* put all our possibilities here */
	setup_combo (dee, ALL_TYPES, NULL);
}

static void
gnome_ditem_edit_changed (GnomeDItemEdit *dee)
{
	dee->_priv->ui_dirty = TRUE;
        g_signal_emit (G_OBJECT (dee), ditem_edit_signals[CHANGED], 0);
}

static void
gnome_ditem_edit_icon_changed (GnomeDItemEdit *dee)
{
	dee->_priv->ui_dirty = TRUE;
        g_signal_emit (G_OBJECT (dee), ditem_edit_signals[ICON_CHANGED], 0);
}

static void
gnome_ditem_edit_name_changed (GnomeDItemEdit *dee)
{
	dee->_priv->ui_dirty = TRUE;
        g_signal_emit (G_OBJECT (dee), ditem_edit_signals[NAME_CHANGED], 0);
}

/**
 * gnome_ditem_edit_get_icon
 * @dee: #GnomeDItemEdit object to work with
 *
 * Description: Get the icon filename.
 *
 * Returns: a newly allocated string with the filename of the icon
 */
gchar *
gnome_ditem_edit_get_icon (GnomeDItemEdit *dee)
{
        g_return_val_if_fail (dee != NULL, NULL);
        g_return_val_if_fail (GNOME_IS_DITEM_EDIT (dee), NULL);
 
        return gnome_icon_entry_get_filename (GNOME_ICON_ENTRY (dee->_priv->icon_entry));
}

/**
 * gnome_ditem_edit_get_name
 * @dee: #GnomeDItemEdit object to work with
 *
 * Description: Get the Name field from the ditem for the current language.
 *
 * Returns: a newly allocated string with the name of the ditem
 */
gchar *
gnome_ditem_edit_get_name (GnomeDItemEdit *dee)
{
        const char * name;

        g_return_val_if_fail (dee != NULL, NULL);
        g_return_val_if_fail (GNOME_IS_DITEM_EDIT (dee), NULL);

        name = gtk_entry_get_text (GTK_ENTRY (dee->_priv->name_entry));
        return g_strdup (name);
}

/* eeeeeeeeek!, evil api */
void
gnome_ditem_edit_grab_focus (GnomeDItemEdit *dee)
{
        g_return_if_fail (dee != NULL);
        g_return_if_fail (GNOME_IS_DITEM_EDIT (dee));

	gtk_widget_grab_focus (dee->_priv->name_entry);
}
void
gnome_ditem_edit_set_editable (GnomeDItemEdit *dee,
			       gboolean editable)
{
        g_return_if_fail (dee != NULL);
        g_return_if_fail (GNOME_IS_DITEM_EDIT (dee));

	/* FIXME: a better way needs to be found */
	gtk_widget_set_sensitive (dee->_priv->child1, editable);
	gtk_widget_set_sensitive (dee->_priv->child2, editable);
}

/* set the string type of the entry */
void
gnome_ditem_edit_set_entry_type (GnomeDItemEdit *dee,
				 const char     *type)
{
        g_return_if_fail (dee != NULL);
        g_return_if_fail (GNOME_IS_DITEM_EDIT (dee));

        gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (dee->_priv->type_combo)->entry),
			    type ? type : "");
}

/* force directory only */
void
gnome_ditem_edit_set_directory_only (GnomeDItemEdit *dee,
				     gboolean        directory_only)
{
        g_return_if_fail (dee != NULL);
        g_return_if_fail (GNOME_IS_DITEM_EDIT (dee));

	if (dee->_priv->directory_only != directory_only) {
		dee->_priv->directory_only = directory_only;

		if (directory_only) {
			gnome_ditem_set_directory_sensitive (dee, TRUE);
			gnome_ditem_edit_set_entry_type (dee, "Directory");
			setup_combo (dee, ONLY_DIRECTORY, NULL);
		} else if (dee->_priv->ditem != NULL) {
			GnomeDesktopItemType type;
			type = gnome_desktop_item_get_entry_type (dee->_priv->ditem);
			if (type == GNOME_DESKTOP_ITEM_TYPE_DIRECTORY) {
				gnome_ditem_set_directory_sensitive (dee, TRUE);
				setup_combo (dee, ONLY_DIRECTORY, NULL);
			} else {
				const char *extra = NULL;
				gnome_ditem_set_directory_sensitive (dee, FALSE);
				if (type == GNOME_DESKTOP_ITEM_TYPE_OTHER) {
					extra = gnome_desktop_item_get_string
						(dee->_priv->ditem, GNOME_DESKTOP_ITEM_TYPE);
				}
				setup_combo (dee, ALL_EXCEPT_DIRECTORY, extra);
			}
		} else {
			gnome_ditem_set_directory_sensitive (dee, FALSE);
			setup_combo (dee, ALL_TYPES, NULL /* extra */);
		}
	}
}
