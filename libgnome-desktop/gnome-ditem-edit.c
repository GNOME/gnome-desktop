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
#include "gnome-ditem-edit.h"

#ifdef NEED_GNOMESUPPORT_H
# include "gnomesupport.h"
#endif
#include "libgnome/libgnomeP.h"

#include "gnome-stock.h"
#include "gnome-dialog.h"
#include "gnome-uidefs.h"
#include "gnome-pixmap.h"
#include "gnome-icon-entry.h"

struct _GnomeDItemEditPrivate {
	/* use the accessors or just use new_notebook */
        GtkWidget *child1; /* basic */
        GtkWidget *child2; /* DND */
        GtkWidget *child3; /* Advanced */

        /* Remaining fields are private - if you need them, 
           please add an accessor function. */
  
	/* we keep a ditem around, since we can never have absolutely
	   everything in the display so we load a file, or get a ditem,
	   sync the display and ref the ditem */
	GnomeDesktopItem *ditem;

        GtkWidget *name_entry;
        GtkWidget *comment_entry;
        GtkWidget *exec_entry;
        GtkWidget *tryexec_entry;
        GtkWidget *doc_entry;
        GtkWidget *wmtitles_entry;

        GtkWidget *simple_dnd_toggle;

        GtkWidget *file_drop_entry;
        GtkWidget *single_file_drop_toggle;

        GtkWidget *url_drop_entry;
        GtkWidget *single_url_drop_toggle;

        GtkWidget *type_combo;

        GtkWidget *terminal_button;  

        GtkWidget *icon_entry;
  
        GtkWidget *translations;
        GtkWidget *transl_lang_entry;
        GtkWidget *transl_name_entry;
        GtkWidget *transl_comment_entry;

	/* we store this so we can set sensitive the
	   drag and drop stuff, no point in an accessor */
        GtkWidget *dndtable;
};

static void gnome_ditem_edit_class_init   (GnomeDItemEditClass *klass);
static void gnome_ditem_edit_init         (GnomeDItemEdit      *messagebox);

static void gnome_ditem_edit_destroy      (GtkObject *dee);

static void gnome_ditem_edit_sync_display (GnomeDItemEdit * dee,
					   GnomeDesktopItem * ditem);

static void gnome_ditem_edit_sync_ditem   (GnomeDItemEdit * dee,
					   GnomeDesktopItem * ditem);

static void gnome_ditem_edit_changed      (GnomeDItemEdit * dee);
static void gnome_ditem_edit_icon_changed (GnomeDItemEdit * dee);
static void gnome_ditem_edit_name_changed (GnomeDItemEdit * dee);

enum {
        CHANGED,
        ICON_CHANGED,
        NAME_CHANGED,
        LAST_SIGNAL
};

static gint ditem_edit_signals[LAST_SIGNAL] = { 0 };

static GtkObjectClass *parent_class = NULL;

guint
gnome_ditem_edit_get_type (void)
{
        static guint dee_type = 0;

        if (!dee_type)
        {
                GtkTypeInfo dee_info =
                {
                        "GnomeDItemEdit",
                        sizeof (GnomeDItemEdit),
                        sizeof (GnomeDItemEditClass),
                        (GtkClassInitFunc) gnome_ditem_edit_class_init,
                        (GtkObjectInitFunc) gnome_ditem_edit_init,
                        (GtkArgSetFunc) NULL,
                        (GtkArgGetFunc) NULL,
                };

                dee_type = gtk_type_unique (gtk_object_get_type (), &dee_info);
        }

        return dee_type;
}

static void
gnome_ditem_edit_class_init (GnomeDItemEditClass *klass)
{
        GtkObjectClass *object_class;
        GnomeDItemEditClass * ditem_edit_class;

        ditem_edit_class = (GnomeDItemEditClass*) klass;

        object_class = (GtkObjectClass*) klass;

        parent_class = gtk_type_class (gtk_object_get_type ());
  
        ditem_edit_signals[CHANGED] =
                gtk_signal_new ("changed",
                                GTK_RUN_LAST,
                                object_class->type,
                                GTK_SIGNAL_OFFSET (GnomeDItemEditClass, changed),
                                gtk_signal_default_marshaller,
                                GTK_TYPE_NONE, 0);

        ditem_edit_signals[ICON_CHANGED] =
                gtk_signal_new ("icon_changed",
                                GTK_RUN_LAST,
                                object_class->type,
                                GTK_SIGNAL_OFFSET (GnomeDItemEditClass, 
                                                   icon_changed),
                                gtk_signal_default_marshaller,
                                GTK_TYPE_NONE, 0);

        ditem_edit_signals[NAME_CHANGED] =
                gtk_signal_new ("name_changed",
                                GTK_RUN_LAST,
                                object_class->type,
                                GTK_SIGNAL_OFFSET (GnomeDItemEditClass, 
                                                   name_changed),
                                gtk_signal_default_marshaller,
                                GTK_TYPE_NONE, 0);

        gtk_object_class_add_signals (object_class, ditem_edit_signals, 
                                      LAST_SIGNAL);

        object_class->destroy = gnome_ditem_edit_destroy;
        ditem_edit_class->changed = NULL;
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
label_new (char *text)
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
        GList *types = NULL;
        GtkWidget *e;
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

        types = g_list_prepend(types, "Directory");
        types = g_list_prepend(types, "URL");
        types = g_list_prepend(types, "PanelApplet");
        types = g_list_prepend(types, "Application");
        dee->_priv->type_combo = gtk_combo_new();
        gtk_combo_set_popdown_strings(GTK_COMBO(dee->_priv->type_combo), types);
        g_list_free(types);
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

        dee->_priv->icon_entry = gnome_icon_entry_new("icon",_("Choose an icon"));
        e = gnome_icon_entry_gtk_entry(GNOME_ICON_ENTRY(dee->_priv->icon_entry));
        gtk_signal_connect_object_while_alive(GTK_OBJECT(e),"changed",
                                              GTK_SIGNAL_FUNC(gnome_ditem_edit_changed),
                                              GTK_OBJECT(dee));
        gtk_signal_connect_object_while_alive(GTK_OBJECT(e),"changed",
                                              GTK_SIGNAL_FUNC(gnome_ditem_edit_icon_changed),
                                              GTK_OBJECT(dee));
        gtk_box_pack_start(GTK_BOX(hbox), dee->_priv->icon_entry, FALSE, FALSE, 0);

        align = gtk_alignment_new(0.0, 0.5, 0.0, 0.0);
        gtk_box_pack_start(GTK_BOX(hbox), align, FALSE, FALSE, 0);

        dee->_priv->terminal_button = gtk_check_button_new_with_label (_("Run in Terminal"));
        gtk_signal_connect_object(GTK_OBJECT(dee->_priv->terminal_button), 
                                  "clicked",
                                  GTK_SIGNAL_FUNC(gnome_ditem_edit_changed),
                                  GTK_OBJECT(dee));
        gtk_container_add(GTK_CONTAINER(align), dee->_priv->terminal_button);
}

