/* gnome-about.c:
 *
 * Copyright (C) 2002 Sun Microsystems, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *      Glynn Foster <glynn.foster@sun.com>
 *      Mark McLoughlin <mark@skynet.ie>
 */

/*
 * TODO:
 *  * Lots of magic numbers here. Each of them should
 *    be calculated based on the pixbuf sizes and text
 *    size. Try this on a large print theme ! :/
 *  * Get the translator credits from the translations.
 *  * Allow the text in gnome-version.xml to be translated.
 *  * The text isn't accessible.
 */

#include <config.h>

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>

#include <libgnomecanvas/gnome-canvas.h>
#include <libgnomecanvas/gnome-canvas-text.h>

#include <libxml/tree.h>
#include <libxml/parser.h>

#include "contributors.h"

static char *translators [] = {
	"Me me me",
	"You you you",
	"And the christmas fairy",
	"FIXME",
	NULL
};

#define GNOME_LOGO_FILENAME "gnome-foot-large.png"

#define CANVAS_MAX_X 480.0
#define CANVAS_MAX_Y 300.0

#define RESPONSE_CREDITS 1
#define STOCK_CREDITS "credits"


typedef struct {
	GnomeCanvasItem *item;
	GnomeCanvas     *canvas;
	GdkPixbuf       *logo;
	int              width;
	int              height;
	int              alpha;
} LogoMergeData;

typedef struct {
	GnomeCanvasItem *item;
	GnomeCanvas     *canvas;
	GnomeCanvasItem *header_item;
	char            *text;
	gdouble          x;
	gdouble          y;
} FoundationLogoData;

typedef struct {
	GnomeCanvasItem *item;
	GnomeCanvas     *canvas;
} AnimationData;

typedef struct {
	char *file;
	char *text;
} FoundationLogo;

static FoundationLogo foundation_logos [] = {
	{ "compaq.gif", N_("Compaq") },
	{ "debian.gif", N_("Debian") },
	{ "fsf.gif", N_("The Free Software Foundation") },
	{ "hp.gif", N_("Hewlett-Packard") },
	{ "ibm.gif", N_("IBM") },
	{ "mandrakesoft.gif", N_("MandrakeSoft") },
	{ "osdn.gif", N_("Open Source\nDevelopment Network") },
	{ "redflag.gif", N_("Red Flag") },
	{ "redhat.gif", N_("Red Hat") },
	{ "sun.gif", N_("Sun Microsystems") },
	{ "ximian.jpg", N_("Ximian") }
};

static PangoLayout  *global_layout = NULL;
static char        **introduction_messages =  NULL;

static gboolean display_foundation_logo      (gpointer data);
static gboolean display_introduction_message (gpointer data);


static gboolean
animate_logo (gpointer data)
{
	FoundationLogoData *logo_data = (FoundationLogoData *) data;
	GnomeCanvasItem    *item = logo_data->item;
	static int          stop_counter = 0;

	if (stop_counter > 0) {
		static GnomeCanvasItem *logo_text;

		if (stop_counter == 20) {
			char *text;

			text = g_strdup_printf ("<big><b>%s</b></big>",
						logo_data->text);

			logo_text = gnome_canvas_item_new (
					GNOME_CANVAS_GROUP (logo_data->canvas->root),
					gnome_canvas_text_get_type (),
					"markup", text,
					"anchor", GTK_ANCHOR_NW,
					"x", logo_data->x,
					"y", logo_data->y,
					NULL);

			g_free (text);
		}

		if (--stop_counter == 0) {
			g_assert (logo_text != NULL);
			gtk_object_destroy (GTK_OBJECT (logo_text));
			logo_text = NULL;
			stop_counter = -1;
		}

		return TRUE;
	}

	if (stop_counter == 0 &&
	    item->x1 + ((item->x2 - item->x1) / 2) <= CANVAS_MAX_X / 2) {
		stop_counter = 20;
		return TRUE;
	}

	if (item->x2 <= 0.0) {
		AnimationData *anim_data;

		stop_counter = 0;

		anim_data = g_new (AnimationData, 1);

		anim_data->item   = logo_data->header_item;
		anim_data->canvas = logo_data->canvas;

		g_timeout_add (1 * 1000, display_foundation_logo, anim_data);

		g_free (logo_data);

		return FALSE;
	}

	gnome_canvas_item_move (item, -10.0, 0.0);

	return TRUE;
}

