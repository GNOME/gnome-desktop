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

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <gtk/gtk.h>

#include <libgnome/gnome-macros.h>
#include "gnome-desktop-i18n.h"

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
        GtkWidget *generic_name_entry;
        GtkWidget *comment_entry;
        GtkWidget *exec_label;
        GtkWidget *exec_entry;
        GtkWidget *tryexec_label;
        GtkWidget *tryexec_entry;
        GtkWidget *doc_entry;

        GtkWidget *type_label;
        GtkWidget *type_option;

        GtkWidget *terminal_button;  

        GtkWidget *icon_entry;

	/* the directory of the theme for the icon, see bug #119208 */
	char *icon_theme_dir;
  
        GtkWidget *translations;
        GtkWidget *transl_lang_entry;
        GtkWidget *transl_name_entry;
        GtkWidget *transl_generic_name_entry;
        GtkWidget *transl_comment_entry;
        GtkWidget *transl_icon_entry;
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

static void set_relation (GtkWidget *widget, GtkLabel *label);
static void destroy_tooltip (GtkObject *object);
static void set_tooltip (GnomeDItemEdit *dee, GtkWidget *widget,
			 const gchar *description);

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
}

enum {
	ALL_TYPES,
	ONLY_DIRECTORY,
	ALL_EXCEPT_DIRECTORY
};

#define TYPE_STRING "GnomeDitemEdit:TypeString"

static void
add_menuitem (GtkWidget   *menu,
	      const char  *str,
	      const char  *label,
	      const char  *select,
	      GtkWidget  **selected)
{
	GtkWidget *item;

	item = gtk_menu_item_new_with_label (label);
	gtk_widget_show (item);

	g_object_set_data_full (G_OBJECT (item), TYPE_STRING,
				g_strdup (str), (GDestroyNotify) g_free);

	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	if (selected && select && !strcmp (str, select))
		*selected = item;
}

static void
setup_option (GnomeDItemEdit *dee,
	      int             type,
	      const char     *select)
{
	GtkWidget *menu;
	GtkWidget *selected = NULL;

	menu = gtk_menu_new ();

	switch (type) {
	case ONLY_DIRECTORY:
		add_menuitem (menu, "Directory", _("Directory"),
			      select, &selected);
		break;
	default: 
		add_menuitem (menu, "Application", _("Application"),
			      select, &selected);

		if (type != ALL_EXCEPT_DIRECTORY)
			add_menuitem (menu, "Directory", _("Directory"),
				      select, &selected);

		add_menuitem (menu, "Link", _("Link"),
			      select, &selected);
		add_menuitem (menu, "FSDevice", _("FSDevice"),
			      select, &selected);
		add_menuitem (menu, "MimeType", _("MIME Type"),
			      select, &selected);
		add_menuitem (menu, "Service", _("Service"),
			      select, &selected);
		add_menuitem (menu, "ServiceType", _("ServiceType"),
			      select, &selected);
		break;
	}

	if (select && !selected)
		add_menuitem (menu, select, _(select), select, &selected);

	gtk_option_menu_set_menu (GTK_OPTION_MENU (dee->_priv->type_option), menu);

	if (selected) {
		GList *children;
		int    pos;

		children = gtk_container_get_children (GTK_CONTAINER (menu));
		pos = g_list_index (children, selected);
		g_list_free (children);	

		gtk_option_menu_set_history (GTK_OPTION_MENU (dee->_priv->type_option), pos);
	}

}

static const char *
get_type_from_option (GnomeDItemEdit *dee)
{
	GtkWidget *menu, *active;

	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (dee->_priv->type_option));
	if (menu == NULL)
		return NULL;

	active = gtk_menu_get_active (GTK_MENU (menu));
	if (active == NULL)
		return NULL;

	return g_object_get_data (G_OBJECT (active), TYPE_STRING);
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

static GtkWidget *
label_new_with_mnemonic (const char *text)
{
        GtkWidget *label;

        label = gtk_label_new_with_mnemonic (text);
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        return label;
}

/* A hack! */
static void
type_option_changed (GnomeDItemEdit *dee)
{
	const char *type;
	type = get_type_from_option (dee);
	if (type != NULL &&
	    strcmp (type, "Link") == 0 /* URL */)
		gtk_label_set_text_with_mnemonic (GTK_LABEL (dee->_priv->exec_label),
				    _("_URL:"));
	else
		gtk_label_set_text_with_mnemonic (GTK_LABEL (dee->_priv->exec_label),
				    _("Comm_and:"));
}

