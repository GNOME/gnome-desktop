/* gnome-about.c: Super Funky Dope gnome about window.
 *
 * Copyright (C) 2002, Sun Microsystems, Inc.
 * Copyright (C) 2003, Kristian Rietveld
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
 *	Glynn Foster <glynn.foster@sun.com>
 *	Mark McLoughlin <mark@skynet.ie>
 *	Kristian Rietveld <kris@gtk.org>
 *	Jeff Waugh <jdub@perkypants.org>
 *	Malcolm Tredinnick <malcolm@commsecure.com.au>
 */

#include <config.h>


#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>
#include <libgnomecanvas/libgnomecanvas.h>

/* for parsing gnome-version.xml */
#include <libxml/tree.h>
#include <libxml/parser.h>

/* for readdir */
#include <sys/types.h>
#include <string.h>

#include <dirent.h>
#include <errno.h>
#include <time.h>

#include "contributors.h"

/* pick some good defaults */
static gdouble canvas_width = 550.0;
static gdouble canvas_height = 350.0;


static char             **introduction_messages = NULL;
static GnomeCanvasItem   *subheader = NULL;
static gdouble            version_info_height = 0.0;
static gint               contrib_i = 0;
static gint              *contrib_order;

/* funky animations */
typedef struct {
	GnomeCanvas *canvas;
	GnomeCanvasItem *item;
} AnimationData;

/* contributors */
static gboolean   display_contributors (gpointer data);

static gboolean
animate_contributor (gpointer data)
{
	AnimationData *ani_data = (AnimationData *)data;
	gboolean before_middle;
	gdouble tmp, tmp2, y;
	guint color, tmpcolor;
	gdouble size;

	g_object_get (ani_data->item,
		      "text_width", &tmp,
		      "text_height", &tmp2,
		      "fill_color_rgba", &color,
		      "size_points", &size,
		      NULL);

	y = ani_data->item->parent->y1;

	/* ugh */
	g_object_set (GNOME_CANVAS_GROUP (ani_data->item->parent)->item_list->data,
		      "x1", -1.0 * (tmp / 2.0 + 3.0),
		      "y1", -1.0 * (tmp2 / 2.0 + 3.0),
		      "x2", tmp / 2.0 + 3.0,
		      "y2", tmp2 / 2.0 + 3.0,
		      NULL);

	before_middle = 130.0 + ((canvas_height - 130.0 - version_info_height - tmp2) / 2.0) < y;

	/* update the color */
	tmpcolor = color & 0xff;
	tmpcolor += before_middle ? -6 : 6;
	size += before_middle ? 1.0 : -1.0;
	size = MAX (0.0, size);
	color = GNOME_CANVAS_COLOR_A (tmpcolor, tmpcolor, tmpcolor, tmpcolor);

	g_object_set (ani_data->item,
		      "fill_color_rgba", color,
		      "size_points", size,
		      NULL);

	/* move damnit!! */
	gnome_canvas_item_move (ani_data->item->parent, 0.0, -2.5);

	y -= 2.5;

	gnome_canvas_update_now (ani_data->canvas);

	/* time for a new one ??? */
	if (y <= 130.0) {
		display_contributors (ani_data->canvas);
		g_free (ani_data);
		return FALSE;
	}

	return TRUE;
}

static gboolean
canvas_button_press_event (GtkWidget      *widget,
			   GdkEventButton *event,
			   gpointer        user_data)
{
	gchar *text;

	/* the links should still be clickable */
	if (event->y <= 80.0)
		return FALSE;

	if (contrib_i >= G_N_ELEMENTS (contributors))
		contrib_i = 0;

	text = g_strdup_printf ("<b>%s</b>", 
				contributors[contrib_order[contrib_i]]);
	contrib_i++;

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (user_data),
			       "markup", text,
			       NULL);
	g_free (text);

	return TRUE;
}