static void
simple_drag_and_drop_toggled(GtkToggleButton *tb, GnomeDItemEdit *dee)
{
	if(tb->active) {
		gtk_widget_set_sensitive(dee->_priv->dndtable,FALSE);
	} else {
		gtk_widget_set_sensitive(dee->_priv->dndtable,TRUE);
	}
	gnome_ditem_edit_changed(dee);
}

static void
fill_dnd_page(GnomeDItemEdit * dee, GtkWidget * bigtable)
{
        GtkWidget *label;
        GtkWidget *table;

        dee->_priv->simple_dnd_toggle =
		gtk_check_button_new_with_label (_("Simple drag and drop"));
        gtk_signal_connect(GTK_OBJECT(dee->_priv->simple_dnd_toggle), 
			   "toggled",
			   GTK_SIGNAL_FUNC(simple_drag_and_drop_toggled),
			   dee);
        gtk_table_attach(GTK_TABLE(bigtable), dee->_priv->simple_dnd_toggle,
                         0, 2, 0, 1,
                         GTK_EXPAND | GTK_FILL,
                         0,
                         0, 0);

	table = dee->_priv->dndtable = gtk_table_new (5, 2, FALSE);
        gtk_container_set_border_width (GTK_CONTAINER (table), GNOME_PAD_SMALL);
        gtk_table_set_row_spacings (GTK_TABLE (table), GNOME_PAD_SMALL);
        gtk_table_set_col_spacings (GTK_TABLE (table), GNOME_PAD_SMALL);

        gtk_table_attach(GTK_TABLE(bigtable), table,
                         0, 2, 1, 2,
                         GTK_EXPAND | GTK_FILL,
                         GTK_EXPAND | GTK_FILL,
                         0, 0);

        label = label_new(_("In the following commands you should use %s in "
			    "the spot where the\nfilename/url should be "
			    "substituted.  If you leave the field blank\n"
			    "drops will not be accepted.  If this app"
			    "can just accept\nmultiple files or urls after "
			    "it's command check the above box."));
        gtk_table_attach(GTK_TABLE(table), label,
                         0, 2, 1, 2,
			 0,
                         0,
                         0, 0);

        label = label_new(_("Command for file drops:"));
        table_attach_label(GTK_TABLE(table),label, 0, 1, 2, 3);

        dee->_priv->file_drop_entry = gtk_entry_new();
        table_attach_entry(GTK_TABLE(table),dee->_priv->file_drop_entry, 1, 2, 2, 3);
        gtk_signal_connect_object_while_alive(GTK_OBJECT(dee->_priv->file_drop_entry), "changed",
                                              GTK_SIGNAL_FUNC(gnome_ditem_edit_changed),
                                              GTK_OBJECT(dee));

        dee->_priv->single_file_drop_toggle =
		gtk_check_button_new_with_label (_("Drop one file at a time "
						   "only."));
        gtk_signal_connect_object(GTK_OBJECT(dee->_priv->single_file_drop_toggle), 
                                  "clicked",
                                  GTK_SIGNAL_FUNC(gnome_ditem_edit_changed),
                                  GTK_OBJECT(dee));
        gtk_table_attach(GTK_TABLE(table), dee->_priv->single_file_drop_toggle,
                         1, 2, 3, 4,
                         GTK_EXPAND | GTK_FILL,
                         0,
                         0, 0);

        label = label_new(_("Command for URL drops:"));
        table_attach_label(GTK_TABLE(table),label, 0, 1, 4, 5);

        dee->_priv->url_drop_entry = gtk_entry_new();
        table_attach_entry(GTK_TABLE(table),dee->_priv->url_drop_entry, 1, 2, 4, 5);
        gtk_signal_connect_object_while_alive(GTK_OBJECT(dee->_priv->url_drop_entry), "changed",
                                              GTK_SIGNAL_FUNC(gnome_ditem_edit_changed),
                                              GTK_OBJECT(dee));

        dee->_priv->single_url_drop_toggle =
		gtk_check_button_new_with_label (_("Drop one URL at a time "
						   "only."));
        gtk_signal_connect_object(GTK_OBJECT(dee->_priv->single_url_drop_toggle), 
                                  "clicked",
                                  GTK_SIGNAL_FUNC(gnome_ditem_edit_changed),
                                  GTK_OBJECT(dee));
        gtk_table_attach(GTK_TABLE(table), dee->_priv->single_url_drop_toggle,
                         1, 2, 5, 6,
                         GTK_EXPAND | GTK_FILL,
                         0,
                         0, 0);
}

