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

static void gnome_ditem_edit_class_init (GnomeDItemEditClass *klass);
static void gnome_ditem_edit_init       (GnomeDItemEdit      *messagebox);

static void gnome_ditem_edit_destroy (GtkObject *dee);

static void gnome_ditem_edit_sync_display(GnomeDItemEdit * dee,
                                          GnomeDesktopItem * ditem);

static void gnome_ditem_edit_sync_ditem(GnomeDItemEdit * dee,
                                        GnomeDesktopItem * ditem);

static void gnome_ditem_edit_changed(GnomeDItemEdit * dee);
static void gnome_ditem_edit_icon_changed(GnomeDItemEdit * dee);
static void gnome_ditem_edit_name_changed(GnomeDItemEdit * dee);

enum {
        CHANGED,
        ICON_CHANGED,
        NAME_CHANGED,
        LAST_SIGNAL
};

static gint ditem_edit_signals[LAST_SIGNAL] = { 0 };

static GtkObjectClass *parent_class;

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

        dee->name_entry = gtk_entry_new();
        table_attach_entry(GTK_TABLE(table),dee->name_entry, 1, 2, 0, 1);
        gtk_signal_connect_object_while_alive(GTK_OBJECT(dee->name_entry), "changed",
                                              GTK_SIGNAL_FUNC(gnome_ditem_edit_changed),
                                              GTK_OBJECT(dee));
        gtk_signal_connect_object_while_alive(GTK_OBJECT(dee->name_entry), "changed",
                                              GTK_SIGNAL_FUNC(gnome_ditem_edit_name_changed),
                                              GTK_OBJECT(dee));


        label = label_new(_("Comment:"));
        table_attach_label(GTK_TABLE(table),label, 0, 1, 1, 2);

        dee->comment_entry = gtk_entry_new();
        table_attach_entry(GTK_TABLE(table),dee->comment_entry, 1, 2, 1, 2);
        gtk_signal_connect_object_while_alive(GTK_OBJECT(dee->comment_entry), "changed",
                                              GTK_SIGNAL_FUNC(gnome_ditem_edit_changed),
                                              GTK_OBJECT(dee));


        label = label_new(_("Command:"));
        table_attach_label(GTK_TABLE(table),label, 0, 1, 2, 3);

        dee->exec_entry = gtk_entry_new();
        table_attach_entry(GTK_TABLE(table),dee->exec_entry, 1, 2, 2, 3);
        gtk_signal_connect_object_while_alive(GTK_OBJECT(dee->exec_entry), "changed",
                                              GTK_SIGNAL_FUNC(gnome_ditem_edit_changed),
                                              GTK_OBJECT(dee));


        label = label_new(_("Type:"));
        table_attach_label(GTK_TABLE(table), label, 0, 1, 3, 4);

        types = g_list_prepend(types, "Application");
        types = g_list_prepend(types, "Directory");
        dee->type_combo = gtk_combo_new();
        gtk_combo_set_popdown_strings(GTK_COMBO(dee->type_combo), types);
        g_list_free(types);
        gtk_combo_set_value_in_list(GTK_COMBO(dee->type_combo), 
                                    FALSE, TRUE);
        table_attach_entry(GTK_TABLE(table),dee->type_combo, 1, 2, 3, 4);
        gtk_signal_connect_object_while_alive(GTK_OBJECT(GTK_COMBO(dee->type_combo)->entry), 
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

        dee->icon_entry = gnome_icon_entry_new("icon",_("Choose an icon"));
        e = gnome_icon_entry_gtk_entry(GNOME_ICON_ENTRY(dee->icon_entry));
        gtk_signal_connect_object_while_alive(GTK_OBJECT(e),"changed",
                                              GTK_SIGNAL_FUNC(gnome_ditem_edit_changed),
                                              GTK_OBJECT(dee));
        gtk_signal_connect_object_while_alive(GTK_OBJECT(e),"changed",
                                              GTK_SIGNAL_FUNC(gnome_ditem_edit_icon_changed),
                                              GTK_OBJECT(dee));
        gtk_box_pack_start(GTK_BOX(hbox), dee->icon_entry, FALSE, FALSE, 0);

        align = gtk_alignment_new(0.0, 0.5, 0.0, 0.0);
        gtk_box_pack_start(GTK_BOX(hbox), align, FALSE, FALSE, 0);

        dee->terminal_button = gtk_check_button_new_with_label (_("Run in Terminal"));
        gtk_signal_connect_object(GTK_OBJECT(dee->terminal_button), 
                                  "clicked",
                                  GTK_SIGNAL_FUNC(gnome_ditem_edit_changed),
                                  GTK_OBJECT(dee));
        gtk_container_add(GTK_CONTAINER(align), dee->terminal_button);
}