static gboolean
display_contributors (gpointer data)
{
	GnomeCanvas *canvas = GNOME_CANVAS (data);
	AnimationData *ani_data;

	static GnomeCanvasItem *contributor = NULL;
	static GnomeCanvasItem *contributor_text = NULL;

	if (contrib_i >= G_N_ELEMENTS (contributors))
		contrib_i = 0;

	if (!contributor) {
		gchar *text;

		contributor =
			gnome_canvas_item_new (GNOME_CANVAS_GROUP (canvas->root),
					       gnome_canvas_group_get_type (),
					       NULL);

		text = g_strdup_printf ("<b>%s</b>", 
					contributors[contrib_order[contrib_i]]);
		contributor_text =
			gnome_canvas_item_new (GNOME_CANVAS_GROUP (contributor),
					       gnome_canvas_text_get_type (),
					       "markup", text,
					       "anchor", GTK_ANCHOR_CENTER,
					       "fill_color_rgba", 0xffffffff,
					       NULL);
		g_free (text);

		g_signal_connect (canvas, "button_press_event",
				  G_CALLBACK (canvas_button_press_event),
				  contributor_text);

		gnome_canvas_item_move (contributor,
				        canvas_width / 2.0,
				        canvas_height - version_info_height);
	} else {
		gchar *text;

		text = g_strdup_printf ("<b>%s</b>", 
					contributors[contrib_order[contrib_i]]);
		gnome_canvas_item_set (contributor_text,
				       "markup", text,
				       "fill_color_rgba", 0xffffffff,
				       "size_points", 0.0,
				       NULL);
		g_free (text);

		gnome_canvas_item_move (contributor, 0.0,
				        canvas_height - version_info_height - contributor->y1);
	}

	ani_data = g_new0 (AnimationData, 1);

	ani_data->canvas = canvas;
	ani_data->item = contributor_text;

	g_timeout_add (75, animate_contributor, ani_data);
	contrib_i++;

	return FALSE;
}

/* subheader */
static gboolean
display_subheader (gpointer data)
{
	GnomeCanvas *canvas = GNOME_CANVAS (data);
	static gboolean first = TRUE;

	if (first) {
		guint color = GNOME_CANVAS_COLOR_A (0xff, 0xff, 0xff, 0xff);

		gnome_canvas_item_set (subheader,
				       "fill_color_rgba", color,
				       NULL);
		gnome_canvas_item_show (subheader);
		gnome_canvas_update_now (canvas);

		first = FALSE;
	} else {
		guint color;
		guint tmp;

		g_object_get (subheader, "fill_color_rgba", &color, NULL);
		tmp = color & 0xFF;

		if (tmp < 15) {
			/* make sure it's black */
			color = GNOME_CANVAS_COLOR_A (0x0, 0x0, 0x0, 0x0);
			g_object_set (subheader,
				      "fill_color_rgba", color, NULL);
			gnome_canvas_update_now (canvas);

			display_contributors (canvas);

			return FALSE;
		}

		tmp -= 15;
		color = GNOME_CANVAS_COLOR_A (tmp, tmp, tmp, tmp);
		g_object_set (subheader, "fill_color_rgba", color, NULL);
		gnome_canvas_update_now (canvas);
	}

	return TRUE;
}

/* introduction messages */
static gboolean	   display_introduction_message (gpointer data);

static gboolean
animate_text (gpointer data)
{
	AnimationData *ani_data = (AnimationData *)data;

	gnome_canvas_item_move (ani_data->item->parent, 0.0, -10.0);

	if (ani_data->item->parent->y1 <= 120.0) {
		g_timeout_add (5 * 1000,
			       display_introduction_message,
			       ani_data->canvas);
		g_free (ani_data);
		return FALSE;
	}

	return TRUE;
}