static void
set_list_width(GtkWidget *clist, const char * const text[])
{
        int i;
        int cols = GTK_CLIST(clist)->columns;
        GtkCList *cl = GTK_CLIST(clist);
        for(i=0;i<cols;i++) {
                int w = gdk_string_width(clist->style->font,text[i]);
                if(cl->column[i].width < w)
                        gtk_clist_set_column_width(cl,i,w);
        }
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
        char *lang;
        char *name;
        char *comment;
        const char *text[3];
        GList *language_list;
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
                        set_list_width (GTK_WIDGET(cl), text);
                        gtk_signal_emit (GTK_OBJECT(dee),
                                         ditem_edit_signals[CHANGED], NULL);
                        g_free (lang);
                        return;
                }
        }
        text[0]=lang;
        text[1]=name;
        text[2]=comment;
        set_list_width(GTK_WIDGET(cl),text);
        gtk_clist_append(cl,(char**)text);
        gtk_signal_emit(GTK_OBJECT(dee), ditem_edit_signals[CHANGED], NULL);
  
        g_free(lang);
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

        label = label_new(_("Window titles to wait for (comma separated):"));
        table_attach_label(GTK_TABLE(page),label, 0, 2, 2, 3);

        dee->_priv->wmtitles_entry = gtk_entry_new_with_max_length(255);
        table_attach_entry(GTK_TABLE(page),dee->_priv->wmtitles_entry, 0, 2, 3, 4);
        gtk_signal_connect_object(GTK_OBJECT(dee->_priv->wmtitles_entry), 
                                  "changed",
                                  GTK_SIGNAL_FUNC(gnome_ditem_edit_changed),
                                  GTK_OBJECT(dee));

        label = gtk_label_new(_("Name/Comment translations:"));
        table_attach_label(GTK_TABLE(page),label, 0, 2, 4, 5);
  
        transl[0] = _("Language");
        transl[1] = _("Name");
        transl[2] = _("Comment");
        dee->_priv->translations = gtk_clist_new_with_titles(3,(char**)transl);
        set_list_width(dee->_priv->translations,transl);
        box = gtk_scrolled_window_new(NULL,NULL);
        gtk_widget_set_usize(box,0,120);
        gtk_container_add(GTK_CONTAINER(box),dee->_priv->translations);
        table_attach_list(GTK_TABLE(page),box, 0, 2, 5, 6);
        gtk_clist_column_titles_passive(GTK_CLIST(dee->_priv->translations));
        gtk_signal_connect(GTK_OBJECT(dee->_priv->translations),"select_row",
                           GTK_SIGNAL_FUNC(translations_select_row),
                           dee);

        box = gtk_hbox_new(FALSE,GNOME_PAD_SMALL);
        table_attach_entry(GTK_TABLE(page),box, 0, 2, 6, 7);
  
        dee->_priv->transl_lang_entry = gtk_entry_new();
        gtk_box_pack_start(GTK_BOX(box),dee->_priv->transl_lang_entry,FALSE,FALSE,0);
        gtk_widget_set_usize(dee->_priv->transl_lang_entry,50,0);

        dee->_priv->transl_name_entry = gtk_entry_new();
        gtk_box_pack_start(GTK_BOX(box),dee->_priv->transl_name_entry,TRUE,TRUE,0);

        dee->_priv->transl_comment_entry = gtk_entry_new();
        gtk_box_pack_start(GTK_BOX(box),dee->_priv->transl_comment_entry,TRUE,TRUE,0);

        box = gtk_hbox_new(FALSE,GNOME_PAD_SMALL);
        table_attach_entry(GTK_TABLE(page),box, 0, 2, 7, 8);
  
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
make_page(void)
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
gnome_ditem_edit_init (GnomeDItemEdit *dee)
{
	dee->_priv = g_new0(GnomeDItemEditPrivate, 1);

        dee->_priv->child1 = make_page();
        fill_easy_page(dee, GTK_BIN(dee->_priv->child1)->child);
        gtk_widget_show_all(dee->_priv->child1);

        dee->_priv->child2 = make_page();
        fill_dnd_page(dee, GTK_BIN(dee->_priv->child2)->child);
        gtk_widget_show_all(dee->_priv->child2);

        dee->_priv->child3 = make_page();
        fill_advanced_page(dee, GTK_BIN(dee->_priv->child3)->child);
        gtk_widget_show_all(dee->_priv->child3);
}


/**
 * gnome_ditem_edit_new
 *
 * Description: Creates a new #GnomeDItemEdit object. The object is not
 * a widget, but just an object which creates some widgets which you have
 * to add to a notebook. Use the #gnome_ditem_edit_new_notebook to add
 * pages to the notebook.  If you use this, make sure to take care of
 * the refcounts of this object correctly.  You should ref/sink it when
 * you create it and you should also unref it when your dialog dies.
 *
 * Returns: Newly-created #GnomeDItemEdit object.
 */
GtkObject *
gnome_ditem_edit_new (void)
{
        GnomeDItemEdit * dee;

        dee = gtk_type_new(gnome_ditem_edit_get_type());

        return GTK_OBJECT (dee);
}

/**
 * gnome_ditem_edit_child1
 * @dee: #GnomeDItemEdit object to work with
 *
 * Description: Get the widget pointer to the first page.  This
 * is the "basic" page.
 *
 * Returns: a pointer to a widget
 */