static void
set_list_width(GtkWidget *clist, char *text[])
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
	
        gtk_entry_set_text(GTK_ENTRY(dee->transl_lang_entry), lang);
        gtk_entry_set_text(GTK_ENTRY(dee->transl_name_entry), name);
        gtk_entry_set_text(GTK_ENTRY(dee->transl_comment_entry), comment);
}

static void
translations_add(GtkWidget *button, GnomeDItemEdit *dee)
{
        int i;
        char *lang;
        char *name;
        char *comment;
        char *text[3];
        GList *language_list;
        const char *curlang;
        GtkCList *cl = GTK_CLIST(dee->translations);

        lang = gtk_entry_get_text(GTK_ENTRY(dee->transl_lang_entry));
        name = gtk_entry_get_text(GTK_ENTRY(dee->transl_name_entry));
        comment = gtk_entry_get_text(GTK_ENTRY(dee->transl_comment_entry));
  
        g_assert(lang && name && comment);
	
        lang = g_strstrip(g_strdup(lang));
	
        /*we are setting the current language so set the easy page entries*/
        /*FIXME: do the opposite as well!, but that's not that crucial*/
        language_list = gnome_i18n_get_language_list("LC_ALL");
        curlang = language_list ? language_list->data : NULL;
        if ((curlang && strcmp(curlang,lang)==0) ||
            ((!curlang || strcmp(curlang,"C")==0) && !*lang)) {
                gtk_entry_set_text(GTK_ENTRY(dee->name_entry),name);
                gtk_entry_set_text(GTK_ENTRY(dee->comment_entry),comment);
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
        gtk_clist_append(cl,text);
        gtk_signal_emit(GTK_OBJECT(dee), ditem_edit_signals[CHANGED], NULL);
  
        g_free(lang);
}

static void
translations_remove(GtkWidget *button, GnomeDItemEdit *dee)
{
        GtkCList *cl = GTK_CLIST(dee->translations);
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
        char *transl[3];

        label = label_new(_("Try this before using:"));
        table_attach_label(GTK_TABLE(page),label, 0, 1, 0, 1);

        dee->tryexec_entry = gtk_entry_new();
        table_attach_entry(GTK_TABLE(page),dee->tryexec_entry, 1, 2, 0, 1);
        gtk_signal_connect_object(GTK_OBJECT(dee->tryexec_entry), 
                                  "changed",
                                  GTK_SIGNAL_FUNC(gnome_ditem_edit_changed),
                                  GTK_OBJECT(dee));

        label = label_new(_("Documentation:"));
        table_attach_label(GTK_TABLE(page),label, 0, 1, 1, 2);

        dee->doc_entry = gtk_entry_new_with_max_length(255);
        table_attach_entry(GTK_TABLE(page),dee->doc_entry, 1, 2, 1, 2);
        gtk_signal_connect_object(GTK_OBJECT(dee->doc_entry), 
                                  "changed",
                                  GTK_SIGNAL_FUNC(gnome_ditem_edit_changed),
                                  GTK_OBJECT(dee));

        label = gtk_label_new(_("Name/Comment translations:"));
        table_attach_label(GTK_TABLE(page),label, 0, 2, 2, 3);
  
        transl[0] = _("Language");
        transl[1] = _("Name");
        transl[2] = _("Comment");
        dee->translations = gtk_clist_new_with_titles(3,transl);
        set_list_width(dee->translations,transl);
        box = gtk_scrolled_window_new(NULL,NULL);
        gtk_widget_set_usize(box,0,120);
        gtk_container_add(GTK_CONTAINER(box),dee->translations);
        table_attach_list(GTK_TABLE(page),box, 0, 2, 3, 4);
        gtk_clist_column_titles_passive(GTK_CLIST(dee->translations));
        gtk_signal_connect(GTK_OBJECT(dee->translations),"select_row",
                           GTK_SIGNAL_FUNC(translations_select_row),
                           dee);

        box = gtk_hbox_new(FALSE,GNOME_PAD_SMALL);
        table_attach_entry(GTK_TABLE(page),box, 0, 2, 4, 5);
  
        dee->transl_lang_entry = gtk_entry_new();
        gtk_box_pack_start(GTK_BOX(box),dee->transl_lang_entry,FALSE,FALSE,0);
        gtk_widget_set_usize(dee->transl_lang_entry,50,0);

        dee->transl_name_entry = gtk_entry_new();
        gtk_box_pack_start(GTK_BOX(box),dee->transl_name_entry,TRUE,TRUE,0);

        dee->transl_comment_entry = gtk_entry_new();
        gtk_box_pack_start(GTK_BOX(box),dee->transl_comment_entry,TRUE,TRUE,0);

        box = gtk_hbox_new(FALSE,GNOME_PAD_SMALL);
        table_attach_entry(GTK_TABLE(page),box, 0, 2, 5, 6);
  
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
        dee->child1       = NULL;
        dee->child2       = NULL;
}


/**
 * gnome_ditem_edit_new
 *
 * Description: Creates a new #GnomeDItemEdit object. The object is not
 * a widget, but just an object which creates some widgets which you have
 * to add to a notebook. Use the #gnome_ditem_edit_new_notebook to add
 * pages to the notebook.
 *
 * Returns: Newly-created #GnomeDItemEdit object.
 */
GtkObject *
gnome_ditem_edit_new (void)
{
        GnomeDItemEdit * dee;

        dee = gtk_type_new(gnome_ditem_edit_get_type());

        dee->child1 = make_page();
        fill_easy_page(dee, GTK_BIN(dee->child1)->child);
        gtk_widget_show_all(dee->child1);

        dee->child2 = make_page();
        fill_advanced_page(dee, GTK_BIN(dee->child2)->child);
        gtk_widget_show_all(dee->child2);

        return GTK_OBJECT (dee);
}


/**
 * gnome_ditem_edit_new_notebook
 * @notebook: notebook to add the pages to
 *
 * Description: Creates a new #GnomeDItemEdit object and adds it's pages
 * to the @notebook specified in the parameter.
 *
 * Returns: Newly-created #GnomeDItemEdit object.
 */
GtkObject *
gnome_ditem_edit_new_notebook (GtkNotebook *notebook)
{
        GnomeDItemEdit * dee;

        g_return_val_if_fail(notebook != NULL, NULL);
  
        dee = GNOME_DITEM_EDIT(gnome_ditem_edit_new());

        gtk_notebook_append_page (GTK_NOTEBOOK(notebook), 
                                  dee->child1, 
                                  gtk_label_new(_("Basic")));

        gtk_notebook_append_page (GTK_NOTEBOOK(notebook), 
                                  dee->child2, 
                                  gtk_label_new(_("Advanced")));

        /* Destroy self with the notebook. */
        gtk_signal_connect_object(GTK_OBJECT(notebook), "destroy",
                                  GTK_SIGNAL_FUNC(gtk_object_destroy),
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

        gtk_widget_destroy(de->icon_entry);

        if (GTK_OBJECT_CLASS(parent_class)->destroy)
                (* (GTK_OBJECT_CLASS(parent_class)->destroy))(dee);
}

/* Conform display to ditem */
static void
gnome_ditem_edit_sync_display(GnomeDItemEdit *dee,
                              GnomeDesktopItem *ditem)
{
        gchar * s = NULL;
        GList *i18n_list,*li;
        const gchar* cs;
        const gchar** csv;
        
        g_return_if_fail(dee != NULL);
        g_return_if_fail(GNOME_IS_DITEM_EDIT(dee));
  

        cs = gnome_desktop_item_get_local_name(ditem);
        
        gtk_entry_set_text(GTK_ENTRY(dee->name_entry), 
                           cs ? cs : "");

        cs = gnome_desktop_item_get_local_comment(ditem);
        
        gtk_entry_set_text(GTK_ENTRY(dee->comment_entry),
                           cs ? cs : "");

        csv = gnome_desktop_item_get_command(ditem);

        if (csv)
                s = g_strjoinv(" ", csv);
        gtk_entry_set_text(GTK_ENTRY(dee->exec_entry), s ? s : "");
        g_free(s);

        
        gtk_entry_set_text(GTK_ENTRY(dee->tryexec_entry), 
                           ditem->tryexec ? ditem->tryexec : "");

        
        
        gnome_icon_entry_set_icon(GNOME_ICON_ENTRY(dee->icon_entry),
                                  ditem->icon ? ditem->icon : "");

        gtk_entry_set_text(GTK_ENTRY(dee->doc_entry), 
                           ditem->docpath ? ditem->docpath : "");

        gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(dee->type_combo)->entry),
                           ditem->type ? ditem->type : "");

        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dee->terminal_button),
                                     ditem->terminal);
  
        /*set the names and comments from our i18n list*/
        gtk_clist_clear (GTK_CLIST(dee->translations));
        i18n_list = gnome_desktop_entry_get_i18n_list (ditem);
        for (li=i18n_list; li; li=li->next) {
                char *text[3];
                GnomeDesktopItemI18N *e = li->data;
                text[0] = e->lang?e->lang:"";
                text[1] = e->name?e->name:"";
                text[2] = e->comment?e->comment:"";
                set_list_width (dee->translations,text);
                gtk_clist_append (GTK_CLIST(dee->translations),text);
        }
}