static gboolean
display_introduction_message (gpointer data)
{
	GnomeCanvas *canvas = GNOME_CANVAS (data);
	AnimationData *ani_data;

	static GnomeCanvasItem *intro = NULL;
	static GnomeCanvasItem *intro_text = NULL;
	static gint intro_i = 0;

	if (!introduction_messages || !introduction_messages[intro_i]) {
		/* just hide the intro now, the canvas will
		 * take care of disposing it (if we do an _unref here
		 * it crashes for some reason, too lazy to find out why)
		 */
		if (intro)
			gnome_canvas_item_hide (GNOME_CANVAS_ITEM (intro));
		intro = NULL;

		g_timeout_add (100, display_subheader, canvas);
		return FALSE;
	}

	if (!intro_text) {
		gdouble tmp;

		intro =
			gnome_canvas_item_new (GNOME_CANVAS_GROUP (canvas->root),
					       gnome_canvas_group_get_type (),
					       NULL);

		intro_text =
			gnome_canvas_item_new (GNOME_CANVAS_GROUP (intro),
					       gnome_canvas_rich_text_get_type (),
					       "text", introduction_messages[intro_i],
						"editable", FALSE,
					       /* FIXME */
					       "width", 300.0,
					       "height", 80.0,
					       "grow_height", TRUE,
					       "cursor_visible", FALSE,
					       NULL);

		g_object_get (intro_text, "height", &tmp, NULL);
		gnome_canvas_item_move (intro,
					(canvas_width - 300.0) / 2.0,
					 canvas_height - version_info_height - tmp);
		gnome_canvas_update_now (canvas);
	} else {
		gdouble y;
		gdouble tmp;

		gnome_canvas_item_set (intro_text,
				       "text", introduction_messages[intro_i],
				       NULL);

		g_object_get (intro_text, "height", &tmp, NULL);
		y = intro->y1;

		gnome_canvas_item_move (intro,
					0.0,
					canvas_height - version_info_height - y - tmp);
		gnome_canvas_update_now (canvas);
	}

	ani_data = g_new0 (AnimationData, 1);

	ani_data->canvas = canvas;
	ani_data->item = intro_text;

	g_timeout_add (15, animate_text, ani_data);
	intro_i++;

	return FALSE;
}

static void
start_animations (GtkWidget *widget,
		  gpointer   user_data)
{
	display_introduction_message (GNOME_CANVAS (widget));
}

/* loading funky stuff */
static void
show_error_dialog (const gchar *message)
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

static GdkPixbuf *
load_random_header (void)
{
	GdkPixbuf *pixbuf;
	GList *files = NULL, *i;
	GError *error = NULL;
	gchar *text;
	gchar *directory;
	gint selected;

	DIR *dir;
	struct dirent *d;

	directory = gnome_program_locate_file (NULL,
					       GNOME_FILE_DOMAIN_APP_DATADIR,
					       "headers",
					       TRUE, NULL);

	if (!directory) {
		show_error_dialog (_("Could not locate the directory with header images."));

		return NULL;
	}

	dir = opendir (directory);
	if (!dir) {
		char *message;

		message = g_strdup_printf (_("Failed to open directory with header images: %s"),
					   strerror (errno));
		show_error_dialog (message);
		g_free (message);
		g_free (directory);

		return NULL;
	}

	while ((d = readdir (dir))) {
		if (g_str_has_suffix (d->d_name, ".gif") ||
		    g_str_has_suffix (d->d_name, ".png"))
			files = g_list_prepend (files, g_strdup (d->d_name));
	}

	closedir (dir);

	selected = g_random_int_range (0, g_list_length (files));

	text = g_strdup_printf ("%s/%s", directory,
				(char *)g_list_nth_data (files, selected));
	g_free (directory);

	pixbuf = gdk_pixbuf_new_from_file (text, &error);
	g_free (text);

	for (i = files; i; i = i->next)
		g_free (i->data);
	g_list_free (files);

	if (error) {
		char *message;

		message = g_strdup_printf (_("Unable to load header image: %s"),
					   error->message);
		show_error_dialog (message);

		g_free (message);
		g_error_free (error);

		return NULL;
	}

	return pixbuf;
}

