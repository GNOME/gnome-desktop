#include <config.h>
#include <gnome.h>
#include "gnome-hint.h"
#include <gconf/gconf-client.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#define GH_TEXT_FONT "sans bold 10"
#define GH_TITLE_FONT "sans bold 20"

#define MINIMUM_WIDTH 380 
#define MINIMUM_HEIGHT 180

#define GH_TITLE_POS_X 10.0
#define GH_TITLE_POS_Y 10.0

#define GH_TEXTVIEW_X 40.0
#define GH_TEXTVIEW_Y 60.0
#define GH_TEXTVIEW_WIDTH 280.0
#define GH_TEXTVIEW_HEIGHT 70.0

static int hintnum = 0;

struct _GnomeHintPrivate {
	GConfClient* client;
	gchar *startupkey;

	GList *hintlist;
	GList *curhint;
	
	GtkWidget *canvas;
	GtkWidget *checkbutton;

	GnomeCanvasItem *background_image;
	GnomeCanvasItem *logo_image;

	GnomeCanvasItem *title_text;
	GnomeCanvasItem *hint_text;
};

static void gnome_hint_class_init (GnomeHintClass *klass); 
static void gnome_hint_object_init (GnomeHint *gnome_hint,
		 GnomeHintClass *klass);
static void gnome_hint_instance_init (GnomeHint *gnome_hint);
static void gnome_hint_set_property (GObject *object, guint prop_id,
		 const GValue *value, GParamSpec *pspec);
static void gnome_hint_get_property (GObject *object, guint prop_id,
		 GValue *value, GParamSpec *pspec);
static void gnome_hint_repaint (GtkWidget *widget, GdkDrawable *drawable,
		 GnomeHint *gnome_hint);
static void gnome_hint_notify (GObject *object, GParamSpec *pspec);
static void gnome_hint_finalize (GObject *object);
static void dialog_button_clicked(GtkWindow *window, int button, gpointer data);static void checkbutton_clicked(GtkWidget *w, gpointer data);

GNOME_CLASS_BOILERPLATE (GnomeHint, gnome_hint, GnomeDialog,
			 gnome_dialog, GNOME_TYPE_DIALOG)

static void
gnome_hint_class_init (GnomeHintClass *klass) {

  GObjectClass *object_class = G_OBJECT_CLASS (klass);  
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);  

  object_class->set_property = gnome_hint_set_property;
  object_class->get_property = gnome_hint_get_property; 
  object_class->notify = gnome_hint_notify;
  object_class->finalize = gnome_hint_finalize;
}

static void
gnome_hint_instance_init (GnomeHint *gnome_hint) {

  GtkWidget *sw;
	
  gnome_hint->_priv = g_new (GnomeHintPrivate, 1);

  sw = sw = gtk_scrolled_window_new(NULL,NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
                                 GTK_POLICY_NEVER,
                                 GTK_POLICY_NEVER);
  gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(gnome_hint)->vbox),sw,TRUE,TRUE,0);

  gnome_hint->_priv->hintlist = NULL;
  gnome_hint->_priv->curhint = NULL;

  gnome_hint->_priv->client = gconf_client_get_default();

  gnome_hint->_priv->canvas = gnome_canvas_new();
  gtk_container_add(GTK_CONTAINER(sw),gnome_hint->_priv->canvas);
  gnome_canvas_set_scroll_region(GNOME_CANVAS(gnome_hint->_priv->canvas),
                                 0.0,0.0,MINIMUM_WIDTH,MINIMUM_HEIGHT);
  gtk_widget_set_usize(gnome_hint->_priv->canvas,MINIMUM_WIDTH,MINIMUM_HEIGHT);

  gnome_hint->_priv->checkbutton = 
	gtk_check_button_new_with_mnemonic(_("_Show Hints at Startup"));
	
  gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(gnome_hint)->vbox),
	gnome_hint->_priv->checkbutton,TRUE,TRUE,0);

  gtk_signal_connect(GTK_OBJECT(gnome_hint->_priv->checkbutton), "clicked",
		     GTK_SIGNAL_FUNC(checkbutton_clicked), gnome_hint);

  gnome_dialog_append_button (GNOME_DIALOG(gnome_hint),GNOME_STOCK_BUTTON_PREV);
  gnome_dialog_append_button (GNOME_DIALOG(gnome_hint),GNOME_STOCK_BUTTON_NEXT);
  gnome_dialog_append_button (GNOME_DIALOG(gnome_hint),GNOME_STOCK_BUTTON_OK);

  gtk_signal_connect(GTK_OBJECT(gnome_hint), "clicked", 
		     GTK_SIGNAL_FUNC(dialog_button_clicked), gnome_hint);
}