/* Conform ditem to display */
static void
gnome_ditem_edit_sync_ditem(GnomeDItemEdit *dee,
                            GnomeDesktopItem *ditem)
{
        GList *i18n_list = NULL;
        gchar * text;
        int i;

        g_return_if_fail(dee != NULL);
        g_return_if_fail(GNOME_IS_DITEM_EDIT(dee));
        g_return_if_fail(ditem != NULL);

        text = gtk_entry_get_text(GTK_ENTRY(dee->name_entry));
        g_free(ditem->name);
        if (text[0] != '\0')
                ditem->name = g_strdup(text);
        else
                ditem->name = NULL;

        text = gtk_entry_get_text(GTK_ENTRY(dee->comment_entry));
        g_free(ditem->comment);
        if (text[0] != '\0')
                ditem->comment = g_strdup(text);
        else
                ditem->comment = NULL;

        text = gtk_entry_get_text(GTK_ENTRY(dee->exec_entry));
        g_strfreev(ditem->exec);
        if (text[0] != '\0')
                gnome_config_make_vector(text, &ditem->exec_length, &ditem->exec);
        else {
                ditem->exec_length = 0;
                ditem->exec = NULL;
        }

        text = gtk_entry_get_text(GTK_ENTRY(dee->tryexec_entry));
        g_free(ditem->tryexec);
        if (text[0] != '\0')
                ditem->tryexec = g_strdup(text);
        else
                ditem->tryexec = NULL;
  
        text = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(dee->type_combo)->entry));
        g_free(ditem->type);
        if (text[0] != '\0')
                ditem->type = g_strdup(text);
        else
                ditem->type = NULL;
  
        g_free(ditem->icon);
        ditem->icon = gnome_icon_entry_get_filename(GNOME_ICON_ENTRY(dee->icon_entry));

        text = gtk_entry_get_text(GTK_ENTRY(dee->doc_entry));
        g_free(ditem->docpath);
        if (text[0] != '\0')
                ditem->docpath = g_strdup(text);
        else
                ditem->docpath = NULL;

        ditem->terminal = GTK_TOGGLE_BUTTON(dee->terminal_button)->active;

        g_free(ditem->location);
        ditem->location = NULL;
        g_free(ditem->geometry);
        ditem->geometry = NULL;

        for(i=0;i<GTK_CLIST(dee->translations)->rows;i++) {
                char *lang,*name,*comment;
                GnomeDesktopItemI18N *i18n_entry;
                gtk_clist_get_text(GTK_CLIST(dee->translations),i,0,&lang);
                gtk_clist_get_text(GTK_CLIST(dee->translations),i,1,&name);
                gtk_clist_get_text(GTK_CLIST(dee->translations),i,2,&comment);
                if(!*lang) lang = NULL;
                if(!*name) name = NULL;
                if(!*comment) comment = NULL;
                if(!name && !comment)
                        continue;

                i18n_entry = g_new0(GnomeDesktopItemI18N,1);
                i18n_entry->lang = lang?g_strdup(lang):NULL;
                i18n_entry->name = name?g_strdup(name):NULL;
                i18n_entry->comment = comment?g_strdup(comment):NULL;
                i18n_list = g_list_prepend(i18n_list,i18n_entry);
        }
        gnome_desktop_entry_free_i18n_list(gnome_desktop_entry_get_i18n_list(ditem));
        gnome_desktop_entry_set_i18n_list(ditem,i18n_list);
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

        newentry = gnome_desktop_entry_load_unconditional(path);

        if (newentry) {
                gnome_ditem_edit_sync_display(dee, newentry);
                gnome_desktop_entry_destroy(newentry);
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
 * Description: Destroy existing ditem and replace
 * it with this one, updating the #GnomeDItemEdit to reflect it.
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

        gnome_ditem_edit_sync_display(dee, ditem);
}

