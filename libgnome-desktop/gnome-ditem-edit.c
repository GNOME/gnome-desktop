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
/* FIXME: this needs to be localized */
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
			 GtkNotebook, gtk_notebook,
			 GTK_TYPE_NOTEBOOK)

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
                gtk_signal_new ("changed",
                                GTK_RUN_LAST,
                                GTK_CLASS_TYPE (object_class),
                                GTK_SIGNAL_OFFSET (GnomeDItemEditClass, changed),
                                gtk_signal_default_marshaller,
                                GTK_TYPE_NONE, 0);

        ditem_edit_signals[ICON_CHANGED] =
                gtk_signal_new ("icon_changed",
                                GTK_RUN_LAST,
                                GTK_CLASS_TYPE (object_class),
                                GTK_SIGNAL_OFFSET (GnomeDItemEditClass, 
                                                   icon_changed),
                                gtk_signal_default_marshaller,
                                GTK_TYPE_NONE, 0);

        ditem_edit_signals[NAME_CHANGED] =
                gtk_signal_new ("name_changed",
                                GTK_RUN_LAST,
                                GTK_CLASS_TYPE (object_class),
                                GTK_SIGNAL_OFFSET (GnomeDItemEditClass, 
                                                   name_changed),
                                gtk_signal_default_marshaller,
                                GTK_TYPE_NONE, 0);

        object_class->destroy = gnome_ditem_edit_destroy;
        gobject_class->finalize = gnome_ditem_edit_finalize;
        ditem_edit_class->changed = NULL;
}

enum {
	ALL_TYPES,
	ONLY_DIRECTORY,
	ALL_EXCEPT_DIRECTORY
};

static void
setup_combo (GnomeDItemEdit *dee, int type, const char *extra)
{
	GList *types = NULL;

	if (type == ONLY_DIRECTORY) {
		types = g_list_prepend (types, "Directory");
	} else {
		types = g_list_prepend (types, "Application");
		types = g_list_prepend (types, "Link");
		types = g_list_prepend (types, "FSDevice");
		types = g_list_prepend (types, "MimeType");
		if (type != ALL_EXCEPT_DIRECTORY)
			types = g_list_prepend (types, "Directory");
		types = g_list_prepend (types, "Service");
		types = g_list_prepend (types, "ServiceType");
	}
	if (extra != NULL)
		types = g_list_prepend (types, (char *)extra);

	gtk_combo_set_popdown_strings
		(GTK_COMBO (dee->_priv->type_combo), types);

	g_list_free (types);
}

static void 
table_attach_entry(GtkTable * table, GtkWidget * w,
		   gint l, gint r, gint t, gint b)
{
        gtk_table_attach(table, w, l, r, t, b,
                         GTK_EXPAND | GTK_FILL | GTK_SHRINK,
                         GTK_FILL,
                         0, 0);
}