static GdkPixbuf *
load_logo (void)
{
	gchar *file;
	GdkPixbuf *pixbuf;
	GError *error = NULL;

	file = gnome_program_locate_file (NULL,
					  GNOME_FILE_DOMAIN_APP_DATADIR,
					  "gnome-64.png",
					  TRUE, NULL);
	if (!file) {
		show_error_dialog (_("Could not locate the GNOME logo."));

		return NULL;
	}

	pixbuf = gdk_pixbuf_new_from_file (file, &error);
	if (error) {
		char *message;

		message = g_strdup_printf (_("Unable to load '%s': %s"),
					   file,
					   error->message);
		g_free (file);
		show_error_dialog (message);

		g_free (message);
		g_error_free (error);

		return NULL;
	}

	g_free (file);

	return pixbuf;
}

/* href item impl */
typedef struct {
	GnomeCanvasItem *item;

	const gchar *text;
	const gchar *url;
} HRefItem;

static gboolean
href_item_event_callback (GnomeCanvasItem *item,
			  GdkEvent        *event,
			  gpointer         user_data)
{
	HRefItem *href = (HRefItem *) user_data;

	switch (event->type) {
	case GDK_ENTER_NOTIFY: {
		GdkCursor *cursor;

		cursor = gdk_cursor_new (GDK_HAND2);

		gdk_window_set_cursor (GTK_WIDGET (item->canvas)->window, cursor);
		gdk_cursor_unref (cursor);
	}
		break;
	case GDK_LEAVE_NOTIFY:
		gdk_window_set_cursor (GTK_WIDGET (item->canvas)->window, NULL);
		break;

	case GDK_BUTTON_PRESS: {
		GError *error= NULL;

		gnome_url_show (href->url, &error);
		if (error) {
			char *message;

			message = g_strdup_printf (_("Could not open the address \"%s\": %s"),
						   href->url, error->message);
			show_error_dialog (message);

			g_free (message);
			g_error_free (error);
		}

		return TRUE;
	}

	default:
		break;
	}

	return FALSE;
}

static HRefItem *
href_item_new (GnomeCanvasGroup *group,
	       const gchar      *text,
	       const gchar      *url,
	       gdouble          *current_x,
	       gdouble          *current_y)
{
	HRefItem *item;
	gdouble tmp;

	item = g_new0 (HRefItem, 1);
	item->text = g_strdup (text);
	item->url = g_strdup (url);

	item->item =
		gnome_canvas_item_new (group,
				       gnome_canvas_text_get_type (),
				       "text", text,
				       "anchor", GTK_ANCHOR_NW,
				       "x", *current_x,
				       "y", *current_y,
				       "underline", PANGO_UNDERLINE_SINGLE,
				       "weight", PANGO_WEIGHT_BOLD,
				       "fill_color", "#000000",
				       NULL);
	g_signal_connect (item->item, "event",
			  G_CALLBACK (href_item_event_callback), item);

	g_object_get (item->item, "text_width", &tmp, NULL);
	*current_x += tmp + 5.0;

	return item;
}

static GnomeCanvasItem *
create_dot (GnomeCanvasGroup *group,
	    gdouble          *current_x,
	    gdouble          *current_y,
	    gdouble           dot_delta)
{
	GnomeCanvasItem *item;
	gdouble tmp;

	item = gnome_canvas_item_new (group,
				      gnome_canvas_text_get_type (),
				      "text", ".",
				      "anchor", GTK_ANCHOR_NW,
				      "x", *current_x,
				      "y", *current_y - dot_delta,
				      "weight", PANGO_WEIGHT_BOLD,
				      "fill_color", "#000000",
				      NULL);

	g_object_get (item, "text_width", &tmp, NULL);
	*current_x += tmp + 5.0;

	return item;
}

/* the canvas */
static char *
strip_newlines (const char *str)
{
	char **strv;
	char *tmp;

	if (!str)
		return NULL;

	strv = g_strsplit (str, "\n", -1);
	tmp = g_strjoinv (" ", strv);
	g_strfreev (strv);
	tmp = g_strchug (tmp);

	return tmp;
}

/* FIXME: This could possibly be done more smoothly. The locale matching stuff
 * needs to be abstracted out, since it must be useful in a number of places. */