static GtkWidget *
make_easy_page (GnomeDItemEdit *dee)
{
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *hbox;
	GtkWidget *align;
	GtkWidget *option;
	GtkWidget *icon_entry;
	GtkWidget *check_button;

        table = gtk_table_new (5, 2, FALSE);
        gtk_container_set_border_width (GTK_CONTAINER (table), 12);
        gtk_table_set_row_spacings (GTK_TABLE (table), 6);
        gtk_table_set_col_spacings (GTK_TABLE (table), 12);

	/* Name */
	label = label_new_with_mnemonic (_("_Name:"));
	table_attach_label (GTK_TABLE (table), label, 0, 1, 0, 1);

	entry = gtk_entry_new ();
	gtk_label_set_mnemonic_widget (GTK_LABEL(label), entry);
	table_attach_entry (GTK_TABLE (table), entry, 1, 2, 0, 1);

	g_signal_connect_object (entry, "changed",
				 G_CALLBACK (gnome_ditem_edit_changed),
				 dee, G_CONNECT_SWAPPED);		

	g_signal_connect_object (entry, "changed",
				 G_CALLBACK (gnome_ditem_edit_name_changed),
				 dee, G_CONNECT_SWAPPED);
	dee->_priv->name_entry = entry;

	set_relation (dee->_priv->name_entry, GTK_LABEL (label));

	/* Generic Name */
	label = label_new_with_mnemonic (_("_Generic name:"));
	table_attach_label (GTK_TABLE (table), label, 0, 1, 1, 2);

	entry = gtk_entry_new ();
	gtk_label_set_mnemonic_widget (GTK_LABEL(label), entry);
	table_attach_entry (GTK_TABLE (table), entry, 1, 2, 1, 2);

	g_signal_connect_object (entry, "changed",
				 G_CALLBACK (gnome_ditem_edit_changed),
				 dee, G_CONNECT_SWAPPED);		

	g_signal_connect_object (entry, "changed",
				 G_CALLBACK (gnome_ditem_edit_name_changed),
				 dee, G_CONNECT_SWAPPED);
	dee->_priv->generic_name_entry = entry;

	set_relation (dee->_priv->generic_name_entry, GTK_LABEL (label));

	/* Comment */
	label = label_new_with_mnemonic (_("Co_mment:"));
	table_attach_label (GTK_TABLE (table), label, 0, 1, 2, 3);

	entry = gtk_entry_new ();
	gtk_label_set_mnemonic_widget (GTK_LABEL(label), entry);
	table_attach_entry (GTK_TABLE (table), entry, 1, 2, 2, 3);

	g_signal_connect_object (entry, "changed",
				 G_CALLBACK (gnome_ditem_edit_changed),
                                 dee, G_CONNECT_SWAPPED);
	dee->_priv->comment_entry = entry;

	set_relation (dee->_priv->comment_entry, GTK_LABEL (label));

	label = label_new_with_mnemonic (_("Comm_and:"));
	table_attach_label (GTK_TABLE (table), label, 0, 1, 3, 4);
	dee->_priv->exec_label = label;

	entry = gnome_file_entry_new ("command", _("Browse"));
	gtk_label_set_mnemonic_widget (GTK_LABEL(label), entry);
	table_attach_entry (GTK_TABLE (table), entry, 1, 2, 3, 4);
 
	g_signal_connect_object (entry, "changed",
				 G_CALLBACK (gnome_ditem_edit_changed),
				 dee, G_CONNECT_SWAPPED);
	dee->_priv->exec_entry = entry;

	set_relation (dee->_priv->exec_entry, GTK_LABEL (label));

	label = label_new_with_mnemonic (_("_Type:"));
	table_attach_label (GTK_TABLE (table), label, 0, 1, 4, 5);
	dee->_priv->type_label = label;

	dee->_priv->type_option = option = gtk_option_menu_new ();
	gtk_label_set_mnemonic_widget (GTK_LABEL(label), option);
	setup_option (dee, ALL_TYPES, NULL);

	table_attach_entry (GTK_TABLE (table), option, 1, 2, 4, 5);

	g_signal_connect_object (G_OBJECT (option), "changed",
				 G_CALLBACK (gnome_ditem_edit_changed),
				 dee, G_CONNECT_SWAPPED);	
	g_signal_connect_object (G_OBJECT (option), "changed",
				 G_CALLBACK (type_option_changed),
				 dee, G_CONNECT_SWAPPED);

	set_relation (dee->_priv->type_option, GTK_LABEL (label));

	label = label_new_with_mnemonic (_("_Icon:"));
	table_attach_label (GTK_TABLE (table), label, 0, 1, 5, 6);

	hbox = gtk_hbox_new (FALSE, 12);
	gtk_table_attach (GTK_TABLE (table), hbox, 1, 2, 5, 6,
			  GTK_EXPAND | GTK_FILL, 0, 0, 0); 

	/* FIXME: locale specific icons!!! how the hell do we
	 * handle that !!! */
        icon_entry = gnome_icon_entry_new (
			"desktop-icon", _("Browse icons"));
        gtk_label_set_mnemonic_widget (GTK_LABEL(label), icon_entry);

        g_signal_connect_swapped (icon_entry, "changed",
				  G_CALLBACK (gnome_ditem_edit_changed), dee);
        g_signal_connect_swapped (icon_entry, "changed",
                                  G_CALLBACK (gnome_ditem_edit_icon_changed), dee);
        gtk_box_pack_start (
		GTK_BOX (hbox), icon_entry, FALSE, FALSE, 0);
        dee->_priv->icon_entry = icon_entry;

	set_relation (dee->_priv->icon_entry, GTK_LABEL (label));

	align = gtk_alignment_new (0.0, 0.5, 0.0, 0.0);
	gtk_box_pack_start (GTK_BOX (hbox), align, FALSE, FALSE, 0);

        check_button = gtk_check_button_new_with_mnemonic (_("Run in t_erminal"));
        g_signal_connect_swapped (check_button, "clicked",
				  G_CALLBACK (gnome_ditem_edit_changed), dee);

        gtk_container_add (GTK_CONTAINER (align), check_button);
        dee->_priv->terminal_button = check_button;

	return table;
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
translations_select_row (GtkTreeSelection *selection,
			 GnomeDItemEdit   *dee)
{
        GtkTreeModel *model = NULL;
        GtkTreeIter   iter;
        char         *lang;
        char         *name;
        char         *generic_name;
        char         *comment;

	 if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	gtk_tree_model_get (
		model, &iter,
		0, &lang,
		1, &name,
		2, &generic_name,
		3, &comment,
		-1);

        gtk_entry_set_text(
		GTK_ENTRY (dee->_priv->transl_lang_entry), lang);
        gtk_entry_set_text (
		GTK_ENTRY (dee->_priv->transl_name_entry), name);
        gtk_entry_set_text (
		GTK_ENTRY (dee->_priv->transl_generic_name_entry), generic_name);
        gtk_entry_set_text(
		GTK_ENTRY (dee->_priv->transl_comment_entry), comment);
	
        g_free (lang);
        g_free (comment);
        g_free (name);
}

static void
translations_add (GtkWidget      *button,
		  GnomeDItemEdit *dee)
{
	GtkTreeView  *tree;
	GtkTreeModel *model;
	GtkTreeIter   iter;
	const char   *tmp;
	const char   *name;
	const char   *generic_name;
	const char   *comment;
	char         *lang;
	const char   *locale;
	gboolean      ret;

	tmp     = gtk_entry_get_text (GTK_ENTRY (dee->_priv->transl_lang_entry));
	name    = gtk_entry_get_text (GTK_ENTRY (dee->_priv->transl_name_entry));
	generic_name = gtk_entry_get_text (GTK_ENTRY (dee->_priv->transl_generic_name_entry));
	comment = gtk_entry_get_text (GTK_ENTRY (dee->_priv->transl_comment_entry));

	g_assert (tmp != NULL && name != NULL && comment != NULL);
	
	lang = g_strstrip (g_strdup (tmp));

	if (!lang [0]) {
		g_free (lang);
		return;
	}

	/*
	 * If we are editing the current language, change the name and
	 * comment entries on the easy page as well.
	 */
	locale = gnome_desktop_item_get_attr_locale (gnome_ditem_edit_get_ditem (dee),
						     "Name");

	if ((locale && !strcmp (locale, lang)) || (!locale && !strcmp (lang, "C"))) {
		gtk_entry_set_text (
			GTK_ENTRY (dee->_priv->name_entry), name);
		gtk_entry_set_text (
			GTK_ENTRY (dee->_priv->generic_name_entry), generic_name);
		gtk_entry_set_text (
			GTK_ENTRY (dee->_priv->comment_entry), comment);
	}

	tree  = GTK_TREE_VIEW (dee->_priv->translations);
	model = gtk_tree_view_get_model (tree);

	ret = gtk_tree_model_get_iter_first (model, &iter);
        while (ret) {
		char *string;

		gtk_tree_model_get (model, &iter, 0, &string, -1);

		if (!strcmp (lang, string)) {
			gtk_list_store_set (
				GTK_LIST_STORE (model), &iter,
				1, name,
				2, generic_name,
				3, comment,
				-1);

			gnome_ditem_edit_changed (dee);

			g_free (string);
			g_free (lang);

			return;
		}

		g_free (string);

		ret = gtk_tree_model_iter_next (model, &iter);
	}

	gtk_list_store_append (GTK_LIST_STORE (model), &iter);
	gtk_list_store_set (
		GTK_LIST_STORE (model), &iter,
		0, lang,
		1, name,
		2, generic_name,
		3, comment,
		-1);

	gtk_editable_delete_text (
		GTK_EDITABLE (dee->_priv->transl_lang_entry), 0, -1);
	gtk_editable_delete_text (
		GTK_EDITABLE (dee->_priv->transl_name_entry), 0, -1);
	gtk_editable_delete_text (
		GTK_EDITABLE (dee->_priv->transl_generic_name_entry), 0, -1);
	gtk_editable_delete_text (
		GTK_EDITABLE (dee->_priv->transl_comment_entry), 0, -1);

	gnome_ditem_edit_changed (dee);

	g_free (lang);
}

static void
translations_remove (GtkWidget      *button,
		     GnomeDItemEdit *dee)
{
        GtkTreeView      *view;
        GtkTreeSelection *selection;
        GtkTreeModel     *model;
        GtkTreeIter       iter;

        view = GTK_TREE_VIEW (dee->_priv->translations);
        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

        if (!gtk_tree_selection_get_selected (selection, &model, &iter))
               return;

        gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
	
	gnome_ditem_edit_changed (dee);
}

static GtkWidget *
setup_translations_list (GnomeDItemEdit *dee)
{
	GtkCellRenderer   *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection  *selection;
	GtkListStore      *model;
	GtkWidget         *tree;

	model = gtk_list_store_new (4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
	g_object_unref (model);

	renderer = gtk_cell_renderer_text_new ();	

	column = gtk_tree_view_column_new_with_attributes (
					_("Language"), renderer,
					"text", 0, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

	column = gtk_tree_view_column_new_with_attributes (
					_("Name"), renderer,
					"text", 1, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

	column = gtk_tree_view_column_new_with_attributes (
					_("Generic name"), renderer,
					"text", 2, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

	column = gtk_tree_view_column_new_with_attributes (
					_("Comment"), renderer,
					"text", 3, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

	gtk_tree_view_columns_autosize (GTK_TREE_VIEW (tree));

	gtk_tree_view_set_headers_clickable (GTK_TREE_VIEW (tree), FALSE);

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));

	g_signal_connect (selection, "changed",
			  G_CALLBACK (translations_select_row), dee);

	return tree;
}
 
static GtkWidget *
make_advanced_page (GnomeDItemEdit *dee)
{
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *button;
	GtkWidget *box;

	vbox = gtk_vbox_new (FALSE, 6);
        gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);

        table = gtk_table_new (2, 2, FALSE);
        gtk_table_set_row_spacings (GTK_TABLE (table), 6);
        gtk_table_set_col_spacings (GTK_TABLE (table), 12);

        gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);

	label = label_new_with_mnemonic (_("_Try this before using:"));
	table_attach_label (GTK_TABLE (table), label, 0, 1, 0, 1);
	dee->_priv->tryexec_label = label;

	entry = gtk_entry_new ();
	gtk_label_set_mnemonic_widget (GTK_LABEL(label), entry);
	table_attach_entry (GTK_TABLE (table), entry, 1, 2, 0, 1);
	g_signal_connect_swapped (entry, "changed",
				  G_CALLBACK (gnome_ditem_edit_changed), dee);
	dee->_priv->tryexec_entry = entry;

	set_relation (dee->_priv->tryexec_entry, GTK_LABEL (label));

	label = label_new_with_mnemonic (_("_Documentation:"));
	table_attach_label (GTK_TABLE (table), label, 0, 1, 1, 2);

	entry = gtk_entry_new ();
	gtk_label_set_mnemonic_widget (GTK_LABEL(label), entry);
	gtk_entry_set_max_length (GTK_ENTRY (entry), 255);        
	table_attach_entry (GTK_TABLE (table), entry, 1, 2, 1, 2);
	g_signal_connect_swapped (entry, "changed",
				  G_CALLBACK (gnome_ditem_edit_changed), dee);
	dee->_priv->doc_entry = entry;

	set_relation (dee->_priv->doc_entry, GTK_LABEL (label));

	label = gtk_label_new_with_mnemonic (_("_Name/Comment translations:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	dee->_priv->translations = setup_translations_list (dee);

	box = gtk_scrolled_window_new (NULL, NULL);
	gtk_label_set_mnemonic_widget (GTK_LABEL(label),
	                               dee->_priv->translations);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (box),
					     GTK_SHADOW_NONE);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (box),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request (box, 0, 120);
	gtk_container_add (GTK_CONTAINER (box), dee->_priv->translations);
	gtk_box_pack_start (GTK_BOX (vbox), box, TRUE, TRUE, 0);

	box = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, FALSE, 0);

	entry = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (box), entry, TRUE, TRUE, 0);
	gtk_widget_set_size_request (entry, 30, -1);
	dee->_priv->transl_lang_entry = entry;

	set_tooltip (dee, GTK_WIDGET(entry), _("Language"));
	set_relation (dee->_priv->transl_lang_entry, GTK_LABEL (label));

	entry = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (box), entry, TRUE, TRUE, 0);
	gtk_widget_set_size_request (entry, 80, -1);
	dee->_priv->transl_name_entry = entry;

	set_tooltip (dee, GTK_WIDGET(entry), _("Name"));
	set_relation (dee->_priv->transl_name_entry, GTK_LABEL (label));

	entry = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (box), entry, TRUE, TRUE, 0);
	gtk_widget_set_size_request (entry, 80, -1);
	dee->_priv->transl_generic_name_entry = entry;

	set_tooltip (dee, GTK_WIDGET (entry), _("Generic name"));
	set_relation (dee->_priv->transl_generic_name_entry, GTK_LABEL (label));

	/* FIXME: transl_icon_entry, locale specific icons */

	entry = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (box), entry, TRUE, TRUE, 0);
	gtk_widget_set_size_request (entry, 80, -1);
	dee->_priv->transl_comment_entry = entry;

	set_tooltip (dee, GTK_WIDGET(entry), _("Comment"));
	set_relation (dee->_priv->transl_comment_entry, GTK_LABEL (label));

	button = gtk_button_new_with_mnemonic (_("_Add/Set"));
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (translations_add), dee);
  
	set_tooltip (dee, GTK_WIDGET(button),
		    _("Add or Set Name/Comment Translations"));

	button = gtk_button_new_with_mnemonic (_("Re_move"));
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (translations_remove), dee);
	set_tooltip (dee, GTK_WIDGET(button),
		    _("Remove Name/Comment Translation"));
  
	return vbox;
}