static void
table_attach_label(GtkTable * table, GtkWidget * w,
		   gint l, gint r, gint t, gint b)
{
        gtk_table_attach(table, w, l, r, t, b,
                         GTK_FILL,
                         GTK_FILL,
                         0, 0);
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

static void
fill_easy_page(GnomeDItemEdit * dee, GtkWidget * table)
{
        GtkWidget *label;
        GtkWidget *hbox;
        GtkWidget *align;

        label = label_new(_("Name:"));
        table_attach_label(GTK_TABLE(table), label, 0, 1, 0, 1);

        dee->_priv->name_entry = gtk_entry_new();
        table_attach_entry(GTK_TABLE(table),dee->_priv->name_entry, 1, 2, 0, 1);
        gtk_signal_connect_object_while_alive(GTK_OBJECT(dee->_priv->name_entry), "changed",
                                              GTK_SIGNAL_FUNC(gnome_ditem_edit_changed),
                                              GTK_OBJECT(dee));
        gtk_signal_connect_object_while_alive(GTK_OBJECT(dee->_priv->name_entry), "changed",
                                              GTK_SIGNAL_FUNC(gnome_ditem_edit_name_changed),
                                              GTK_OBJECT(dee));


        label = label_new(_("Comment:"));
        table_attach_label(GTK_TABLE(table),label, 0, 1, 1, 2);

        dee->_priv->comment_entry = gtk_entry_new();
        table_attach_entry(GTK_TABLE(table),dee->_priv->comment_entry, 1, 2, 1, 2);
        gtk_signal_connect_object_while_alive(GTK_OBJECT(dee->_priv->comment_entry), "changed",
                                              GTK_SIGNAL_FUNC(gnome_ditem_edit_changed),
                                              GTK_OBJECT(dee));


        label = label_new(_("Command:"));
        table_attach_label(GTK_TABLE(table),label, 0, 1, 2, 3);

        dee->_priv->exec_entry = gtk_entry_new();
        table_attach_entry(GTK_TABLE(table),dee->_priv->exec_entry, 1, 2, 2, 3);
        gtk_signal_connect_object_while_alive(GTK_OBJECT(dee->_priv->exec_entry), "changed",
                                              GTK_SIGNAL_FUNC(gnome_ditem_edit_changed),
                                              GTK_OBJECT(dee));


        label = label_new(_("Type:"));
        table_attach_label(GTK_TABLE(table), label, 0, 1, 3, 4);

        dee->_priv->type_combo = gtk_combo_new();
	setup_combo (dee, ALL_TYPES, NULL);
        gtk_combo_set_value_in_list(GTK_COMBO(dee->_priv->type_combo), 
                                    FALSE, TRUE);
        table_attach_entry(GTK_TABLE(table),dee->_priv->type_combo, 1, 2, 3, 4);
        gtk_signal_connect_object_while_alive(GTK_OBJECT(GTK_COMBO(dee->_priv->type_combo)->entry), 
                                              "changed",
                                              GTK_SIGNAL_FUNC(gnome_ditem_edit_changed),
                                              GTK_OBJECT(dee));

        label = label_new(_("Icon:"));
        table_attach_label(GTK_TABLE(table), label, 0, 1, 4, 5);

        hbox = gtk_hbox_new(FALSE, GNOME_PAD_BIG);
        gtk_table_attach(GTK_TABLE(table), hbox,
                         1, 2, 4, 5,
                         GTK_EXPAND | GTK_FILL,
                         0,
                         0, 0);

        dee->_priv->icon_entry = gnome_icon_entry_new ("desktop-icon", _("Browse icons"));
	gtk_signal_connect_object (GTK_OBJECT (dee->_priv->icon_entry), "changed",
				   GTK_SIGNAL_FUNC (gnome_ditem_edit_changed),
				   GTK_OBJECT (dee));
	gtk_signal_connect_object (GTK_OBJECT (dee->_priv->icon_entry), "changed",
				   GTK_SIGNAL_FUNC (gnome_ditem_edit_icon_changed),
				   GTK_OBJECT (dee));
	gtk_box_pack_start (GTK_BOX (hbox), dee->_priv->icon_entry,
			    FALSE, FALSE, 0);

        align = gtk_alignment_new (0.0, 0.5, 0.0, 0.0);
        gtk_box_pack_start (GTK_BOX (hbox), align, FALSE, FALSE, 0);

        dee->_priv->terminal_button = gtk_check_button_new_with_label (_("Run in Terminal"));
        gtk_signal_connect_object (GTK_OBJECT (dee->_priv->terminal_button), 
				   "clicked",
				   GTK_SIGNAL_FUNC (gnome_ditem_edit_changed),
				   GTK_OBJECT (dee));
        gtk_container_add (GTK_CONTAINER (align),
			   dee->_priv->terminal_button);
}

static void
translations_select_row(GtkCList *cl, int row, int column,
			GdkEvent *event, GnomeDItemEdit *dee)
{
        char *lang;
        char *name;
        char *comment;
        gtk_clist_get_text(cl,row,0,&lang);
        gtk_clist_get_text(cl,row,1,&name);
        gtk_clist_get_text(cl,row,2,&comment);
	
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->transl_lang_entry), lang);
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->transl_name_entry), name);
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->transl_comment_entry), comment);
}