/* The language selection stuff here is not blindingly efficient, but it is
 * only done once for a small number of paragraphs, so I (Malcolm) think we can
 * just pay the penalty rather than going for something more complex.
 *
 * The problem we are solving here is, for each paragraph, find the translated
 * version that matches the highest preference possible from amongst the user's
 * LC_MESSAGES settings. Note that the translated paragraphs may not appear in
 * the same order as the LC_MESSAGE preferences, so a certain amount of messing
 * around is required (in particular, we cannot quit after finding the first
 * match).
 */

/* FIXME: this code is atrocious */

static void
get_description_messages (xmlNodePtr node)
{
	xmlNodePtr paras;
	GSList *list = NULL, *l;
	const GList *langs = NULL;
	gint i;
	gboolean started = FALSE;
	char *best_value = NULL;

	langs = gnome_i18n_get_language_list ("LC_MESSAGES");
	paras = node->children;

	while (paras) {
		while (paras) {
			char *value = (char *)xmlNodeGetContent (paras);
			char *tmp;
			xmlChar *cur, *best = NULL;
			const GList *t;

			if (paras->type == XML_ELEMENT_NODE &&
					xmlStrEqual (paras->name, "p") &&
					value && value[0]) {
				cur = xmlNodeGetLang (paras);

				tmp = strip_newlines (value);

				if (!started) {
					started = TRUE;
					if (!cur) {
						best = xmlCharStrdup ("C");
					}
					else {
						best = xmlStrdup (cur);
					}
					best_value = g_strdup (tmp);
				}
				else {
					if (!cur || xmlStrEqual (cur, "C")) {
						break;
					}
					/* See if the current lanaguage occurs
					 * earlier than the previous best. */
					for (t = langs; t; t = t->next) {
						if (xmlStrEqual (t->data,
								best)) {
							break;
						}
						else if (xmlStrEqual (t->data,
									cur)) {
							xmlFree (best);
							best = xmlStrdup (cur);
							g_free (best_value);
							best_value = g_strdup (tmp);
							/* If this is the first
							 * language on the
							 * list of choices,
							 * stop. We are not
							 * going to go any
							 * better. */
							if (t == langs) {
								break;
							}
						}
					}
				}
				g_free (tmp);
				xmlFree (cur);
				xmlFree (best);
			}

			paras = paras->next;
			xmlFree (value);
		}
		list = g_slist_prepend (list, best_value);
		started = FALSE;
	}

	list = g_slist_reverse (list);

	introduction_messages = g_new (char *, g_slist_length (list) + 1);

	for (i = 0, l = list; l; l = l->next, i++)
		introduction_messages[i] = l->data;

	introduction_messages[i] = NULL;

	g_slist_free (list);
}

static char*
create_date_string (const char *value)
{
	char  *result;
	GDate *date;
	int    day;
	int    month;
	int    year;

	/* YYYY-MM-DD */
	if (sscanf (value, "%d-%d-%d", &year, &month, &day) < 3)
		return NULL;

	date = g_date_new ();

	g_date_set_dmy (date, day, month, year);

	result = g_new0 (char, 24);
	g_date_strftime (result, 24, "%x", date);

	g_date_free (date);

	return result;
}