static void
gnome_ditem_edit_instance_init (GnomeDItemEdit *dee)
{
	GtkWidget *page;

	dee->_priv = g_new0 (GnomeDItemEditPrivate, 1);

        page = make_easy_page (dee);
        gtk_widget_show_all (page);

        gtk_notebook_append_page (GTK_NOTEBOOK (dee), page,
				  gtk_label_new (_("Basic")));

	dee->_priv->child1 = page;

        page = make_advanced_page (dee);
        gtk_widget_show_all (page);

        gtk_notebook_append_page (GTK_NOTEBOOK (dee), page,
				  gtk_label_new (_("Advanced")));

	dee->_priv->child2 = page;

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

	destroy_tooltip (object);

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
	gtk_widget_set_sensitive (dee->_priv->type_option, ! is_directory);
	gtk_widget_set_sensitive (dee->_priv->terminal_button, ! is_directory);
	gtk_widget_set_sensitive (dee->_priv->tryexec_label, ! is_directory);
	gtk_widget_set_sensitive (dee->_priv->tryexec_entry, ! is_directory);
}

/* Conform display to ditem */
static void
gnome_ditem_edit_sync_display (GnomeDItemEdit *dee)
{
	GnomeDesktopItemType  type;
	GnomeDesktopItem    *ditem;
        GtkTreeModel        *model; 
        GtkTreeIter          iter;
        GList                *i18n_list;
        GList                *li;
        const char           *cs;
        const char           *name;
        const char           *generic_name;
        const char           *comment;
        char                 *tmpstr;

        g_return_if_fail (dee != NULL);
        g_return_if_fail (GNOME_IS_DITEM_EDIT (dee));

	ditem = dee->_priv->ditem;

	if (ditem == NULL) {
		gnome_ditem_edit_clear (dee);
		return;
	}

	type = gnome_desktop_item_get_entry_type (ditem);
        cs = gnome_desktop_item_get_string (ditem,
					    GNOME_DESKTOP_ITEM_TYPE);
	if (type == GNOME_DESKTOP_ITEM_TYPE_DIRECTORY ||
	    dee->_priv->directory_only) {
		gnome_ditem_set_directory_sensitive (dee, TRUE);
		setup_option (dee, ONLY_DIRECTORY, cs);
	} else {
		gnome_ditem_set_directory_sensitive (dee, FALSE);
		setup_option (dee, ALL_EXCEPT_DIRECTORY, cs);
	}

        name = gnome_desktop_item_get_localestring (
			ditem, GNOME_DESKTOP_ITEM_NAME);
        gtk_entry_set_text (GTK_ENTRY (dee->_priv->name_entry), 
			    name ? name : "");

        generic_name = gnome_desktop_item_get_localestring (
				ditem, GNOME_DESKTOP_ITEM_GENERIC_NAME);
        gtk_entry_set_text (GTK_ENTRY (dee->_priv->generic_name_entry), 
			    generic_name ? generic_name : "");

        comment = gnome_desktop_item_get_localestring (
				ditem, GNOME_DESKTOP_ITEM_COMMENT);
        gtk_entry_set_text (GTK_ENTRY (dee->_priv->comment_entry),
			    comment ? comment : "");

	if (type == GNOME_DESKTOP_ITEM_TYPE_LINK) {
		cs = gnome_desktop_item_get_string (ditem,
						    GNOME_DESKTOP_ITEM_URL);
	} else {
		cs = gnome_desktop_item_get_string (ditem,
						    GNOME_DESKTOP_ITEM_EXEC);
	}
        gnome_file_entry_set_filename
		(GNOME_FILE_ENTRY (dee->_priv->exec_entry), cs ? cs : "");

        cs = gnome_desktop_item_get_string (ditem,
					    GNOME_DESKTOP_ITEM_TRY_EXEC);
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->tryexec_entry), 
                           cs ? cs : "");

	cs = gnome_desktop_item_get_string (ditem, GNOME_DESKTOP_ITEM_ICON);
        tmpstr = gnome_desktop_item_get_icon (ditem, NULL);
	gnome_icon_entry_set_filename (GNOME_ICON_ENTRY (dee->_priv->icon_entry), tmpstr);

	g_free (dee->_priv->icon_theme_dir);
	if (cs != NULL &&  ! g_path_is_absolute (cs)) {
		/* this is a themed icon, see bug #119208 */
		dee->_priv->icon_theme_dir = g_path_get_dirname (tmpstr);
		/* FIXME: what about theme changes when the dialog is up */
	} else {
		/* use the default pixmap directory as the standard icon_theme_dir,
		 * since the standard directory is themed */
		g_object_get (G_OBJECT (dee->_priv->icon_entry), "pixmap_subdir",
			      &(dee->_priv->icon_theme_dir), NULL);
	}

	g_free (tmpstr);

        cs = gnome_desktop_item_get_string (ditem, "X-GNOME-DocPath");
        gtk_entry_set_text (GTK_ENTRY (dee->_priv->doc_entry), cs ? cs : "");

        gtk_toggle_button_set_active
		(GTK_TOGGLE_BUTTON (dee->_priv->terminal_button),
		 gnome_desktop_item_get_boolean (ditem,
						 GNOME_DESKTOP_ITEM_TERMINAL));

        /*set the names and comments from our i18n list*/
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (dee->_priv->translations));
        gtk_list_store_clear (GTK_LIST_STORE (model));
        i18n_list = gnome_desktop_item_get_languages (ditem, NULL);
	for (li = i18n_list; li != NULL; li = li->next) {
                const char *name, *comment, *generic_name;
                const char *lang = li->data;

                name = gnome_desktop_item_get_localestring_lang
			(ditem, GNOME_DESKTOP_ITEM_NAME, lang);
                generic_name = gnome_desktop_item_get_localestring_lang
			(ditem, GNOME_DESKTOP_ITEM_GENERIC_NAME, lang);
                comment = gnome_desktop_item_get_localestring_lang
			(ditem, GNOME_DESKTOP_ITEM_COMMENT, lang);
                
		/* only include a language in the list if it 
		 * has a useful translation
		 */
		if (name || generic_name || comment) {
			gtk_list_store_append (GTK_LIST_STORE(model), &iter);
			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
					    0, lang ? lang : "",
					    1, name ? name : "",
					    2, generic_name ? generic_name : "",
					    3, comment ? comment : "",
					    -1);
		}
        }
        g_list_free (i18n_list);

	/* clear the entries for add/remove */
        gtk_entry_set_text (GTK_ENTRY (dee->_priv->transl_lang_entry), "");
        gtk_entry_set_text (GTK_ENTRY (dee->_priv->transl_name_entry), "");
        gtk_entry_set_text (GTK_ENTRY (dee->_priv->transl_generic_name_entry), "");
        gtk_entry_set_text (GTK_ENTRY (dee->_priv->transl_comment_entry), "");

	/* ui can't be dirty, I mean, damn we just synced it from the ditem */
	dee->_priv->ui_dirty = FALSE;
}