static void
translations_add(GtkWidget *button, GnomeDItemEdit *dee)
{
        int i;
        const char *lang;
        const char *name;
        const char *comment;
        const char *text[3];
        const GList *language_list;
        const char *curlang;
        GtkCList *cl = GTK_CLIST(dee->_priv->translations);

        lang = gtk_entry_get_text(GTK_ENTRY(dee->_priv->transl_lang_entry));
        name = gtk_entry_get_text(GTK_ENTRY(dee->_priv->transl_name_entry));
        comment = gtk_entry_get_text(GTK_ENTRY(dee->_priv->transl_comment_entry));
  
        g_assert(lang && name && comment);
	
        lang = g_strstrip(g_strdup(lang));
	
        /*we are setting the current language so set the easy page entries*/
        /*FIXME: do the opposite as well!, but that's not that crucial*/
        language_list = gnome_i18n_get_language_list("LC_ALL");
        curlang = language_list ? language_list->data : NULL;
        if ((curlang && strcmp(curlang,lang)==0) ||
            ((!curlang || strcmp(curlang,"C")==0) && !*lang)) {
                gtk_entry_set_text(GTK_ENTRY(dee->_priv->name_entry),name);
                gtk_entry_set_text(GTK_ENTRY(dee->_priv->comment_entry),comment);
        }

        for (i=0;i<cl->rows;i++) {
                char *s;
                gtk_clist_get_text(cl,i,0,&s);
                if (strcmp(lang,s)==0) {
                        gtk_clist_set_text(cl,i,1,name);
                        gtk_clist_set_text(cl,i,2,comment);
                        text[0] = s;
                        text[1] = name;
                        text[2] = comment;
                        gtk_signal_emit (GTK_OBJECT(dee),
                                         ditem_edit_signals[CHANGED], NULL);
                        return;
                }
        }
        text[0]=lang;
        text[1]=name;
        text[2]=comment;
        gtk_clist_append(cl,(char**)text);
        gtk_signal_emit(GTK_OBJECT(dee), ditem_edit_signals[CHANGED], NULL);
}

static void
translations_remove(GtkWidget *button, GnomeDItemEdit *dee)
{
        GtkCList *cl = GTK_CLIST(dee->_priv->translations);
        if(cl->selection == NULL)
                return;
        gtk_clist_remove(cl,GPOINTER_TO_INT(cl->selection->data));
        gtk_signal_emit(GTK_OBJECT(dee), ditem_edit_signals[CHANGED], NULL);
}