GtkWidget *
gnome_ditem_edit_child1(GnomeDItemEdit  *dee)
{
        g_return_val_if_fail(dee != NULL, NULL);
        g_return_val_if_fail(GNOME_IS_DITEM_EDIT(dee), NULL);

	return dee->_priv->child1;
}

/**
 * gnome_ditem_edit_child2
 * @dee: #GnomeDItemEdit object to work with
 *
 * Description: Get the widget pointer to the second page.  This is the
 * "Drag and Drop" page.
 *
 * Returns: a pointer to a widget
 */
GtkWidget *
gnome_ditem_edit_child2(GnomeDItemEdit  *dee)
{
        g_return_val_if_fail(dee != NULL, NULL);
        g_return_val_if_fail(GNOME_IS_DITEM_EDIT(dee), NULL);

	return dee->_priv->child2;
}

/**
 * gnome_ditem_edit_child3
 * @dee: #GnomeDItemEdit object to work with
 *
 * Description: Get the widget pointer to the third page.  This is the
 * "advanced" page.
 *
 * Returns: a pointer to a widget
 */
GtkWidget *
gnome_ditem_edit_child3(GnomeDItemEdit  *dee)
{
        g_return_val_if_fail(dee != NULL, NULL);
        g_return_val_if_fail(GNOME_IS_DITEM_EDIT(dee), NULL);

	return dee->_priv->child3;
}


/**
 * gnome_ditem_edit_new_notebook
 * @notebook: notebook to add the pages to
 *
 * Description: Creates a new #GnomeDItemEdit object and adds it's pages
 * to the @notebook specified in the parameter.  The object is reffed and
 * sunk.  When the notebook dies it will be unreffed.  In effect, the notebook
 * takes ownership of the #GnomeDItemEdit.
 *
 * Returns: Newly-created #GnomeDItemEdit object.
 */
GtkObject *
gnome_ditem_edit_new_notebook (GtkNotebook *notebook)
{
        GnomeDItemEdit * dee;

        g_return_val_if_fail(notebook != NULL, NULL);
        g_return_val_if_fail(GTK_IS_NOTEBOOK(notebook), NULL);
  
        dee = GNOME_DITEM_EDIT(gnome_ditem_edit_new());

	gtk_object_ref(GTK_OBJECT(dee));
	gtk_object_sink(GTK_OBJECT(dee));

        gtk_notebook_append_page (GTK_NOTEBOOK(notebook), 
                                  dee->_priv->child1, 
                                  gtk_label_new(_("Basic")));

        gtk_notebook_append_page (GTK_NOTEBOOK(notebook), 
                                  dee->_priv->child2, 
                                  gtk_label_new(_("Drag and Drop")));

        gtk_notebook_append_page (GTK_NOTEBOOK(notebook), 
                                  dee->_priv->child3, 
                                  gtk_label_new(_("Advanced")));

        /* Destroy self with the notebook. */
        gtk_signal_connect_object_while_alive(GTK_OBJECT(notebook), "destroy",
					      GTK_SIGNAL_FUNC(gtk_object_unref),
					      GTK_OBJECT(dee));

        return GTK_OBJECT (dee);
}

static void
gnome_ditem_edit_destroy (GtkObject *dee)
{
        GnomeDItemEdit *de;
        g_return_if_fail(dee != NULL);
        g_return_if_fail(GNOME_IS_DITEM_EDIT(dee));

        de = GNOME_DITEM_EDIT(dee);

	if(de->_priv->ditem)
		gnome_desktop_item_unref(de->_priv->ditem);
	de->_priv->ditem = NULL; /* just for sanity */

        if (GTK_OBJECT_CLASS(parent_class)->destroy)
                (* (GTK_OBJECT_CLASS(parent_class)->destroy))(dee);

	g_free(de->_priv);
	de->_priv = NULL;
}

/* set sensitive for directory/other type of a ditem */
static void
gnome_ditem_set_directory_sensitive(GnomeDItemEdit *dee,
				    gboolean is_directory)
{
	gtk_widget_set_sensitive(dee->_priv->exec_entry, !is_directory);
	gtk_widget_set_sensitive(dee->_priv->type_combo, !is_directory);
	gtk_widget_set_sensitive(dee->_priv->terminal_button, !is_directory);
	gtk_widget_set_sensitive(dee->_priv->tryexec_entry, !is_directory);
	gtk_widget_set_sensitive(dee->_priv->wmtitles_entry, !is_directory);
	gtk_widget_set_sensitive(dee->_priv->simple_dnd_toggle, !is_directory);
	gtk_widget_set_sensitive(dee->_priv->dndtable, !is_directory);
}