static void
display_version_info (GnomeCanvasGroup *group)
{
	xmlDocPtr about;
	xmlNodePtr node;
	xmlNodePtr bits;

	gchar *file;

	char *platform = NULL;
	char *minor = NULL;
	char *micro = NULL;
	char *version_string = NULL;
	char *distributor_string = NULL;
	char *build_date_string = NULL;
	char *text = NULL;

	GnomeCanvasItem *info;
	gdouble height = 0.0;

	file = gnome_program_locate_file (NULL,
					  GNOME_FILE_DOMAIN_APP_DATADIR,
					  "gnome-version.xml",
					  TRUE, NULL);
	if (!file) {
		show_error_dialog (_("Could not locate the file with GNOME version information."));
		return;
	}

	about = xmlParseFile (file);
	g_free (file);

	if (!about)
		return;

	node = about->children;

	if (g_ascii_strcasecmp (node->name, "gnome-version")) {
		xmlFreeDoc (about);
		return;
	}

	bits = node->children;

	while (bits) {
		char *name = (char *)bits->name;
		char *value;

		if (!g_ascii_strcasecmp (name, "description"))
			get_description_messages (bits);

		value = (char *)xmlNodeGetContent (bits);

		if (!g_ascii_strcasecmp (name, "platform")
		    && value && value[0])
			platform = g_strdup (value);
		if (!g_ascii_strcasecmp (name, "minor") && value && value[0])
			minor = g_strdup (value);
		if (!g_ascii_strcasecmp (name, "micro") && value && value[0])
			micro = g_strdup (value);
		if (!g_ascii_strcasecmp (name, "distributor") && value && value[0])
			distributor_string = g_strdup (value);
		if (!g_ascii_strcasecmp (name, "date") && value && value[0])
			build_date_string = create_date_string (value);

		bits = bits->next;
		xmlFree (value);
	}

	xmlFreeDoc (about);

	if (!minor)
		version_string = g_strconcat (platform, NULL);

	if (!version_string && !micro)
		version_string = g_strconcat (platform, ".", minor, NULL);

	if (!version_string)
		version_string = g_strconcat (platform, ".", minor, ".",
					      micro, NULL);

	g_free (platform);
	g_free (minor);
	g_free (micro);

	info = gnome_canvas_item_new (group,
				      gnome_canvas_group_get_type (),
				      "x", 10.0,
				      NULL);

	if (version_string && version_string[0]) {
		gdouble tmp;
		GnomeCanvasItem *item;

		text = g_strdup_printf ("<b>%s: </b>%s",
					_("Version"), version_string);
		item = gnome_canvas_item_new (GNOME_CANVAS_GROUP (info),
					      gnome_canvas_text_get_type (),
					      "markup", text,
					      "anchor", GTK_ANCHOR_NW,
					      "x", 0.0,
					      "y", height,
					      NULL);
		g_free (text);

		g_object_get (item, "text_height", &tmp, NULL);
		height += tmp + 4.0;
	}

	if (distributor_string && distributor_string[0]) {
		gdouble tmp;
		GnomeCanvasItem *item;

		text = g_strdup_printf ("<b>%s: </b>%s",
					_("Distributor"), distributor_string);
		item = gnome_canvas_item_new (GNOME_CANVAS_GROUP (info),
					      gnome_canvas_text_get_type (),
					      "markup", text,
					      "anchor", GTK_ANCHOR_NW,
					      "x", 0.0,
					      "y", height,
					      NULL);
		g_free (text);

		g_object_get (item, "text_height", &tmp, NULL);
		height += tmp + 4.0;
	}

	if (build_date_string && build_date_string[0]) {
		gdouble tmp;
		GnomeCanvasItem *item;

		text = g_strdup_printf ("<b>%s: </b>%s",
					_("Build Date"), build_date_string);
		item = gnome_canvas_item_new (GNOME_CANVAS_GROUP (info),
					      gnome_canvas_text_get_type (),
					      "markup", text,
					      "anchor", GTK_ANCHOR_NW,
					      "x", 0.0,
					      "y", height,
					      NULL);
		g_free (text);

		g_object_get (item, "text_height", &tmp, NULL);
		height += tmp + 4.0;
	}

	g_free (version_string);
	g_free (distributor_string);
	g_free (build_date_string);

	gnome_canvas_item_set (info, "y", canvas_height - height, NULL);
	version_info_height = height;
}