/*XXX: whoops!!!!, this call is left here just for binary
  compatibility, it should be gnome_ditem_edit_get_ditem*/
/**
 * gnome_ditem_get_ditem
 * @dee: #GnomeDItemEdit object to work with
 *
 * Description: This call is actually the #gnome_ditem_edit_get_ditem,
 * it should not be used, it is left ONLY for compatibility reasons.
 *
 * Returns: a newly allocated #GnomeDesktopItem structure.
 */
GnomeDesktopItem *
gnome_ditem_get_ditem(GnomeDItemEdit *dee)
{
        return gnome_ditem_edit_get_ditem(dee);
}
/**
 * gnome_ditem_edit_get_ditem
 * @dee: #GnomeDItemEdit object to work with
 *
 * Description: Get the current status of the editting areas
 * as a #GnomeDesktopItem structure.
 *
 * Returns: a newly allocated #GnomeDesktopItem structure.
 */
GnomeDesktopItem *
gnome_ditem_edit_get_ditem(GnomeDItemEdit *dee)
{
        GnomeDesktopItem * newentry;

        g_return_val_if_fail(dee != NULL, NULL);
        g_return_val_if_fail(GNOME_IS_DITEM_EDIT(dee), NULL);

        newentry = g_new0(GnomeDesktopItem, 1);
        gnome_ditem_edit_sync_ditem(dee, newentry);
        return newentry;
}