static void gnome_hint_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
  /* Does nothing for the moment */
}
static void gnome_hint_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
  /* Does nothing for the moment */
}

static void 
gnome_hint_notify (GObject *object, GParamSpec *pspec) {
  /* Does nothing for the moment */
}

static void 
gnome_hint_finalize (GObject *object) {
  GList *node;
  GnomeHint *gnome_hint = GNOME_HINT (object);

  node = gnome_hint->_priv->hintlist;

  while (node){
	g_free(node->data);
	node=node->next;
  }

  g_free(gnome_hint->_priv->startupkey);

  g_free (gnome_hint->_priv);
  gnome_hint->_priv = NULL;

  GNOME_CALL_PARENT_HANDLER (G_OBJECT_CLASS, finalize, (object));
}

static void
checkbutton_clicked(GtkWidget *w, gpointer data)
{
	GnomeHint *gh = GNOME_HINT(data);
	
	gconf_client_set_bool(gh->_priv->client, gh->_priv->startupkey,
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gh->_priv->checkbutton)),NULL);
	
}

static void
dialog_button_clicked(GtkWindow *window, int button, gpointer data)
{
	GnomeHint *gh = GNOME_HINT(data);

        switch(button) {
        case 0:
		if (gh->_priv->curhint == NULL)
                        return;
                gh->_priv->curhint = gh->_priv->curhint->prev;
                if (gh->_priv->curhint == NULL)
                        gh->_priv->curhint = g_list_last (gh->_priv->hintlist);
                gnome_canvas_item_set(gh->_priv->hint_text,
                        "text",(char *)gh->_priv->curhint->data,
                        NULL);
                break;
        case 1:
		if (gh->_priv->curhint == NULL)
                        return;
                gh->_priv->curhint = gh->_priv->curhint->next;
                if (gh->_priv->curhint == NULL)
                        gh->_priv->curhint = gh->_priv->hintlist;
                gnome_canvas_item_set(gh->_priv->hint_text,
                     "text",(char *)gh->_priv->curhint->data,
                     NULL);
                break;
        default:
		gtk_widget_destroy(GTK_WIDGET(window));
		break;
        }
}

/*find the language, but only look as far as gotlang*/
static const GList *
find_lang (const GList *langlist, const GList *gotlang, const char *lang)
{
        while (langlist != NULL &&
               langlist != gotlang) {
                if (strcmp (langlist->data, lang) == 0)
                        return langlist;
                langlist = langlist->next;
        }
        return NULL;
}

/*parse all children and pick out the best language one*/
static char *
get_i18n_string (xmlDocPtr doc, xmlNodePtr child, const char *name)
{
        const GList *langlist;
        char *current;
        xmlNodePtr cur;
        const GList *gotlang = NULL; /*the highest language we got*/

        langlist = gnome_i18n_get_language_list("LC_ALL");

        current = NULL;

        /*find C the locale string*/
        for(cur = child->xmlChildrenNode; cur; cur = cur->next) {
                char *lang;
                if (cur->name == NULL ||
                    g_strcasecmp (cur->name, name) != 0)
                        continue;

                lang = xmlGetProp (cur, "xml:lang");
                if (lang == NULL ||
                    lang[0] == '\0') {
                        if (lang != NULL)
                                xmlFree (lang);
                        if (gotlang != NULL)
                                continue;
                        if (current != NULL)
                                xmlFree (current);
                        current = xmlNodeListGetString (doc, cur->xmlChildrenNode, 1);
                } else {
                        const GList *ll = find_lang (langlist, gotlang, lang);
                        xmlFree (lang);
                        if (ll != NULL) {
                                if (current != NULL)
                                        xmlFree (current);

                                current = xmlNodeListGetString (doc, cur->xmlChildrenNode, 1);
                                gotlang = ll;
                                if (ll == langlist) /*we can't get any better then this*/
                                        break;
                        }
                }
        }
        return current;
}