static GdkPixbuf *
get_logo_pixbuf (FoundationLogo *logos,
		 int            *iter)
{
	GdkPixbuf *retval = NULL;

	while (!retval) {
		GError *error = NULL;
		char   *image;
		char   *tmp;

		if (*iter >= G_N_ELEMENTS (foundation_logos))
			break;

		tmp = g_strdup_printf ("gnome-about/%s",
				       logos [*iter].file);
		image = gnome_program_locate_file (
				NULL, GNOME_FILE_DOMAIN_PIXMAP,
				tmp, TRUE, NULL);	
		g_free (tmp);
		if (!image) {
			g_warning (_("Unable to locate '%s'"),
				   logos [*iter].file);
			(*iter)++;
			continue;
		}
	
		retval = gdk_pixbuf_new_from_file (image, &error);
		if (error) {
			g_warning (_("Unable to load '%s': %s"),
				   logos [*iter].file,
				   error->message);
			g_error_free (error);
			g_free (image);
			(*iter)++;
			continue;
		}
		g_free (image);
	}

	return retval;
}

static FoundationLogo *
randomize_logos (FoundationLogo *logos,
		 int             n_elements)
{
	FoundationLogo *retval;
	GSList         *list = NULL;
	int             i = 0;

	for (i = 0; i < n_elements; i++)
		list = g_slist_prepend (list, &logos [i]);

	retval = g_new (FoundationLogo, n_elements);

	i = 0;

	while (list != NULL) {
		FoundationLogo *logo;
		GSList         *l;
		int             random = rand () % g_slist_length (list);

		l = g_slist_nth (list, random);
		logo = (FoundationLogo *) l->data;
		list = g_slist_delete_link (list, l);

		retval [i].file = logo->file;
		retval [i].text = _(logo->text);
		i++;
	}

	g_assert (i == n_elements);

	return retval;
}

static gboolean
display_foundation_logo (gpointer data)
{
	AnimationData          *anim_data = (AnimationData *) data;
	FoundationLogoData     *logo_data;
	GdkPixbuf              *pixbuf;
	int                     logo_width;
	int                     logo_height;
	static FoundationLogo  *randomized_logos = NULL;
	static GnomeCanvasItem *logo = NULL;
	static int              i = 0;

	if (i >= G_N_ELEMENTS (foundation_logos)) {
		gtk_object_destroy (GTK_OBJECT (anim_data->item));
		g_free (randomized_logos);
		g_free (anim_data);
		return FALSE;
	}

	if (!randomized_logos)
		randomized_logos = randomize_logos (
					foundation_logos,
					G_N_ELEMENTS (foundation_logos));

	pixbuf = get_logo_pixbuf (randomized_logos, &i);
	if (!pixbuf) {
		gtk_object_destroy (GTK_OBJECT (anim_data->item));
		g_free (anim_data);
		return FALSE;
	}

	logo_width  = gdk_pixbuf_get_width  (pixbuf);
	logo_height = gdk_pixbuf_get_height (pixbuf);

	if (!logo) {
		logo = gnome_canvas_item_new (
				GNOME_CANVAS_GROUP (anim_data->canvas->root),
				gnome_canvas_pixbuf_get_type (),
				"pixbuf", pixbuf,
				"x", CANVAS_MAX_X + logo_width  + 10.0,
				"y", 170.0,
				NULL);
		gnome_canvas_item_lower_to_bottom (logo);
	} else {
		gnome_canvas_item_move (
			logo,
			(CANVAS_MAX_X + logo_width  + 10.0) - logo->x1,
			170.0 - logo->y1);
		gnome_canvas_item_set (logo, "pixbuf", pixbuf, NULL);
	}

	g_object_unref (pixbuf);

	logo_data = g_new (FoundationLogoData, 1);

	logo_data->item        = logo;
	logo_data->canvas      = anim_data->canvas;
	logo_data->header_item = anim_data->item;
	logo_data->text        = randomized_logos [i].text;
	logo_data->x           = CANVAS_MAX_X / 2 + logo_width / 2 + 10.0;
	logo_data->y           = 170.0;

	g_timeout_add (50, animate_logo, logo_data); 

	i++;

	g_free (anim_data);

	return FALSE;
}