static GtkWidget *
create_canvas (void)
{
	GdkColor color = {0, 0xffff, 0xffff, 0xffff};
	GtkWidget *canvas;

	HRefItem *href;
	gchar *text;

	GnomeCanvasItem *item;
	GnomeCanvasGroup *root;
	GnomeCanvasPoints *points;

	GdkPixbuf *header;
	GdkPixbuf *logo;

	gdouble current_x;
	gdouble current_y;
	gdouble dot_delta;
	gdouble tmp;

	/* set up a canvas */
	canvas = gnome_canvas_new ();

	gnome_canvas_set_scroll_region (GNOME_CANVAS (canvas), 0, 0,
					canvas_width, canvas_height);
	gtk_widget_set_size_request (canvas, canvas_width, canvas_height);

	gdk_colormap_alloc_color (gtk_widget_get_colormap (GTK_WIDGET (canvas)),
				  &color, TRUE, TRUE);

	/* euhm? */
	gtk_widget_modify_bg (GTK_WIDGET (canvas), GTK_STATE_NORMAL, &color);

	root = GNOME_CANVAS_GROUP (GNOME_CANVAS (canvas)->root);

	/* the header */
	header = load_random_header ();
	if (!header)
		/* emergency stop complete with leaks */
		return NULL;

	item = gnome_canvas_item_new (root,
				      gnome_canvas_pixbuf_get_type (),
				      "x", 0.0,
				      "y", 0.0,
				      "pixbuf", header,
				      NULL);

	/* load logo in advance, we need it's size */
	logo = load_logo ();
	if (!logo)
		/* emergency stop complete with leaks */
		return NULL;

	/* and a coulple o' links */
	current_x = 10.0 + (gdouble)gdk_pixbuf_get_width (logo) + 10.0;
	current_y = (gdouble)gdk_pixbuf_get_height (header) + 5.0;

	href = href_item_new (root,
			      _("About GNOME"),
			      "http://www.gnome.org/about/",
			      &current_x, &current_y);

	/* make a nice guess for the dot delta */
	g_object_get (href->item, "text_height", &tmp, NULL);
	dot_delta = tmp / 4.5;

	/* draw a dot */
	item = create_dot (root, &current_x, &current_y, dot_delta);

	/* and more items on a likewise way.
	 */
	href = href_item_new (root,
			      _("News"),
			      "http://news.gnome.org",
			      &current_x, &current_y);
	item = create_dot (root, &current_x, &current_y, dot_delta);

	/*
	 * FIXME: this used to be 'users' and it would be great to make it
	 * users again once there is a user-centric page on the we site.
	 */
	href = href_item_new (root,
			      _("Software"),
			      "http://www.gnome.org/softwaremap",
			      &current_x, &current_y);
	item = create_dot (root, &current_x, &current_y, dot_delta);

	href = href_item_new (root,
			      _("Developers"),
			      "http://developer.gnome.org/",
			      &current_x, &current_y);
	item = create_dot (root, &current_x, &current_y, dot_delta);

	href = href_item_new (root,
			      _("Friends of GNOME"),
			      "http://www.gnome.org/friends/",
			      &current_x, &current_y);
	item = create_dot (root, &current_x, &current_y, dot_delta);

	href = href_item_new (root,
			      _("Contact"),
			      "http://www.gnome.org/contact/",
			      &current_x, &current_y);

	/* resize */
	canvas_width = current_x;
	gnome_canvas_set_scroll_region (GNOME_CANVAS (canvas), 0, 0,
					canvas_width, canvas_height);
	gtk_widget_set_size_request (GTK_WIDGET (canvas),
				     canvas_width, canvas_height);

	/* and a nice black stripe */
	points = gnome_canvas_points_new (2);
	points->coords[0] = 0.0;
	points->coords[1] = gdk_pixbuf_get_height (header);
	points->coords[2] = current_x;
	points->coords[3] = gdk_pixbuf_get_height (header);

	item = gnome_canvas_item_new (root,
				      gnome_canvas_line_get_type (),
				      "points", points,
				      "fill_color", "#666666",
				      "width_pixels", 1,
				      NULL);

	gnome_canvas_points_free (points);

	/* the gnome logo */
	item = gnome_canvas_item_new (root,
				      gnome_canvas_pixbuf_get_type (),
				      "x", 10.0,
				      "y", 10.0,
				      "pixbuf", logo,
				      NULL);

	/* and some introduction text */
	text = g_strdup_printf ("<big><big><b>%s</b></big></big>",
				_("Welcome to the GNOME Desktop"));
	item = gnome_canvas_item_new (root,
				      gnome_canvas_text_get_type (),
				      "markup", text,
				      "anchor", GTK_ANCHOR_NW,
				      "y", current_y + 25.0,
				      "fill_color", "#000000",
				      NULL);
	g_free (text);

	g_object_get (item, "text_width", &tmp, NULL);
	gnome_canvas_item_set (item,
			       "x", (canvas_width - tmp) / 2.0,
			       NULL);


	text = g_strdup_printf ("<big><b>%s</b></big>",
				_("Brought to you by:"));
	item = gnome_canvas_item_new (root,
				      gnome_canvas_text_get_type (),
				      "markup", text,
				      "anchor", GTK_ANCHOR_NW,
				      "y", current_y + 55.0,
				      NULL);
	subheader = item;
	gnome_canvas_item_hide (item);
	g_free (text);

	g_object_get (item, "text_width", &tmp, NULL);
	gnome_canvas_item_set (item,
			       "x", (canvas_width - tmp) / 2.0,
			       NULL);

	/* and the version info */
	display_version_info (root);

	/* pfff done */
	return canvas;
}