static GList*
read_hints_from_file (const char *file, GList *hintlist)
{
        xmlDocPtr doc;
        xmlNodePtr cur,root;
        doc = xmlParseFile(file);
	root = xmlDocGetRootElement(doc);
        if (doc == NULL)
                return;

        if (root == NULL)
	{
                xmlFreeDoc (doc);
                return;
        }

        for (cur = root->xmlChildrenNode; cur; cur = cur->next) {
                char *str;
                if (cur->name == NULL ||
                    g_strcasecmp (cur->name, "Hint") != 0)
                        continue;
                str = get_i18n_string (doc, cur, "Content");
                if (str != NULL) {
                        hintlist = g_list_prepend (hintlist, str);
                        hintnum++;
                }
        }

        xmlFreeDoc (doc);
	return hintlist;
}

static void
pick_random_hint(GnomeHint *gh)
{
        srandom(time(NULL));
        gh->_priv->curhint = g_list_nth (gh->_priv->hintlist, random()%hintnum);
}

GtkWidget *
gnome_hint_new (const gchar *hintfile,
		const gchar *title,
		const gchar *background_image,
		const gchar *logo_image,
		const gchar *startupkey) 
{
  GnomeHint *gnome_hint;
  GdkPixbuf *im;

  if (!g_file_exists(hintfile)) return NULL;

  gnome_hint = g_object_new (GNOME_TYPE_HINT, NULL);

  gnome_hint->_priv->startupkey = g_strdup(startupkey);

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gnome_hint->_priv->checkbutton)
	,gconf_client_get_bool(gnome_hint->_priv->client,startupkey,NULL));

  /*Create a canvas item for the background and logo images
    and the title*/
  if (background_image){
  	im = gdk_pixbuf_new_from_file(background_image, NULL);
  	if (im){
  		gnome_hint->_priv->background_image = gnome_canvas_item_new(
		  gnome_canvas_root(GNOME_CANVAS(gnome_hint->_priv->canvas)),
        	  gnome_canvas_pixbuf_get_type (),
		  "pixbuf", im, "x", 0.0, "y", 0.0,
                  "width", (double) gdk_pixbuf_get_width(im),
                  "height", (double) gdk_pixbuf_get_height(im),
                  NULL);
       	gdk_pixbuf_unref(im);
  	}
  }
  if (logo_image){
  	im = gdk_pixbuf_new_from_file(logo_image, NULL);
  	if (im){
        	gnome_hint->_priv->background_image = gnome_canvas_item_new(
                  gnome_canvas_root(GNOME_CANVAS(gnome_hint->_priv->canvas)),
                  gnome_canvas_pixbuf_get_type (),
                  "pixbuf", im, "x", (double)(MINIMUM_WIDTH -75), "y", 0.0,
                  "width", (double) gdk_pixbuf_get_width(im),
                  "height", (double) gdk_pixbuf_get_height(im),
                  NULL);
       	gdk_pixbuf_unref(im);
  	}
  }
  if (!title) title="Gnome Hints";
  gnome_hint->_priv->title_text = gnome_canvas_item_new(
        gnome_canvas_root(GNOME_CANVAS(gnome_hint->_priv->canvas)),
        gnome_canvas_text_get_type (),
        "x", GH_TITLE_POS_X, "y", GH_TITLE_POS_Y,
        "font", GH_TITLE_FONT,
        "anchor", GTK_ANCHOR_NW,
        "fill_color", "white",
        "text", title,
        NULL);

  gtk_window_set_title (GTK_WINDOW (gnome_hint),title);

  /* Now load up the hints into gnome_hint->_priv->hintlist*/

  gnome_hint->_priv->hintlist = read_hints_from_file(hintfile,
        gnome_hint->_priv->hintlist);
  pick_random_hint(gnome_hint);

  gnome_hint->_priv->hint_text = gnome_canvas_item_new(
	gnome_canvas_root(GNOME_CANVAS(gnome_hint->_priv->canvas)),
        gnome_canvas_rich_text_get_type (),
	"cursor_visible", FALSE,
	"text", (char *)gnome_hint->_priv->curhint->data,
	"x", GH_TEXTVIEW_X, "y", GH_TEXTVIEW_Y,
	"width",GH_TEXTVIEW_WIDTH, "height", GH_TEXTVIEW_HEIGHT,
	"anchor", GTK_ANCHOR_NW,
	NULL);

  return GTK_WIDGET (gnome_hint);
}