static void
display_foundation_logos (GnomeCanvas *canvas)
{
	AnimationData   *anim_data;
	GnomeCanvasItem *item;
	char            *text;

	text = g_strdup_printf ("<big><b>%s</b></big>",
				_("Supported by:"));

	item = gnome_canvas_item_new (
			GNOME_CANVAS_GROUP (canvas->root),
			gnome_canvas_text_get_type (),
			"markup", text, 
			"anchor", GTK_ANCHOR_NW,
			"x", 150.0, "y", 150.0, NULL);

	g_free (text);

	anim_data = g_new (AnimationData, 1);

	anim_data->canvas = canvas;
	anim_data->item   = item;

	display_foundation_logo (anim_data);
}

static gboolean
animate_text (gpointer data)
{
	AnimationData *anim_data = (AnimationData *) data;

	gnome_canvas_item_move (anim_data->item, 0.0, -10.0);

	if (anim_data->item->y1 <= 60.0) {
		g_timeout_add (5 * 1000,
			       display_introduction_message,
			       anim_data->canvas);
		g_free (anim_data);
		return FALSE;
	}

	return TRUE;
}

static gboolean
display_introduction_message (gpointer data)
{
	GnomeCanvas            *canvas = (GnomeCanvas *) data;
	AnimationData          *anim_data;
	static GnomeCanvasItem *introduction_text = NULL;
	static int              i = 0;

	if (!introduction_messages [i])
		return FALSE;

	if (!introduction_text)
		introduction_text =
			gnome_canvas_item_new (
				GNOME_CANVAS_GROUP (canvas->root),
				gnome_canvas_rich_text_get_type (),
				"text", introduction_messages [i],
			        "x", 150.0,
				"y", CANVAS_MAX_Y + 20.0,
				"width", 320.0,
				"height", 220.0, 
				"cursor_visible", FALSE,
				NULL);
	else {
		gnome_canvas_item_move (
			introduction_text,
			0.0,
			(CANVAS_MAX_Y + 20.0) - introduction_text->y1);
		gnome_canvas_item_set (
			introduction_text,
			"text", introduction_messages [i],
			NULL);
	}

	anim_data = g_new (AnimationData, 1);

	anim_data->canvas = canvas;
	anim_data->item   = introduction_text;

	g_timeout_add (10, animate_text, anim_data); 

	i++;

	return FALSE;
}

static void
display_introduction_text (GnomeCanvas *canvas)
{
	char *text;

	text = g_strdup_printf ("<big><b>%s</b></big>",
				_("Welcome to the GNOME Desktop"));

	gnome_canvas_item_new (
		GNOME_CANVAS_GROUP (canvas->root),
		gnome_canvas_text_get_type (),
		"markup", text, 
		"anchor", GTK_ANCHOR_NW,
		"x", 150.0, "y", 30.0, NULL);

	g_free (text);

	if (introduction_messages)
		display_introduction_message (canvas);

	display_foundation_logos (canvas);
}

static gdouble
get_text_height (const char *text)
{
	PangoRectangle extents;

	g_assert (global_layout != NULL);

	pango_layout_set_markup (global_layout, text, -1);
	pango_layout_get_pixel_extents (global_layout, NULL, &extents);

	return extents.height;
}

static char *
strip_newlines (const char *str)
{
	char **strv;
	char  *retval;

	if (!str)
		return NULL;

	strv = g_strsplit (str, "\n", -1);

	retval = g_strjoinv ("", strv);

	g_strfreev (strv);

	return retval;
}

static void
get_description_messages (xmlNodePtr node)
{
	xmlNodePtr  paras;
	GSList     *list = NULL, *l;
	int         i;

	paras = node->children;

	while (paras) {
		char *name  = (char*) paras->name;
		char *value = (char*) xmlNodeGetContent (paras);

		if (!g_ascii_strcasecmp (name, "p") && value && value [0])
			list = g_slist_prepend (list, strip_newlines (value));

		paras = paras->next;
	}

	list = g_slist_reverse (list);

	introduction_messages = g_new (char *, g_slist_length (list) + 1);

	for (i = 0, l = list; l; l = l->next, i++)
		introduction_messages [i] = l->data;

	introduction_messages [i] = NULL;

	g_slist_free (list);
}