/* Conform display to ditem */
static void
gnome_ditem_edit_sync_display(GnomeDItemEdit *dee,
                              GnomeDesktopItem *ditem)
{
        GSList *i18n_list,*li;
        const gchar* cs;
        
        g_return_if_fail(dee != NULL);
        g_return_if_fail(GNOME_IS_DITEM_EDIT(dee));
        g_return_if_fail(ditem != NULL);

        cs = gnome_desktop_item_get_type(ditem);
	if(cs && strcmp(cs,"Directory")==0) {
		GList *types = NULL;
		gnome_ditem_set_directory_sensitive(dee, TRUE);
		types = g_list_prepend(types, "Directory");
		gtk_combo_set_popdown_strings(GTK_COMBO(dee->_priv->type_combo),
					      types);
		g_list_free(types);
	} else {
		GList *types = NULL;
		gnome_ditem_set_directory_sensitive(dee, FALSE);
		types = g_list_prepend(types, "URL");
		types = g_list_prepend(types, "PanelApplet");
		types = g_list_prepend(types, "Application");
		if(cs &&
		   strcmp(cs,"URL") != 0 &&
		   strcmp(cs,"PanelApplet") != 0 &&
		   strcmp(cs,"Application") != 0)
			types = g_list_prepend(types, (char *)cs);
		gtk_combo_set_popdown_strings(GTK_COMBO(dee->_priv->type_combo),
					      types);
		g_list_free(types);
	}

        cs = gnome_desktop_item_get_local_name(ditem);
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->name_entry), 
                           cs ? cs : "");

        cs = gnome_desktop_item_get_local_comment(ditem);
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->comment_entry),
                           cs ? cs : "");

        cs = gnome_desktop_item_get_command(ditem);
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->exec_entry), cs ? cs : "");

        cs = gnome_desktop_item_get_attribute(ditem, "TryExec");
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->tryexec_entry), 
                           cs ? cs : "");

        cs = gnome_desktop_item_get_icon_path(ditem);
        gnome_icon_entry_set_filename(GNOME_ICON_ENTRY(dee->_priv->icon_entry), cs);

        cs = gnome_desktop_item_get_attribute(ditem, "DocPath"); /* FIXME check name */
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->doc_entry), 
                           cs ? cs : "");

        cs = gnome_desktop_item_get_attribute(ditem, "WMTitles");
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->wmtitles_entry), 
                           cs ? cs : "");

        cs = gnome_desktop_item_get_type(ditem);
        gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(dee->_priv->type_combo)->entry),
                           cs ? cs : "");

        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dee->_priv->terminal_button),
                                     gnome_desktop_item_get_flags(ditem) &
				      GNOME_DESKTOP_ITEM_RUN_IN_TERMINAL);

        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dee->_priv->simple_dnd_toggle),
                                     gnome_desktop_item_get_flags(ditem) &
				      GNOME_DESKTOP_ITEM_OLD_STYLE_DROP);

        cs = gnome_desktop_item_get_attribute(ditem, "FileDropExec");
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->file_drop_entry), 
                           cs ? cs : "");

        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dee->_priv->single_file_drop_toggle),
                                     gnome_desktop_item_get_flags(ditem) &
				      GNOME_DESKTOP_ITEM_SINGLE_FILE_DROP_ONLY);
	
        cs = gnome_desktop_item_get_attribute(ditem, "URLDropExec");
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->url_drop_entry), 
                           cs ? cs : "");

        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dee->_priv->single_url_drop_toggle),
                                     gnome_desktop_item_get_flags(ditem) &
				      GNOME_DESKTOP_ITEM_SINGLE_URL_DROP_ONLY);
  
        /*set the names and comments from our i18n list*/
        gtk_clist_clear (GTK_CLIST(dee->_priv->translations));
        i18n_list = gnome_desktop_item_get_languages(ditem);
        for (li=i18n_list; li; li=li->next) {
                const char *text[3];
                const gchar* lang = li->data;
                cs = lang;
                text[0] = cs ? cs : "";
                cs = gnome_desktop_item_get_name(ditem, lang);
                text[1] = cs ? cs : "";
                cs = gnome_desktop_item_get_comment(ditem, lang);
                text[2] = cs ? cs : "";
                set_list_width (dee->_priv->translations,text);
                gtk_clist_append (GTK_CLIST(dee->_priv->translations),(char**)text);
        }
        g_slist_free(i18n_list);

	/* clear the entries for add/remove */
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->transl_lang_entry), "");
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->transl_name_entry), "");
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->transl_comment_entry), "");
}