static void
fill_advanced_page(GnomeDItemEdit * dee, GtkWidget * page)
{
        GtkWidget * label;
        GtkWidget * button;
        GtkWidget * box;
        const char *transl[3];

        label = label_new(_("Try this before using:"));
        table_attach_label(GTK_TABLE(page),label, 0, 1, 0, 1);

        dee->_priv->tryexec_entry = gtk_entry_new();
        table_attach_entry(GTK_TABLE(page),dee->_priv->tryexec_entry, 1, 2, 0, 1);
        gtk_signal_connect_object(GTK_OBJECT(dee->_priv->tryexec_entry), 
                                  "changed",
                                  GTK_SIGNAL_FUNC(gnome_ditem_edit_changed),
                                  GTK_OBJECT(dee));

        label = label_new(_("Documentation:"));
        table_attach_label(GTK_TABLE(page),label, 0, 1, 1, 2);

        dee->_priv->doc_entry = gtk_entry_new_with_max_length(255);
        table_attach_entry(GTK_TABLE(page),dee->_priv->doc_entry, 1, 2, 1, 2);
        gtk_signal_connect_object(GTK_OBJECT(dee->_priv->doc_entry), 
                                  "changed",
                                  GTK_SIGNAL_FUNC(gnome_ditem_edit_changed),
                                  GTK_OBJECT(dee));

        label = gtk_label_new(_("Name/Comment translations:"));
        table_attach_label(GTK_TABLE(page),label, 0, 2, 2, 3);
  
        transl[0] = _("Language");
        transl[1] = _("Name");
        transl[2] = _("Comment");
        dee->_priv->translations = gtk_clist_new_with_titles(3,(char**)transl);
	gtk_clist_set_column_auto_resize (GTK_CLIST (dee->_priv->translations),
					  0, TRUE);
	gtk_clist_set_column_auto_resize (GTK_CLIST (dee->_priv->translations),
					  1, TRUE);
	gtk_clist_set_column_auto_resize (GTK_CLIST (dee->_priv->translations),
					  2, TRUE);
        box = gtk_scrolled_window_new(NULL,NULL);
        gtk_widget_set_usize(box,0,120);
        gtk_container_add(GTK_CONTAINER(box),dee->_priv->translations);
        table_attach_list(GTK_TABLE(page),box, 0, 2, 5, 6);
        gtk_clist_column_titles_passive(GTK_CLIST(dee->_priv->translations));
        gtk_signal_connect(GTK_OBJECT(dee->_priv->translations),"select_row",
                           GTK_SIGNAL_FUNC(translations_select_row),
                           dee);

        box = gtk_hbox_new(FALSE,GNOME_PAD_SMALL);
        table_attach_entry(GTK_TABLE(page),box, 0, 2, 3, 4);
  
        dee->_priv->transl_lang_entry = gtk_entry_new();
        gtk_box_pack_start(GTK_BOX(box),dee->_priv->transl_lang_entry,FALSE,FALSE,0);
        gtk_widget_set_usize(dee->_priv->transl_lang_entry,50,0);

        dee->_priv->transl_name_entry = gtk_entry_new();
        gtk_box_pack_start(GTK_BOX(box),dee->_priv->transl_name_entry,TRUE,TRUE,0);

        dee->_priv->transl_comment_entry = gtk_entry_new();
        gtk_box_pack_start(GTK_BOX(box),dee->_priv->transl_comment_entry,TRUE,TRUE,0);

        box = gtk_hbox_new(FALSE,GNOME_PAD_SMALL);
        table_attach_entry(GTK_TABLE(page),box, 0, 2, 4, 5);
  
        button = gtk_button_new_with_label(_("Add/Set"));
        gtk_box_pack_start(GTK_BOX(box),button,FALSE,FALSE,0);
        gtk_signal_connect(GTK_OBJECT(button),"clicked",
                           GTK_SIGNAL_FUNC(translations_add),
                           dee);
        button = gtk_button_new_with_label(_("Remove"));
        gtk_box_pack_start(GTK_BOX(box),button,FALSE,FALSE,0);
        gtk_signal_connect(GTK_OBJECT(button),"clicked",
                           GTK_SIGNAL_FUNC(translations_remove),
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

        dee = gtk_type_new (gnome_ditem_edit_get_type());

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

	GNOME_CALL_PARENT_HANDLER (GTK_OBJECT_CLASS, destroy, (object));
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

	GNOME_CALL_PARENT_HANDLER (G_OBJECT_CLASS, finalize, (object));
}

/* set sensitive for directory/other type of a ditem */
static void
gnome_ditem_set_directory_sensitive (GnomeDItemEdit *dee,
				     gboolean is_directory)
{
	gtk_widget_set_sensitive (dee->_priv->exec_entry, ! is_directory);
	gtk_widget_set_sensitive (dee->_priv->type_combo, ! is_directory);
	gtk_widget_set_sensitive (dee->_priv->terminal_button, ! is_directory);
	gtk_widget_set_sensitive (dee->_priv->tryexec_entry, ! is_directory);
}

/* Conform display to ditem */
static void
gnome_ditem_edit_sync_display (GnomeDItemEdit *dee)
{
        GList *i18n_list, *li;
	GnomeDesktopItemType type;
        const gchar* cs;
	GnomeDesktopItem *ditem;
        
        g_return_if_fail (dee != NULL);
        g_return_if_fail (GNOME_IS_DITEM_EDIT (dee));

	ditem = dee->_priv->ditem;

	if (ditem == NULL) {
		gnome_ditem_edit_clear (dee);
		return;
	}

	type = gnome_desktop_item_get_entry_type (ditem);
	if (type == GNOME_DESKTOP_ITEM_TYPE_DIRECTORY) {
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

        cs = gnome_desktop_item_get_string (ditem,
					    GNOME_DESKTOP_ITEM_EXEC);
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->exec_entry), cs ? cs : "");

        cs = gnome_desktop_item_get_string (ditem,
					    GNOME_DESKTOP_ITEM_TRY_EXEC);
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->tryexec_entry), 
                           cs ? cs : "");

        cs = gnome_desktop_item_get_icon (ditem);
	gnome_icon_entry_set_filename (GNOME_ICON_ENTRY (dee->_priv->icon_entry), cs);

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
        gtk_clist_clear (GTK_CLIST(dee->_priv->translations));
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
                gtk_clist_append (GTK_CLIST (dee->_priv->translations), (char**)text);
        }
        g_list_free (i18n_list);

	/* clear the entries for add/remove */
        gtk_entry_set_text (GTK_ENTRY (dee->_priv->transl_lang_entry), "");
        gtk_entry_set_text (GTK_ENTRY (dee->_priv->transl_name_entry), "");
        gtk_entry_set_text (GTK_ENTRY (dee->_priv->transl_comment_entry), "");

	/* ui can't be dirty, I mean, damn we just synced it from the ditem */
	dee->_priv->ui_dirty = FALSE;
}