/* the dialog */
static gboolean
quit_callback (GtkWidget *widget,
	       gpointer   user_data)
{
	gtk_main_quit ();

	return FALSE;
}

static void
response_callback (GtkDialog *dialog,
		   int        reponse_id,
		   gpointer   user_data)
{
	if (reponse_id == GTK_RESPONSE_CLOSE)
		quit_callback (GTK_WIDGET (dialog), NULL);
}

static GtkWidget *
create_about_dialog (void)
{
	GtkWidget *dialog;
	GtkWidget *canvas;

	dialog = gtk_dialog_new_with_buttons (_("About the GNOME Desktop"),
					      NULL, 0,
					      GTK_STOCK_CLOSE,
					      GTK_RESPONSE_CLOSE,
					      NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog),
					 GTK_RESPONSE_CLOSE);

	g_signal_connect (dialog, "delete_event",
			  G_CALLBACK (quit_callback), NULL);
	g_signal_connect (dialog, "response",
			  G_CALLBACK (response_callback), NULL);

	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
	g_object_set (dialog,
		      "allow_shrink", FALSE,
		      "allow_grow", FALSE,
		      NULL);

	canvas = create_canvas ();
	if (!canvas) {
		gtk_widget_destroy (dialog);
		return NULL;
	}

	/* start animations once the canvas has been mapped */
	g_signal_connect (canvas, "map",
			  G_CALLBACK (start_animations), NULL);

	gtk_container_set_border_width
		(GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    canvas, TRUE, TRUE, 0);

	return dialog;
}

static void
generate_randomness (void)
{
	gint i;
	gint random_number;
	gint tmp;
	gint num_contributors;
	GRand *generator;

	generator = g_rand_new ();

	num_contributors = G_N_ELEMENTS (contributors);

	contrib_order = g_malloc (num_contributors * sizeof (gint));

	for (i = 0; i < num_contributors; i++) {
		contrib_order[i]=i;
	}

	for (i = 0; i < num_contributors; i++) {
		random_number = g_rand_int_range (generator, i, 
						  num_contributors);
		tmp = contrib_order[i];
		contrib_order[i] = contrib_order[random_number];
		contrib_order[random_number] = tmp;
	}

	g_rand_free (generator);
}
		

/* main */
int
main (int argc, char **argv)
{
	GnomeProgram *program;
	GtkWidget *dialog;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	program = gnome_program_init ("gnome-about", VERSION,
				      LIBGNOMEUI_MODULE,
				      argc, argv,
				      GNOME_PARAM_APP_DATADIR, DATADIR,
				      NULL);
	gnome_window_icon_set_default_from_file (GNOME_ICONDIR
			"/gnome-logo-icon-transparent.png");

	dialog = create_about_dialog ();
	if (!dialog)
		return -1;

	generate_randomness ();

	gtk_widget_show_all (dialog);

	gtk_main ();

	g_object_unref (program);

	return 0;
}
