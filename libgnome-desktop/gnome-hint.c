#include <config.h>
#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>
#include <libgnomeui/gnome-hint.h>
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

#define GNOME_HINT_RESPONSE_NEXT 1
#define GNOME_HINT_RESPONSE_PREV 2

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

GNOME_CLASS_BOILERPLATE (GnomeHint, gnome_hint, GtkDialog, GTK_TYPE_DIALOG)

static void gnome_hint_set_accessible_information (GnomeHint *gh,
						   const gchar *name);

static void
dialog_response (GnomeHint *gnome_hint,
		 int        response,
		 gpointer   dummy)
{
	GnomeHintPrivate *priv;

	g_return_if_fail (GNOME_IS_HINT (gnome_hint));

	priv = gnome_hint->_priv;

        switch (response) {
        case GNOME_HINT_RESPONSE_PREV:
		if (!priv->curhint)
                        return;

                priv->curhint =	priv->curhint->prev;

                if (!priv->curhint)
                        priv->curhint = g_list_last (priv->hintlist);

                gnome_canvas_item_set (priv->hint_text,
				       "text", (char *) priv->curhint->data,
				       NULL);
		gnome_hint_set_accessible_information (gnome_hint, NULL);
                break;
        case GNOME_HINT_RESPONSE_NEXT:
		if (!priv->curhint)
                        return;

                priv->curhint = priv->curhint->next;

                if (!priv->curhint)
                        priv->curhint = priv->hintlist;

                gnome_canvas_item_set (priv->hint_text,
				       "text", (char *) priv->curhint->data,
				       NULL);
		gnome_hint_set_accessible_information (gnome_hint, NULL);
                break;
        default:
		gtk_widget_destroy (GTK_WIDGET (gnome_hint));
		break;
        }
}

static void
checkbutton_clicked (GtkWidget *w,
		     GnomeHint *gnome_hint)
{
	gconf_client_set_bool (
		gnome_hint->_priv->client,
		gnome_hint->_priv->startupkey,
		gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (gnome_hint->_priv->checkbutton)),
		NULL);
}

static void 
gnome_hint_finalize (GObject *object)
{
	GnomeHint *gnome_hint;
	GList     *l;

	g_return_if_fail (GNOME_IS_HINT (object));

	gnome_hint = GNOME_HINT (object);

	for (l = gnome_hint->_priv->hintlist; l; l = l->next)
		g_free (l->data);
	g_list_free (gnome_hint->_priv->hintlist);

	g_free (gnome_hint->_priv->startupkey);

	g_free  (gnome_hint->_priv);
	gnome_hint->_priv = NULL;

	GNOME_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
gnome_hint_class_init (GnomeHintClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);  

	object_class->finalize = gnome_hint_finalize;
}

static void
gnome_hint_instance_init (GnomeHint *gnome_hint) {

	GtkWidget *sw;

	gnome_hint->_priv = g_new (GnomeHintPrivate, 1);

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_NEVER,
					GTK_POLICY_NEVER);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (gnome_hint)->vbox),
			    sw, TRUE, TRUE, 0);

	gnome_hint->_priv->client = gconf_client_get_default();

	gnome_hint->_priv->canvas = gnome_canvas_new ();
	gnome_canvas_set_scroll_region (
		GNOME_CANVAS (gnome_hint->_priv->canvas),
		0.0, 0.0, MINIMUM_WIDTH, MINIMUM_HEIGHT);

	gtk_widget_set_size_request(
		gnome_hint->_priv->canvas,
		MINIMUM_WIDTH, MINIMUM_HEIGHT);

	gtk_container_add (GTK_CONTAINER (sw), gnome_hint->_priv->canvas);

	gnome_hint->_priv->checkbutton = 
		gtk_check_button_new_with_mnemonic (_("_Show Hints at Startup"));
	
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (gnome_hint)->vbox),
			    gnome_hint->_priv->checkbutton, TRUE, TRUE, 0);

	g_signal_connect_swapped (
		gnome_hint->_priv->checkbutton, "clicked",
		G_CALLBACK (checkbutton_clicked), gnome_hint);

	gtk_dialog_add_button (
			GTK_DIALOG (gnome_hint),
			GTK_STOCK_GO_BACK,
			GNOME_HINT_RESPONSE_PREV);

	gtk_dialog_add_button (
			GTK_DIALOG (gnome_hint),
			GTK_STOCK_GO_FORWARD,
			GNOME_HINT_RESPONSE_NEXT);

	gtk_dialog_add_button (
			GTK_DIALOG (gnome_hint),
			GTK_STOCK_OK,
			GTK_RESPONSE_OK);
  
	g_signal_connect_swapped (
			gnome_hint, "response", 
			G_CALLBACK (dialog_response), NULL);
}

/*find the language, but only look as far as gotlang*/
static const char *
find_lang (const char * const *langs_pointer, const char *gotlang, const char *lang)
{
	int i;

	for (i = 0;
	     langs_pointer[i] != NULL && langs_pointer[i] != gotlang;
	     i++) {
                if (strcmp (langs_pointer[i], lang) == 0)
                        return langs_pointer[i];
        }
        return NULL;
}

/*parse all children and pick out the best language one*/
static char *
get_i18n_string (xmlDocPtr doc, xmlNodePtr child, const char *name)
{
        char *current;
        xmlNodePtr cur;
        const char * const *langs_pointer;
        const char  *gotlang = NULL; /*the highest language we got*/

        langs_pointer = g_get_language_names ();

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
                        const char *ll = find_lang (langs_pointer, gotlang, lang);
                        xmlFree (lang);
                        if (ll != NULL) {
                                if (current != NULL)
                                        xmlFree (current);

                                current = xmlNodeListGetString (doc, cur->xmlChildrenNode, 1);
                                gotlang = ll;
                                if (ll == langs_pointer[0]) /*we can't get any better then this*/
                                        break;
                        }
                }
        }
        return current;
}

static GList*
read_hints_from_file (const char *file, GList *hintlist)
{
        xmlNodePtr cur, root;
        xmlDocPtr  doc;

        if (!(doc = xmlParseFile (file)))
                return NULL;

	if (!(root = xmlDocGetRootElement (doc))) {
                xmlFreeDoc (doc);
                return NULL;
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
	int rnd = g_random_int_range (0, hintnum);
        gh->_priv->curhint = g_list_nth (gh->_priv->hintlist, rnd);
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

  if (!g_file_test(hintfile,G_FILE_TEST_EXISTS)) return NULL;

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

  gnome_hint_set_accessible_information (gnome_hint, title);

  return GTK_WIDGET (gnome_hint);
}

/*
 * Set accessible information like name and description
 */
static void
gnome_hint_set_accessible_information (GnomeHint *gh, const gchar *name)
{
	GtkWidget *widget;
	AtkObject *aobj;

	widget = gh->_priv->canvas;
	g_return_if_fail (widget != NULL);

	aobj = gtk_widget_get_accessible (widget);

	/* Return immediately if GAIL is not loaded */
	if (!GTK_IS_ACCESSIBLE (aobj))
		return;

	if (name)
		atk_object_set_name (aobj, name);
	atk_object_set_description (aobj, (gchar *) gh->_priv->curhint->data);
}