/**
 * gnome_ditem_edit_clear
 * @dee: #GnomeDItemEdit object to work with
 *
 * Description: Clear the editting areas.
 *
 * Returns:
 */
void
gnome_ditem_edit_clear(GnomeDItemEdit *dee)
{
        g_return_if_fail(dee != NULL);
        g_return_if_fail(GNOME_IS_DITEM_EDIT(dee));

        gtk_entry_set_text(GTK_ENTRY(dee->name_entry), "");
        gtk_entry_set_text(GTK_ENTRY(dee->comment_entry),"");
        gtk_entry_set_text(GTK_ENTRY(dee->exec_entry), "");  
        gtk_entry_set_text(GTK_ENTRY(dee->tryexec_entry), "");
        gtk_entry_set_text(GTK_ENTRY(dee->doc_entry), "");
        gnome_icon_entry_set_icon(GNOME_ICON_ENTRY(dee->icon_entry),"");
        gtk_entry_set_text(GTK_ENTRY(dee->transl_lang_entry), "");
        gtk_entry_set_text(GTK_ENTRY(dee->transl_name_entry), "");
        gtk_entry_set_text(GTK_ENTRY(dee->transl_comment_entry), "");
        gtk_clist_clear(GTK_CLIST(dee->translations));
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
 
        return gnome_icon_entry_get_filename(GNOME_ICON_ENTRY(dee->icon_entry));
}

/**
 * gnome_ditem_edit_get_name
 * @dee: #GnomeDItemEdit object to work with
 *
 * Description: Get the Name field from the ditem.
 *
 * Returns: a newly allocated string with the name of the ditem
 */
gchar *
gnome_ditem_edit_get_name (GnomeDItemEdit *dee)
{
        gchar * name;

        name = gtk_entry_get_text(GTK_ENTRY(dee->name_entry));
        return g_strdup(name);
}

/**
 * gnome_ditem_edit_get_name_entry
 * @dee: #GnomeDItemEdit object to work with
 *
 * Description: Get the entry widget (a #GtkEntry) for the Name field.
 *
 * Returns: a pointer to a #GtkEntry widget used for the Name field
 */
GtkWidget *
gnome_ditem_get_name_entry (GnomeDItemEdit *dee)
{
        return dee->name_entry;
}

/**
 * gnome_ditem_edit_get_comment_entry
 * @dee: #GnomeDItemEdit object to work with
 *
 * Description: Get the entry widget (a #GtkEntry) for the Comment field.
 *
 * Returns: a pointer to a #GtkEntry widget used for the Comment field
 */
GtkWidget *
gnome_ditem_get_comment_entry (GnomeDItemEdit *dee)
{
        return dee->comment_entry;
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
gnome_ditem_get_exec_entry (GnomeDItemEdit *dee)
{
        return dee->exec_entry;
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
gnome_ditem_get_tryexec_entry (GnomeDItemEdit *dee)
{
        return dee->tryexec_entry;
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
gnome_ditem_get_doc_entry (GnomeDItemEdit *dee)
{
        return dee->doc_entry;
}

/**
 * gnome_ditem_edit_get_icon_entry
 * @dee: #GnomeDItemEdit object to work with
 *
 * Description: Get the entry widget (a #GnomeIconEntry) for the icon field
 *
 * Returns: a pointer to a #GnomeIconEntry widget used for the icon field
 */
GtkWidget *
gnome_ditem_get_icon_entry (GnomeDItemEdit *dee)
{
        return dee->icon_entry;
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