static void
display_version_info (GnomeCanvas *canvas,
		      gdouble      x,
		      gdouble      y)
{
	xmlDocPtr   about;
	xmlNodePtr  node;
	xmlNodePtr  bits;
	char       *platform = NULL;
	char       *minor = NULL;
	char       *micro = NULL;
	char       *version_string = NULL;
	char       *vendor_string = NULL;
	char       *build_date_string = NULL;
	char       *text = NULL;

	about = xmlParseFile (gnome_program_locate_file (
			      NULL, GNOME_FILE_DOMAIN_DATADIR,
			      "gnome-about/gnome-version.xml", TRUE, NULL) );
	if (!about)
		return;

	node = about->children;
	
	if (g_ascii_strcasecmp (node->name, "gnome-version")) {
		xmlFreeDoc (about);
		return;
	}

	bits = node->children;

	while (bits) {
		char *name  = (char*) bits->name;
		char *value;

		if (!g_ascii_strcasecmp (name, "description"))
			get_description_messages (bits);

		value = (char*) xmlNodeGetContent (bits);

		if (!g_ascii_strcasecmp (name, "platform") && value && value [0])
			platform = g_strdup (value);
		if (!g_ascii_strcasecmp (name, "minor") && value && value [0])
			minor = g_strdup (value);
		if (!g_ascii_strcasecmp (name, "micro") && value && value [0])
			micro = g_strdup (value);
		if (!g_ascii_strcasecmp (name, "vendor") && value && value [0])
			vendor_string = g_strdup (value);
		if (!g_ascii_strcasecmp (name, "date") && value && value [0])
			build_date_string = g_strdup (value);
		
		bits = bits->next;
	}

	xmlFreeDoc (about);

	if (!minor)
		version_string = g_strconcat (platform, NULL);

	if (!version_string && !micro)
		version_string = g_strconcat (platform, ".", minor, NULL);

	if (!version_string)
		version_string = g_strconcat (platform, ".", minor, ".", micro, NULL);

	g_free (platform);
	g_free (minor);
	g_free (micro);

	if (version_string && version_string [0]) {
		text = g_strdup_printf ("<big><b>%s: </b>%s</big>",
					_("Version"), version_string);
		gnome_canvas_item_new (
			GNOME_CANVAS_GROUP (canvas->root),
			gnome_canvas_text_get_type (),
			"markup", text, 
			"anchor", GTK_ANCHOR_NW,
			"x", x,
			"y", y,
			NULL);

		y += get_text_height (text) + 4.0;

		g_free (text);
	}

	if (vendor_string && vendor_string [0]) {
		text = g_strdup_printf ("<big><b>%s: </b>%s</big>",
					_("Vendor"), vendor_string);
		gnome_canvas_item_new (
			GNOME_CANVAS_GROUP (canvas->root),
			gnome_canvas_text_get_type (),
			"markup", text, 
			"anchor", GTK_ANCHOR_NW,
			"x", x,
			"y", y,
			NULL);

		y += get_text_height (text) + 4.0;

		g_free (text);
	}

	if (build_date_string && build_date_string [0]) {
		text = g_strdup_printf ("<big><b>%s: </b>%s</big>",
					_("Build Date"), build_date_string);
		gnome_canvas_item_new (
			GNOME_CANVAS_GROUP (canvas->root),
			gnome_canvas_text_get_type (),
			"markup", text, 
			"anchor", GTK_ANCHOR_NW,
			"x", x,
			"y", y,
			NULL);

		y += get_text_height (text) + 4.0;

		g_free (text);
	}

	g_free (version_string);
	g_free (vendor_string);
	g_free (build_date_string);
}

static gboolean 
merge_in_logo (gpointer data)
{
	LogoMergeData *merge_data = (LogoMergeData *) data;
	GdkPixbuf     *merged;

	if (merge_data->alpha == -1) {
		GnomeCanvas *canvas;

		gnome_canvas_item_set (
			merge_data->item, "pixbuf",
			merge_data->logo, NULL);

		g_object_unref (merge_data->logo);

		canvas = merge_data->canvas;

		g_free (merge_data);

		display_version_info (
			canvas, 10.0, 20.0 + merge_data->height);

		display_introduction_text (canvas);

		return FALSE;
	}

	merged = gdk_pixbuf_composite_color_simple (
			merge_data->logo,
			merge_data->width,
			merge_data->height,
			GDK_INTERP_HYPER,
			merge_data->alpha,
			128, 0xffffffff, 0xffffffff);

	gnome_canvas_item_set (merge_data->item, "pixbuf", merged, NULL);

	g_object_unref (merged);

	if ((merge_data->alpha += 15) >= 0xff)
		merge_data->alpha = -1;

	return TRUE;
}