static const char *
get_language (void)
{
	/* there is some include header problem and we can't include the
	   gnome i18n header or some such */
	extern const GList *gnome_i18n_get_language_list (const char *category);

	const GList *list;
	const GList *l;

	list = gnome_i18n_get_language_list ("LC_MESSAGES");

	for (l = list; l; l = l->next)
		if (!strchr (l->data, '.'))
			return (char *) l->data;

	return NULL;
}

static void
gnome_ditem_edit_sync_ditem (GnomeDItemEdit *dee)
{
	GnomeDesktopItem *ditem;
	GtkTreeModel     *model;
	GtkTreeIter       iter;
	GtkWidget        *entry;
	const char       *type;
	const char       *uri;
	const char       *attr;
	char             *file;
	gboolean          ret;

	g_return_if_fail (dee != NULL);
	g_return_if_fail (GNOME_IS_DITEM_EDIT (dee));

	if (!dee->_priv->ditem)
		dee->_priv->ditem = gnome_desktop_item_new ();

	ditem = dee->_priv->ditem;

	entry = gnome_file_entry_gtk_entry
		(GNOME_FILE_ENTRY (dee->_priv->exec_entry));
	uri = gtk_entry_get_text (GTK_ENTRY (entry));

	type = get_type_from_option (dee);
	gnome_desktop_item_set_string (ditem, GNOME_DESKTOP_ITEM_TYPE, type);

	/* hack really */
	if (type && !strcmp (type, "Link"))
		attr = GNOME_DESKTOP_ITEM_URL;
	else
		attr  = GNOME_DESKTOP_ITEM_EXEC;

	gnome_desktop_item_set_string (ditem, attr, uri);

	gnome_desktop_item_set_string (
		ditem, GNOME_DESKTOP_ITEM_TRY_EXEC,
		gtk_entry_get_text (GTK_ENTRY (dee->_priv->tryexec_entry)));
  
	file = gnome_icon_entry_get_filename (
			GNOME_ICON_ENTRY (dee->_priv->icon_entry));
	if (file != NULL && file[0] != '\0') {
		/* if the icon_theme_dir is the same as the directory name of this
		   icon, then just use the basename as we've just picked another
		   icon from the theme.  See bug #119208 */
		char *dn = g_path_get_dirname (file);
		if (dee->_priv->icon_theme_dir != NULL &&
		    strcmp (dn, dee->_priv->icon_theme_dir) == 0) {
			char *base = g_path_get_basename (file);
			g_free (file);
			file = base;
		}
		g_free (dn);
	}
	gnome_desktop_item_set_string (ditem, GNOME_DESKTOP_ITEM_ICON, file);
	g_free (file);

	gnome_desktop_item_set_string (
			ditem, GNOME_DESKTOP_ITEM_DOC_PATH,
			gtk_entry_get_text (GTK_ENTRY (dee->_priv->doc_entry)));

	gnome_desktop_item_set_boolean (
			ditem, GNOME_DESKTOP_ITEM_TERMINAL,
			GTK_TOGGLE_BUTTON (dee->_priv->terminal_button)->active);

	gnome_desktop_item_clear_localestring (
			ditem, GNOME_DESKTOP_ITEM_NAME);
	gnome_desktop_item_clear_localestring (
			ditem, GNOME_DESKTOP_ITEM_GENERIC_NAME);
	gnome_desktop_item_clear_localestring (
			ditem, GNOME_DESKTOP_ITEM_COMMENT);

	model = gtk_tree_view_get_model (
			GTK_TREE_VIEW (dee->_priv->translations));

	ret = gtk_tree_model_get_iter_first (model, &iter);
	while (ret) {
		char *lang;
		char *name;
		char *comment;
		char *generic_name;

		gtk_tree_model_get (
			model, &iter, 0, &lang, 1, &name, 2, &generic_name, 3, &comment, -1);

		if (!name && !comment) {
			g_free (lang);
			ret = gtk_tree_model_iter_next (model, &iter);
			continue;
		}

		if (!lang)
			lang = g_strdup (get_language ());

		gnome_desktop_item_set_localestring_lang (
			ditem, GNOME_DESKTOP_ITEM_NAME, lang, name);
		gnome_desktop_item_set_localestring_lang (
			ditem, GNOME_DESKTOP_ITEM_GENERIC_NAME, lang, generic_name);
		gnome_desktop_item_set_localestring_lang (
			ditem, GNOME_DESKTOP_ITEM_COMMENT, lang, comment);
		
		g_free (name);
		g_free (generic_name);
		g_free (comment);
		g_free (lang);

		ret = gtk_tree_model_iter_next (model, &iter);
	}

	gnome_desktop_item_set_localestring (
			ditem, GNOME_DESKTOP_ITEM_NAME,
			gtk_entry_get_text (GTK_ENTRY(dee->_priv->name_entry)));

	gnome_desktop_item_set_localestring (
			ditem, GNOME_DESKTOP_ITEM_GENERIC_NAME,
			gtk_entry_get_text (GTK_ENTRY(dee->_priv->generic_name_entry)));

	gnome_desktop_item_set_localestring (
			ditem, GNOME_DESKTOP_ITEM_COMMENT,
			gtk_entry_get_text (GTK_ENTRY(dee->_priv->comment_entry)));
			
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

        if (!newentry)
		return FALSE;

	if (dee->_priv->ditem)
		gnome_desktop_item_unref (dee->_priv->ditem);

	dee->_priv->ditem = newentry;
	dee->_priv->ui_dirty = TRUE;

	gnome_ditem_edit_sync_display (dee);

	return TRUE;
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

	if (dee->_priv->ditem)
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

	if (!dee->_priv->ditem) {
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
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->generic_name_entry), "");
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->comment_entry),"");
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->exec_entry), "");  
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->tryexec_entry), "");
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->doc_entry), "");
        gnome_icon_entry_set_filename (GNOME_ICON_ENTRY (dee->_priv->icon_entry), "");
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->transl_lang_entry), "");
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->transl_name_entry), "");
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->transl_generic_name_entry), "");
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->transl_comment_entry), "");
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (dee->_priv->translations));
        gtk_list_store_clear (GTK_LIST_STORE (model));
	/* set everything to non-directory type which means any type */
	gnome_ditem_set_directory_sensitive (dee, FALSE);

	/* put all our possibilities here */
	setup_option (dee, ALL_TYPES, NULL /* select */);
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
	const char *prev;

        g_return_if_fail (dee != NULL);
        g_return_if_fail (GNOME_IS_DITEM_EDIT (dee));
        g_return_if_fail (type != NULL);

	prev = get_type_from_option (dee);
	if (prev != NULL &&
	    strcmp (prev, type) == 0)
		return;

	if (dee->_priv->directory_only) {
		gnome_ditem_set_directory_sensitive (dee, TRUE);
		setup_option (dee, ONLY_DIRECTORY, type);
		g_signal_emit (dee, ditem_edit_signals [CHANGED], 0);
	} else {
		gnome_ditem_set_directory_sensitive (dee, FALSE);
		setup_option (dee, ALL_EXCEPT_DIRECTORY, type);
		g_signal_emit (dee, ditem_edit_signals [CHANGED], 0);
	}
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
			gnome_ditem_edit_set_entry_type (dee, "Directory");
		} else if (dee->_priv->ditem != NULL) {
			const char *cs = NULL;
			GnomeDesktopItemType type;
			type = gnome_desktop_item_get_entry_type
				(dee->_priv->ditem);
			cs = gnome_desktop_item_get_string
				(dee->_priv->ditem, GNOME_DESKTOP_ITEM_TYPE);
			if (type == GNOME_DESKTOP_ITEM_TYPE_DIRECTORY) {
				gnome_ditem_set_directory_sensitive (dee, TRUE);
				setup_option (dee, ONLY_DIRECTORY, cs);
			} else {
				gnome_ditem_set_directory_sensitive (dee, FALSE);
				setup_option (dee, ALL_EXCEPT_DIRECTORY, cs);
			}
		} else {
			const char *type = get_type_from_option (dee);
			gnome_ditem_set_directory_sensitive (dee, FALSE);
			setup_option (dee, ALL_TYPES, type);
		}
	}
}