/* Conform ditem to display */
static void
gnome_ditem_edit_sync_ditem(GnomeDItemEdit *dee,
                            GnomeDesktopItem *ditem)
{
        const gchar * text;
        int i;
        const gchar* lang;
        gint curflags;
        
        g_return_if_fail(dee != NULL);
        g_return_if_fail(GNOME_IS_DITEM_EDIT(dee));
        g_return_if_fail(ditem != NULL);

        lang = gnome_i18n_get_preferred_language();
        
        text = gtk_entry_get_text(GTK_ENTRY(dee->_priv->name_entry));
        gnome_desktop_item_set_name(ditem, lang, text);

        text = gtk_entry_get_text(GTK_ENTRY(dee->_priv->comment_entry));
        gnome_desktop_item_set_comment(ditem, lang, text);

        text = gtk_entry_get_text(GTK_ENTRY(dee->_priv->exec_entry));
        gnome_desktop_item_set_command(ditem, text);

        text = gtk_entry_get_text(GTK_ENTRY(dee->_priv->tryexec_entry));
        gnome_desktop_item_set_attribute(ditem, "TryExec", text);
  
        text = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(dee->_priv->type_combo)->entry));
        gnome_desktop_item_set_type(ditem, text);
  
        gnome_desktop_item_set_icon_path(ditem,
                                         gnome_icon_entry_get_filename(GNOME_ICON_ENTRY(dee->_priv->icon_entry)));

        text = gtk_entry_get_text(GTK_ENTRY(dee->_priv->doc_entry));
        gnome_desktop_item_set_attribute(ditem, "DocPath", text);

        text = gtk_entry_get_text(GTK_ENTRY(dee->_priv->wmtitles_entry));
        gnome_desktop_item_set_attribute(ditem, "WMTitles", text);

        curflags = gnome_desktop_item_get_flags(ditem);
        if (GTK_TOGGLE_BUTTON(dee->_priv->terminal_button)->active)
                curflags |= GNOME_DESKTOP_ITEM_RUN_IN_TERMINAL;
        else
                curflags &= ~GNOME_DESKTOP_ITEM_RUN_IN_TERMINAL;
        gnome_desktop_item_set_flags(ditem, curflags);

        curflags = gnome_desktop_item_get_flags(ditem);
        if (GTK_TOGGLE_BUTTON(dee->_priv->simple_dnd_toggle)->active)
                curflags |= GNOME_DESKTOP_ITEM_OLD_STYLE_DROP;
        else
                curflags &= ~GNOME_DESKTOP_ITEM_OLD_STYLE_DROP;
        gnome_desktop_item_set_flags(ditem, curflags);

        text = gtk_entry_get_text(GTK_ENTRY(dee->_priv->file_drop_entry));
        gnome_desktop_item_set_attribute(ditem, "FileDropExec", text);

        curflags = gnome_desktop_item_get_flags(ditem);
        if (GTK_TOGGLE_BUTTON(dee->_priv->single_file_drop_toggle)->active)
                curflags |= GNOME_DESKTOP_ITEM_SINGLE_FILE_DROP_ONLY;
        else
                curflags &= ~GNOME_DESKTOP_ITEM_SINGLE_FILE_DROP_ONLY;
        gnome_desktop_item_set_flags(ditem, curflags);

        text = gtk_entry_get_text(GTK_ENTRY(dee->_priv->url_drop_entry));
        gnome_desktop_item_set_attribute(ditem, "URLDropExec", text);

        curflags = gnome_desktop_item_get_flags(ditem);
        if (GTK_TOGGLE_BUTTON(dee->_priv->single_url_drop_toggle)->active)
                curflags |= GNOME_DESKTOP_ITEM_SINGLE_URL_DROP_ONLY;
        else
                curflags &= ~GNOME_DESKTOP_ITEM_SINGLE_URL_DROP_ONLY;
        gnome_desktop_item_set_flags(ditem, curflags);
        
	gnome_desktop_item_clear_name(ditem);
	gnome_desktop_item_clear_comment(ditem);
        for(i=0;i<GTK_CLIST(dee->_priv->translations)->rows;i++) {
                const char *lang,*name,*comment;
                gtk_clist_get_text(GTK_CLIST(dee->_priv->translations),i,0,(gchar**)&lang);
                gtk_clist_get_text(GTK_CLIST(dee->_priv->translations),i,1,(gchar**)&name);
                gtk_clist_get_text(GTK_CLIST(dee->_priv->translations),i,2,(gchar**)&comment);
                if(!*lang) lang = NULL;
                if(!*name) name = NULL;
                if(!*comment) comment = NULL;
                if(!name && !comment)
                        continue;

                if (lang == NULL)
                        lang = gnome_i18n_get_preferred_language();

                g_assert(lang != NULL);
                
                gnome_desktop_item_set_name(ditem, lang, name);
                gnome_desktop_item_set_comment(ditem, lang, comment);
        }
}

/**
 * gnome_ditem_edit_load_file
 * @dee: #GnomeDItemEdit object to work with
 * @path: file to load into the editting areas
 *
 * Description: Load a .desktop file and update the editting areas
 * of the object accordingly.
 *
 * Returns:
 */
void
gnome_ditem_edit_load_file(GnomeDItemEdit *dee,
                           const gchar *path)
{
        GnomeDesktopItem * newentry;

        g_return_if_fail(dee != NULL);
        g_return_if_fail(GNOME_IS_DITEM_EDIT(dee));
        g_return_if_fail(path != NULL);

        newentry = gnome_desktop_item_new_from_file(path, 0);

        if (newentry) {
		if(dee->_priv->ditem)
			gnome_desktop_item_unref(dee->_priv->ditem);
		dee->_priv->ditem = newentry;
                gnome_ditem_edit_sync_display(dee, newentry);
        } else {
                g_warning("Failed to load file into GnomeDItemEdit");
                return;
        }
}

/**
 * gnome_ditem_edit_set_ditem
 * @dee: #GnomeDItemEdit object to work with
 * @ditem: #GnomeDesktopItem to use
 *
 * Description: Unref the existing item, and replace it with
 * this one.  This doesn't make a copy, it just refs the the passed
 * item.  It them updates the display to reflect the new item.
 * It will not make any modifications to this ditem, but you shouldn't
 * modify it either.  If you need to modify the item independently,
 * make a copy, call this function and then unref the copy (this function
 * will make it's own reference so it won't kill it but transfer
 * ownership here)
 *
 * Returns:
 */
void
gnome_ditem_edit_set_ditem(GnomeDItemEdit *dee,
                           GnomeDesktopItem *ditem)
{
        g_return_if_fail(dee != NULL);
        g_return_if_fail(GNOME_IS_DITEM_EDIT(dee));
        g_return_if_fail(ditem != NULL);

	if(dee->_priv->ditem)
		gnome_desktop_item_unref(dee->_priv->ditem);
	dee->_priv->ditem = ditem;

	gnome_desktop_item_ref(dee->_priv->ditem);

        gnome_ditem_edit_sync_display(dee, dee->_priv->ditem);
}

/**
 * gnome_ditem_edit_get_ditem
 * @dee: #GnomeDItemEdit object to work with
 *
 * Description: Get the current status of the editting areas
 * as a #GnomeDesktopItem structure.  It always gives a completely
 * newly allocated #GnomeDesktopItem structure.  If a copy of an
 * internal or previously set item is made it is always converted
 * to a gnome type of a desktop item.
 *
 * Returns: a newly allocated #GnomeDesktopItem structure.
 */