static void
show_error_dialog (const char *message)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (NULL,
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_CLOSE,
					 message);

	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_widget_show (dialog);

	gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);
}

static gboolean
load_gnome_logo (GnomeCanvas     *canvas,
		 GnomeCanvasItem *item)
{
	LogoMergeData *merge_data;
	GdkPixbuf     *logo = NULL;
	GError        *error = NULL;
	char          *image;

	image = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP,
					   GNOME_LOGO_FILENAME,
					   TRUE, NULL);	
	if (!image) {
		char *message;

		message = g_strdup_printf (_("Unable to locate '%s'"), 
					   GNOME_LOGO_FILENAME);
		show_error_dialog (message);

		g_free (message);

		return FALSE;
	}
	
	logo = gdk_pixbuf_new_from_file (image, &error);
	if (error) {
		char *message;

		message = g_strdup_printf (_("Unable to load '%s': %s"), 
					   GNOME_LOGO_FILENAME,
					   error->message);
		show_error_dialog (message);

		g_free (message);
		g_free (image);
		g_error_free (error);

		return FALSE;
	}
	g_free (image);

	merge_data = g_new (LogoMergeData, 1);

	merge_data->item    = item;
	merge_data->logo    = logo;
	merge_data->canvas  = canvas;
	merge_data->width   = gdk_pixbuf_get_width (logo);
	merge_data->height  = gdk_pixbuf_get_height (logo);
	merge_data->alpha   = 0;

	g_timeout_add (50, merge_in_logo, merge_data);	

	return TRUE;
}

static GtkWidget *
create_canvas (void)
{
	GnomeCanvasItem *item;
	GtkWidget       *canvas;
	GdkColor         color = { 0, 0xffff, 0xffff, 0xffff };
	
	canvas = gnome_canvas_new ();
        gnome_canvas_set_scroll_region (GNOME_CANVAS (canvas), 0, 0,
					CANVAS_MAX_X, CANVAS_MAX_Y);

	global_layout = gtk_widget_create_pango_layout (
				GTK_WIDGET (canvas), NULL);

        gtk_widget_set_size_request (canvas, CANVAS_MAX_X, CANVAS_MAX_Y);

	gdk_colormap_alloc_color (gtk_widget_get_colormap (GTK_WIDGET (canvas)),
				  &color, FALSE, TRUE);

	gtk_widget_modify_bg (GTK_WIDGET (canvas), GTK_STATE_NORMAL, &color);

	item = gnome_canvas_item_new (GNOME_CANVAS_GROUP (GNOME_CANVAS (canvas)->root),
				      gnome_canvas_pixbuf_get_type (),
				      "x", 10.0,
				      "y", 10.0,
				      NULL);

	if (!load_gnome_logo (GNOME_CANVAS (canvas), item)) {
		gtk_widget_destroy (canvas);
		return NULL;
	}

	return canvas;
}

static GtkWidget *
create_label (char **peoples)
{
	GtkWidget *label;
	GString   *string;
	int        i;

	label = gtk_label_new ("");
	gtk_label_set_selectable (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	gtk_misc_set_padding (GTK_MISC (label), 8, 8);

	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

	string = g_string_new ("");

	for (i = 0; peoples [i]; i++) {
		char *tmp;

		tmp = g_markup_escape_text (_(peoples [i]), -1);
		g_string_append (string, tmp);

		if (peoples [i + 1])
			g_string_append (string, "\n");
		g_free (tmp);
	}

	gtk_label_set_markup (GTK_LABEL (label), string->str);
	g_string_free (string, TRUE);

	return label;
}

static void
display_credits_dialog (GtkDialog *parent)
{
	static GtkWidget *dialog = NULL;
	GtkWidget        *label;
	GtkWidget        *notebook;
	GtkWidget        *sw;

	if (dialog) {
		gtk_window_present (GTK_WINDOW (dialog));
		return;
	}

	dialog = gtk_dialog_new_with_buttons (_("Credits"),
					      GTK_WINDOW (parent),
					      GTK_DIALOG_DESTROY_WITH_PARENT,
					      GTK_STOCK_OK, GTK_RESPONSE_OK,
					      NULL);

	gtk_window_set_default_size (GTK_WINDOW (dialog), 360, 260);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	g_signal_connect (dialog, "response",
			  G_CALLBACK (gtk_widget_destroy), dialog);
	g_signal_connect (dialog, "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &dialog);

	notebook = gtk_notebook_new ();
	gtk_container_set_border_width (GTK_CONTAINER (notebook), 8);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), notebook, TRUE, TRUE, 0);

	if (contributors) {
		label = create_label (contributors);

		sw = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
						GTK_POLICY_AUTOMATIC,
						GTK_POLICY_AUTOMATIC);

		gtk_scrolled_window_add_with_viewport (
				GTK_SCROLLED_WINDOW (sw), label);
		gtk_viewport_set_shadow_type (
				GTK_VIEWPORT (GTK_BIN (sw)->child),
				GTK_SHADOW_NONE);

		gtk_notebook_append_page (
				GTK_NOTEBOOK (notebook), sw,
				gtk_label_new_with_mnemonic (_("Brought to you by")));
	}

	if (translators) {
		label = create_label (translators);

		sw = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
						GTK_POLICY_AUTOMATIC,
						GTK_POLICY_AUTOMATIC);

		gtk_scrolled_window_add_with_viewport (
				GTK_SCROLLED_WINDOW (sw), label);
		gtk_viewport_set_shadow_type (
				GTK_VIEWPORT (GTK_BIN (sw)->child),
				GTK_SHADOW_NONE);

		gtk_notebook_append_page (
				GTK_NOTEBOOK (notebook), sw,
				gtk_label_new_with_mnemonic (_("Translated by")));
	}

	gtk_widget_show_all (dialog);
}