/**
 * set_relation
 * @widget : The Gtk widget which is labelled by @label
 * @label : The label for the @widget.
 * Description : This function establishes atk relation
 * between a gtk widget and a label.
 */
static void
set_relation (GtkWidget *widget, GtkLabel *label)
{
	AtkObject *aobject;
	AtkRelationSet *relation_set;
	AtkRelation *relation;
	AtkObject *targets[1];

	g_return_if_fail (GTK_IS_WIDGET(widget));
	g_return_if_fail (GTK_IS_LABEL(label));

	aobject = gtk_widget_get_accessible (widget);

	/* Return if GAIL is not loaded */
	if (! GTK_IS_ACCESSIBLE (aobject))
		return;

	/* Set the ATK_RELATION_LABEL_FOR relation */
	gtk_label_set_mnemonic_widget (label, widget);

	targets[0] = gtk_widget_get_accessible (GTK_WIDGET (label));

	relation_set = atk_object_ref_relation_set (aobject);

	relation = atk_relation_new (targets, 1, ATK_RELATION_LABELLED_BY);
	atk_relation_set_add (relation_set, relation);
	g_object_unref (G_OBJECT (relation));
}

static void
destroy_tooltip (GtkObject *object)
{
	GtkTooltips *tooltips;

	tooltips = g_object_get_data (G_OBJECT (object), "tooltips");
	if (tooltips) {
		g_object_unref (tooltips);
		g_object_set_data (G_OBJECT (object), "tooltips", NULL);
	}
}

/**
 * set_tooltip
 * Description : Set @description as the tooltip for the @widget.
 *		 Creates the GtkTooltips structure if not already present.
 */
static void
set_tooltip (GnomeDItemEdit *dee, GtkWidget *widget, const gchar *description)
{
        GtkTooltips *tooltips;

	tooltips = g_object_get_data (G_OBJECT (dee), "tooltips");

	/* create if not already present */
	if (!tooltips) {
		tooltips = gtk_tooltips_new ();
		g_return_if_fail (tooltips != NULL);
		g_object_ref (tooltips);
		gtk_object_sink (GTK_OBJECT (tooltips));
		g_object_set_data (G_OBJECT (dee), "tooltips", tooltips);
	}

        gtk_tooltips_set_tip (tooltips, widget, description, NULL);
}