GnomeDesktopItem *
gnome_ditem_edit_get_ditem(GnomeDItemEdit *dee)
{
        GnomeDesktopItem * ditem;

        g_return_val_if_fail(dee != NULL, NULL);
        g_return_val_if_fail(GNOME_IS_DITEM_EDIT(dee), NULL);

	if(dee->_priv->ditem) {
		ditem = gnome_desktop_item_copy(dee->_priv->ditem);
		gnome_desktop_item_set_format(ditem, GNOME_DESKTOP_ITEM_GNOME);
	} else
		ditem = gnome_desktop_item_new();
	gnome_ditem_edit_sync_ditem(dee, ditem);
        return ditem;
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
gnome_ditem_edit_clear(GnomeDItemEdit *dee)
{
	GList *types = NULL;

        g_return_if_fail(dee != NULL);
        g_return_if_fail(GNOME_IS_DITEM_EDIT(dee));

	if(dee->_priv->ditem)
		gnome_desktop_item_unref(dee->_priv->ditem);
	dee->_priv->ditem = NULL;

        gtk_entry_set_text(GTK_ENTRY(dee->_priv->name_entry), "");
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->comment_entry),"");
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->exec_entry), "");  
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->tryexec_entry), "");
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->doc_entry), "");
        gnome_icon_entry_set_filename(GNOME_ICON_ENTRY(dee->_priv->icon_entry),"");
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->transl_lang_entry), "");
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->transl_name_entry), "");
        gtk_entry_set_text(GTK_ENTRY(dee->_priv->transl_comment_entry), "");
        gtk_clist_clear(GTK_CLIST(dee->_priv->translations));

	/* set everything to non-directory type which means any type */
	gnome_ditem_set_directory_sensitive(dee, FALSE);

	/* put all our possibilities here */
	types = g_list_prepend(types, "Application");
	types = g_list_prepend(types, "PanelApplet");
	types = g_list_prepend(types, "URL");
	types = g_list_prepend(types, "Directory");
	gtk_combo_set_popdown_strings(GTK_COMBO(dee->_priv->type_combo),
				      types);
	g_list_free(types);
}

static void
gnome_ditem_edit_changed(GnomeDItemEdit *dee)
{
        gtk_signal_emit(GTK_OBJECT(dee), ditem_edit_signals[CHANGED], NULL);
}

static void
gnome_ditem_edit_icon_changed(GnomeDItemEdit *dee)
{
        gtk_signal_emit(GTK_OBJECT(dee), ditem_edit_signals[ICON_CHANGED], NULL);
}

static void
gnome_ditem_edit_name_changed(GnomeDItemEdit *dee)
{
        gtk_signal_emit(GTK_OBJECT(dee), ditem_edit_signals[NAME_CHANGED], NULL);
}

/**
 * gnome_ditem_edit_get_icon
 * @dee: #GnomeDItemEdit object to work with
 *
 * Description: Get the icon filename. The icon is entered into a
 * #GnomeIconEntry, so the semantics of this call are the same as
 * for #gnome_icon_entry_get_filename
 *
 * Returns: a newly allocated string with the filename of the icon
 */
gchar *
gnome_ditem_edit_get_icon(GnomeDItemEdit *dee)
{
        g_return_val_if_fail(dee != NULL, NULL);
        g_return_val_if_fail(GNOME_IS_DITEM_EDIT(dee), NULL);
 
        return gnome_icon_entry_get_filename(GNOME_ICON_ENTRY(dee->_priv->icon_entry));
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
        gchar * name;

        g_return_val_if_fail(dee != NULL, NULL);
        g_return_val_if_fail(GNOME_IS_DITEM_EDIT(dee), NULL);

        name = gtk_entry_get_text(GTK_ENTRY(dee->_priv->name_entry));
        return g_strdup(name);
}

/**
 * gnome_ditem_edit_get_name_entry
 * @dee: #GnomeDItemEdit object to work with
 *
 * Description: Get the entry widget (a #GtkEntry) for the Name field.
 * Note that the name is only valid for this current language.
 *
 * Returns: a pointer to a #GtkEntry widget used for the Name field
 */
GtkWidget *
gnome_ditem_edit_get_name_entry (GnomeDItemEdit *dee)
{
        g_return_val_if_fail(dee != NULL, NULL);
        g_return_val_if_fail(GNOME_IS_DITEM_EDIT(dee), NULL);

        return dee->_priv->name_entry;
}

/**
 * gnome_ditem_edit_get_comment_entry
 * @dee: #GnomeDItemEdit object to work with
 *
 * Description: Get the entry widget (a #GtkEntry) for the Comment field.
 * Note that the comment is only valid for this current language.
 *
 * Returns: a pointer to a #GtkEntry widget used for the Comment field
 */
GtkWidget *
gnome_ditem_edit_get_comment_entry (GnomeDItemEdit *dee)
{
        g_return_val_if_fail(dee != NULL, NULL);
        g_return_val_if_fail(GNOME_IS_DITEM_EDIT(dee), NULL);

        return dee->_priv->comment_entry;
}

/**
 * gnome_ditem_edit_get_exec_entry
 * @dee: #GnomeDItemEdit object to work with
 *
 * Description: Get the entry widget (a #GtkEntry) for the Command
 * (exec) field.
 *
 * Returns: a pointer to a #GtkEntry widget used for the Command (exec) field
 */
GtkWidget *
gnome_ditem_edit_get_exec_entry (GnomeDItemEdit *dee)
{
        g_return_val_if_fail(dee != NULL, NULL);
        g_return_val_if_fail(GNOME_IS_DITEM_EDIT(dee), NULL);

        return dee->_priv->exec_entry;
}

/**
 * gnome_ditem_edit_get_tryexec_entry
 * @dee: #GnomeDItemEdit object to work with
 *
 * Description: Get the entry widget (a #GtkEntry) for the TryExec field.
 *
 * Returns: a pointer to a #GtkEntry widget used for the TryExec field
 */