static gboolean 
quit_callback (GtkWidget *widget,
	       gpointer  dummy)
{
	gtk_main_quit ();

	return FALSE;
}

static void
response_callback (GtkDialog *dialog,
		   int        response_id,
		   gpointer   dummy)
{
	if (response_id == RESPONSE_CREDITS) {
		display_credits_dialog (dialog);
		return;
	}

	quit_callback (GTK_WIDGET (dialog), NULL);
}

static inline void
register_stock_item (void)
{
	GtkIconFactory *factory;
	GtkIconSet     *cancel_icons;

	static GtkStockItem  credits_item [] = {
		{ STOCK_CREDITS, N_("C_redits"), 0, 0, GETTEXT_PACKAGE },
	};

	cancel_icons = gtk_icon_factory_lookup_default (GNOME_STOCK_ABOUT);

	factory = gtk_icon_factory_new ();

	gtk_icon_factory_add (factory, STOCK_CREDITS, cancel_icons);

	gtk_icon_factory_add_default (factory);

	gtk_stock_add_static (credits_item, 1);
}


static GtkWidget *
create_about_dialog (void)
{
	GtkWidget *canvas;
	GtkWidget *dialog;
	GtkWidget *button;

	dialog = gtk_dialog_new_with_buttons (_("About the GNOME Desktop"), 
					      NULL, 0,
					      GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,	
				   	      NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CLOSE);

	register_stock_item ();

	button = gtk_dialog_add_button (
			GTK_DIALOG (dialog), STOCK_CREDITS, RESPONSE_CREDITS);
        gtk_button_box_set_child_secondary (
			GTK_BUTTON_BOX (GTK_DIALOG (dialog)->action_area),
			button, TRUE);

	g_signal_connect (dialog, "destroy", G_CALLBACK (quit_callback), NULL);
	g_signal_connect (dialog, "delete_event", G_CALLBACK (quit_callback), NULL);
	g_signal_connect (dialog, "response", G_CALLBACK (response_callback), NULL);

	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
	g_object_set (dialog, "allow_shrink", FALSE, "allow_grow", FALSE, NULL);

	if (!(canvas = create_canvas ())) {
		gtk_widget_destroy (dialog);
		return NULL;
	}

	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox),
					GNOME_PAD_SMALL);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    canvas, TRUE, TRUE, 0);

	return dialog;
}

int
main (int argc, char *argv[])   
{
	GtkWidget *dialog;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gnome_program_init ("gnome-about", "1.0", LIBGNOMEUI_MODULE,
			    argc, argv, NULL);
	gnome_window_icon_set_default_from_file (
		GNOME_ICONDIR "/gnome-logo-icon-transparent.png");

	if (!(dialog = create_about_dialog ()))
		return -1;

	gtk_widget_show_all (dialog);

	gtk_main ();

	if (global_layout)
		g_object_unref (global_layout);

	if (introduction_messages)
		g_strfreev (introduction_messages);

	return 0;
}