const char *
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
        int i;
	GnomeDesktopItem *ditem;
        
        g_return_if_fail (dee != NULL);
        g_return_if_fail (GNOME_IS_DITEM_EDIT(dee));

	ditem = dee->_priv->ditem;

	if (ditem == NULL) {
		ditem = gnome_desktop_item_new ();
		dee->_priv->ditem = ditem;
	}

        text = gtk_entry_get_text(GTK_ENTRY(dee->_priv->exec_entry));
        gnome_desktop_item_set_string (ditem, GNOME_DESKTOP_ITEM_EXEC, text);

        text = gtk_entry_get_text(GTK_ENTRY(dee->_priv->tryexec_entry));
        gnome_desktop_item_set_string (ditem, GNOME_DESKTOP_ITEM_TRY_EXEC, text);
  
        text = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(dee->_priv->type_combo)->entry));
        gnome_desktop_item_set_string (ditem, GNOME_DESKTOP_ITEM_TYPE, text);
  
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
        for (i = 0; i < GTK_CLIST (dee->_priv->translations)->rows; i++) {
                const char *lang, *name, *comment;
                gtk_clist_get_text(GTK_CLIST(dee->_priv->translations),i,0,(gchar**)&lang);
                gtk_clist_get_text(GTK_CLIST(dee->_priv->translations),i,1,(gchar**)&name);
                gtk_clist_get_text(GTK_CLIST(dee->_priv->translations),i,2,(gchar**)&comment);
                if(!*lang) lang = NULL;
                if(!*name) name = NULL;
                if(!*comment) comment = NULL;
                if(!name && !comment)
                        continue;

                if (lang == NULL)
                        lang = get_language ();

                g_assert (lang != NULL);
                
                gnome_desktop_item_set_localestring_lang
			(ditem, GNOME_DESKTOP_ITEM_NAME, lang, name);
                gnome_desktop_item_set_localestring_lang
			(ditem, GNOME_DESKTOP_ITEM_NAME, lang, comment);
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
 * gnome_ditem_edit_load_file
 * @dee: #GnomeDItemEdit object to work with
 * @path: file to load into the editting areas
 *
 * Description: Load a .desktop file and update the editting areas
 * of the object accordingly.
 *
 * Returns:  %TRUE if successful, %FALSE otherwise
 */
gboolean
gnome_ditem_edit_load_file (GnomeDItemEdit *dee,
			    const gchar *path,
			    GError **error)
{
        GnomeDesktopItem * newentry;

        g_return_val_if_fail (dee != NULL, FALSE);
        g_return_val_if_fail (GNOME_IS_DITEM_EDIT (dee), FALSE);
        g_return_val_if_fail (path != NULL, FALSE);

        newentry = gnome_desktop_item_new_from_file (path, 0, error);

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
        gtk_clist_clear(GTK_CLIST(dee->_priv->translations));

	/* set everything to non-directory type which means any type */
	gnome_ditem_set_directory_sensitive (dee, FALSE);

	/* put all our possibilities here */
	setup_combo (dee, ALL_TYPES, NULL);
}

static void
gnome_ditem_edit_changed (GnomeDItemEdit *dee)
{
	dee->_priv->ui_dirty = TRUE;
        gtk_signal_emit (GTK_OBJECT (dee), ditem_edit_signals[CHANGED], NULL);
}

static void
gnome_ditem_edit_icon_changed (GnomeDItemEdit *dee)
{
	dee->_priv->ui_dirty = TRUE;
        gtk_signal_emit (GTK_OBJECT (dee), ditem_edit_signals[ICON_CHANGED], NULL);
}

static void
gnome_ditem_edit_name_changed (GnomeDItemEdit *dee)
{
	dee->_priv->ui_dirty = TRUE;
        gtk_signal_emit (GTK_OBJECT (dee), ditem_edit_signals[NAME_CHANGED], NULL);
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

#ifdef TEST_DITEM_EDIT

#include <libgnomeui.h>

static void
changed_callback(GnomeDItemEdit *dee, gpointer data)
{
        g_print("Changed!\n");
        fflush(stdout);
}

static void
icon_changed_callback(GnomeDItemEdit *dee, gpointer data)
{
        g_print("Icon changed!\n");
        fflush(stdout);
}

static void
name_changed_callback(GnomeDItemEdit *dee, gpointer data)
{
        g_print("Name changed!\n");
        fflush(stdout);
}

int
main(int argc, char * argv[])
{
        GtkWidget * app;
        GtkWidget * notebook;
        GtkObject * dee;

        argp_program_version = VERSION;

        gnome_init ("testing ditem edit", NULL, argc, argv, 0, 0);

        app = bonobo_window_new("testing ditem edit", "Testing");

        notebook = gtk_notebook_new();

        bonobo_window_set_contents(BONOBO_WINDOW(app), notebook);

        dee = gnome_ditem_edit_new(GTK_NOTEBOOK(notebook));

        gnome_ditem_edit_load_file(GNOME_DITEM_EDIT(dee),
                                   "/usr/local/share/gnome/apps/grun.desktop");

#ifdef GNOME_ENABLE_DEBUG
        g_print("Dialog (main): %p\n", GNOME_DITEM_EDIT(dee)->icon_dialog);
#endif

        gtk_signal_connect_object(GTK_OBJECT(app), "delete_event", 
                                  GTK_SIGNAL_FUNC(gtk_widget_destroy),
                                  GTK_OBJECT(app));

        gtk_signal_connect(GTK_OBJECT(app), "destroy",
                           GTK_SIGNAL_FUNC(gtk_main_quit),
                           NULL);

        gtk_signal_connect(GTK_OBJECT(dee), "changed",
                           GTK_SIGNAL_FUNC(changed_callback),
                           NULL);

        gtk_signal_connect(GTK_OBJECT(dee), "icon_changed",
                           GTK_SIGNAL_FUNC(icon_changed_callback),
                           NULL);

        gtk_signal_connect(GTK_OBJECT(dee), "name_changed",
                           GTK_SIGNAL_FUNC(name_changed_callback),
                           NULL);

#ifdef GNOME_ENABLE_DEBUG
        g_print("Dialog (main 2): %p\n", GNOME_DITEM_EDIT(dee)->icon_dialog);
#endif

        gtk_widget_show(notebook);
        gtk_widget_show(app);

#ifdef GNOME_ENABLE_DEBUG
        g_print("Dialog (main 3): %p\n", GNOME_DITEM_EDIT(dee)->icon_dialog);
#endif

        gtk_main();

        return 0;
}

#endif