GtkWidget *
gnome_ditem_edit_get_tryexec_entry (GnomeDItemEdit *dee)
{
        g_return_val_if_fail(dee != NULL, NULL);
        g_return_val_if_fail(GNOME_IS_DITEM_EDIT(dee), NULL);

        return dee->_priv->tryexec_entry;
}

/**
 * gnome_ditem_edit_get_doc_entry
 * @dee: #GnomeDItemEdit object to work with
 *
 * Description: Get the entry widget (a #GtkEntry) for the
 * Documentation field.
 *
 * Returns: a pointer to a #GtkEntry widget used for the
 * Documentation field
 */
GtkWidget *
gnome_ditem_edit_get_doc_entry (GnomeDItemEdit *dee)
{
        g_return_val_if_fail(dee != NULL, NULL);
        g_return_val_if_fail(GNOME_IS_DITEM_EDIT(dee), NULL);

        return dee->_priv->doc_entry;
}

/**
 * gnome_ditem_edit_get_icon_entry
 * @dee: #GnomeDItemEdit object to work with
 *
 * Description: Get the icon entry widget (a #GnomeIconEntry)
 * for the icon field.
 *
 * Returns: a pointer to a #GnomeIconEntry widget
 */
GtkWidget *
gnome_ditem_edit_get_icon_entry (GnomeDItemEdit *dee)
{
        g_return_val_if_fail(dee != NULL, NULL);
        g_return_val_if_fail(GNOME_IS_DITEM_EDIT(dee), NULL);

        return dee->_priv->icon_entry;
}

/**
 * gnome_ditem_edit_get_wmtitles_entry
 * @dee: #GnomeDItemEdit object to work with
 *
 * Description: Get the entry widget (a #GtkEntry) for
 * the comma separated list of titles to watch when launching
 * an application
 *
 * Returns: a pointer to a #GtkEntry widget
 */
GtkWidget *
gnome_ditem_edit_get_wmtitles_entry (GnomeDItemEdit *dee)
{
        g_return_val_if_fail(dee != NULL, NULL);
        g_return_val_if_fail(GNOME_IS_DITEM_EDIT(dee), NULL);

        return dee->_priv->wmtitles_entry;
}

/**
 * gnome_ditem_edit_get_simple_dnd_toggle
 * @dee: #GnomeDItemEdit object to work with
 *
 * Description: Get the check button widget (a #GtkCheckButton)
 * that sets the GNOME_DESKTOP_ITEM_OLD_STYLE_DROP flag for the
 * #GnomeDesktopItem.
 *
 * Returns: a pointer to a #GtkCheckButton
 */
GtkWidget *
gnome_ditem_edit_get_simple_dnd_toggle(GnomeDItemEdit *dee)
{
        g_return_val_if_fail(dee != NULL, NULL);
        g_return_val_if_fail(GNOME_IS_DITEM_EDIT(dee), NULL);

	return dee->_priv->simple_dnd_toggle;
}

/**
 * gnome_ditem_edit_get_file_drop_entry
 * @dee: #GnomeDItemEdit object to work with
 *
 * Description: Get the entry widget (a #GtkEntry) for the
 * FileDropExec field
 *
 * Returns: a pointer to a #GtkEntry widget
 */
GtkWidget *
gnome_ditem_edit_get_file_drop_entry(GnomeDItemEdit  *dee)
{
        g_return_val_if_fail(dee != NULL, NULL);
        g_return_val_if_fail(GNOME_IS_DITEM_EDIT(dee), NULL);

	return dee->_priv->file_drop_entry;
}

/**
 * gnome_ditem_edit_get_single_file_drop_toggle
 * @dee: #GnomeDItemEdit object to work with
 *
 * Description: Get the check button widget (a #GtkCheckButton) for the
 * SingleFileDropOnly field
 *
 * Returns: a pointer to a #GtkCheckButton widget
 */
GtkWidget *
gnome_ditem_edit_get_single_file_drop_toggle(GnomeDItemEdit *dee)
{
        g_return_val_if_fail(dee != NULL, NULL);
        g_return_val_if_fail(GNOME_IS_DITEM_EDIT(dee), NULL);

	return dee->_priv->single_file_drop_toggle;
}

/**
 * gnome_ditem_edit_get_url_drop_entry
 * @dee: #GnomeDItemEdit object to work with
 *
 * Description: Get the entry widget (a #GtkEntry) for the
 * URLDropExec field
 *
 * Returns: a pointer to a #GtkEntry widget
 */
GtkWidget *
gnome_ditem_edit_get_url_drop_entry(GnomeDItemEdit  *dee)
{
        g_return_val_if_fail(dee != NULL, NULL);
        g_return_val_if_fail(GNOME_IS_DITEM_EDIT(dee), NULL);

	return dee->_priv->url_drop_entry;
}

/**
 * gnome_ditem_edit_get_single_file_drop_toggle
 * @dee: #GnomeDItemEdit object to work with
 *
 * Description: Get the check button widget (a #GtkCheckButton) for the
 * SingleURLDropOnly field
 *
 * Returns: a pointer to a #GtkCheckButton widget
 */
GtkWidget *
gnome_ditem_edit_get_single_url_drop_toggle(GnomeDItemEdit *dee)
{
        g_return_val_if_fail(dee != NULL, NULL);
        g_return_val_if_fail(GNOME_IS_DITEM_EDIT(dee), NULL);

	return dee->_priv->single_url_drop_toggle;
}


#ifdef TEST_DITEM_EDIT

#include "libgnomeui.h"

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

        app = gnome_app_new("testing ditem edit", "Testing");

        notebook = gtk_notebook_new();

        gnome_app_set_contents(GNOME_APP(app), notebook);

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

