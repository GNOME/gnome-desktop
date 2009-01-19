/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

gnomebg.c: Object for the desktop background.

Copyright (C) 2000 Eazel, Inc.
Copyright (C) 2007-2008 Red Hat, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this program; if not, write to the
Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.

Derived from eel-background.c and eel-gdk-pixbuf-extensions.c by
Darin Adler <darin@eazel.com> and Ramiro Estrugo <ramiro@eazel.com>

Author: Soren Sandmann <sandmann@redhat.com>

*/

#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>

#include <gio/gio.h>

#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <gconf/gconf-client.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnomeui/gnome-bg.h>

#define BG_KEY_DRAW_BACKGROUND    GNOME_BG_KEY_DIR "/draw_background"
#define BG_KEY_PRIMARY_COLOR      GNOME_BG_KEY_DIR "/primary_color"
#define BG_KEY_SECONDARY_COLOR    GNOME_BG_KEY_DIR "/secondary_color"
#define BG_KEY_COLOR_TYPE         GNOME_BG_KEY_DIR "/color_shading_type"
#define BG_KEY_PICTURE_PLACEMENT  GNOME_BG_KEY_DIR "/picture_options"
#define BG_KEY_PICTURE_OPACITY    GNOME_BG_KEY_DIR "/picture_opacity"
#define BG_KEY_PICTURE_FILENAME   GNOME_BG_KEY_DIR "/picture_filename"

typedef struct _SlideShow SlideShow;
typedef struct _Slide Slide;

struct _Slide
{
	double   duration;		/* in seconds */
	gboolean fixed;

	GSList  *file1;
	GSList  *file2;		/* NULL if fixed is TRUE */
};

typedef struct _FileSize FileSize;
struct _FileSize
{
	gint width;
	gint height;

	char *file;
};

/* This is the size of the GdkRGB dither matrix, in order to avoid
 * bad dithering when tiling the gradient
 */
#define GRADIENT_PIXMAP_TILE_SIZE 128

typedef struct FileCacheEntry FileCacheEntry;
#define CACHE_SIZE 4

/*
 *   Implementation of the GnomeBG class
 */
struct _GnomeBG
{
	GObject                 parent_instance;
	char *			filename;
	GnomeBGPlacement	placement;
	GnomeBGColorType	color_type;
	GdkColor		primary;
	GdkColor		secondary;

 	gint                    last_pixmap_width;
 	gint                    last_pixmap_height;

	GFileMonitor *		file_monitor;

	guint                   changed_id;
	guint                   transitioned_id;

	/* Cached information, only access through cache accessor functions */
        SlideShow *		slideshow;
	time_t			file_mtime;
	GdkPixbuf *		pixbuf_cache;
	int			timeout_id;

	GList *		        file_cache;
};

struct _GnomeBGClass
{
	GObjectClass parent_class;
};

enum {
	CHANGED,
	TRANSITIONED,
	N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE (GnomeBG, gnome_bg, G_TYPE_OBJECT)

static GdkPixmap *make_root_pixmap     (GdkScreen  *screen,
					gint        width,
					gint        height);

/* Pixbuf utils */
static guint32    pixbuf_average_value (GdkPixbuf  *pixbuf);
static GdkPixbuf *pixbuf_scale_to_fit  (GdkPixbuf  *src,
					int         max_width,
					int         max_height);
static GdkPixbuf *pixbuf_scale_to_min  (GdkPixbuf  *src,
					int         min_width,
					int         min_height);
static void       pixbuf_draw_gradient (GdkPixbuf  *pixbuf,
					gboolean    horizontal,
					GdkColor   *c1,
					GdkColor   *c2);
static void       pixbuf_tile          (GdkPixbuf  *src,
					GdkPixbuf  *dest);
static void       pixbuf_blend         (GdkPixbuf  *src,
					GdkPixbuf  *dest,
					int         src_x,
					int         src_y,
					int         width,
					int         height,
					int         dest_x,
					int         dest_y,
					double      alpha);

/* Thumbnail utilities */
static GdkPixbuf *create_thumbnail_for_filename (GnomeDesktopThumbnailFactory *factory,
						 const char            *filename);
static gboolean   get_thumb_annotations (GdkPixbuf             *thumb,
					 int                   *orig_width,
					 int                   *orig_height);

/* Cache */
static GdkPixbuf *get_pixbuf           (GnomeBG               *bg);
static void       clear_cache          (GnomeBG               *bg);
static gboolean   is_different         (GnomeBG               *bg,
					const char            *filename);
static time_t     get_mtime            (const char            *filename);
static GdkPixbuf *create_img_thumbnail (GnomeBG               *bg,
					GnomeDesktopThumbnailFactory *factory,
					GdkScreen             *screen,
					int                    dest_width,
					int                    dest_height);
static SlideShow * get_as_slideshow    (GnomeBG               *bg,
					const char 	      *filename);
static Slide *     get_current_slide   (SlideShow 	      *show,
		   			double    	      *alpha);
static gboolean    slideshow_changes_with_size (SlideShow *show);

static void
color_from_string (const char *string,
		   GdkColor   *colorp)
{
	/* If all else fails use black */
	gdk_color_parse ("black", colorp);

	if (!string)
		return;

	if (!gdk_color_parse (string, colorp))
		return;

	gdk_rgb_find_color (gdk_rgb_get_colormap(), colorp);
}

static char *
color_to_string (const GdkColor *color)
{
	return g_strdup_printf ("#%02x%02x%02x",
				color->red >> 8,
				color->green >> 8,
				color->blue >> 8);
}

static GConfEnumStringPair placement_lookup[] = {
	{ GNOME_BG_PLACEMENT_CENTERED,    "centered" },
	{ GNOME_BG_PLACEMENT_FILL_SCREEN, "stretched" },
	{ GNOME_BG_PLACEMENT_SCALED,      "scaled" },
	{ GNOME_BG_PLACEMENT_ZOOMED,      "zoom" },
	{ GNOME_BG_PLACEMENT_TILED,       "wallpaper" },
	{ 0, NULL }
};

static GConfEnumStringPair color_type_lookup[] = {
	{ GNOME_BG_COLOR_SOLID,      "solid" },
	{ GNOME_BG_COLOR_H_GRADIENT, "horizontal-gradient" },
	{ GNOME_BG_COLOR_V_GRADIENT, "vertical-gradient" },
	{ 0, NULL }
};

static void
color_type_from_string (const char       *string,
			GnomeBGColorType *color_type)
{
	*color_type = GNOME_BG_COLOR_SOLID;

	if (string) {
		gconf_string_to_enum (color_type_lookup,
				      string, (int *)color_type);
	}
}

static const char *
color_type_to_string (GnomeBGColorType color_type)
{
	return gconf_enum_to_string (color_type_lookup, color_type);
}

static void
placement_from_string (const char       *string,
		       GnomeBGPlacement *placement)
{
	*placement = GNOME_BG_PLACEMENT_ZOOMED;

	if (string) {
		gconf_string_to_enum (placement_lookup,
				      string, (int *)placement);
	}
}

static const char *
placement_to_string (GnomeBGPlacement placement)
{
	return gconf_enum_to_string (placement_lookup, placement);
}

static gboolean
do_changed (GnomeBG *bg)
{
	bg->changed_id = 0;

	g_signal_emit (G_OBJECT (bg), signals[CHANGED], 0);

	return FALSE;
}

static void
queue_changed (GnomeBG *bg)
{
	if (bg->changed_id > 0) {
		g_source_remove (bg->changed_id);
	}

	bg->changed_id = g_timeout_add_full (G_PRIORITY_LOW,
					     100,
					     (GSourceFunc)do_changed,
					     bg,
					     NULL);
}

static gboolean
do_transitioned (GnomeBG *bg)
{
	bg->transitioned_id = 0;

	g_signal_emit (G_OBJECT (bg), signals[TRANSITIONED], 0);

	return FALSE;
}

static void
queue_transitioned (GnomeBG *bg)
{
	if (bg->transitioned_id > 0) {
		g_source_remove (bg->transitioned_id);
	}

	bg->transitioned_id = g_timeout_add_full (G_PRIORITY_LOW,
					     100,
					     (GSourceFunc)do_transitioned,
					     bg,
					     NULL);
}

void
gnome_bg_load_from_preferences (GnomeBG     *bg,
				GConfClient *client)
{
	char    *tmp;
	char    *filename;
	GnomeBGColorType ctype;
	GdkColor c1, c2;
	GnomeBGPlacement placement;

	g_return_if_fail (GNOME_IS_BG (bg));
	g_return_if_fail (client != NULL);

	/* Filename */
	filename = NULL;
	tmp = gconf_client_get_string (client, BG_KEY_PICTURE_FILENAME, NULL);
	if (tmp != NULL && *tmp != '\0') {
		if (g_utf8_validate (tmp, -1, NULL) &&
		    g_file_test (tmp, G_FILE_TEST_EXISTS)) {
			filename = g_strdup (tmp);
		} else {
			filename = g_filename_from_utf8 (tmp, -1, NULL, NULL, NULL);
		}

		/* Fall back to default background if filename was set
		   but no longer exists */
		if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
			GConfValue *default_value;

			g_free (filename);
			filename = NULL;

			default_value =
				gconf_client_get_default_from_schema (client,
								      BG_KEY_PICTURE_FILENAME,
								      NULL);
			if (default_value != NULL) {
				filename = g_strdup (gconf_value_get_string (default_value));
				gconf_value_free (default_value);
			}
		}
	}
	g_free (tmp);

	/* Colors */
	tmp = gconf_client_get_string (client, BG_KEY_PRIMARY_COLOR, NULL);
	color_from_string (tmp, &c1);
	g_free (tmp);

	tmp = gconf_client_get_string (client, BG_KEY_SECONDARY_COLOR, NULL);
	color_from_string (tmp, &c2);
	g_free (tmp);

	/* Color type */
	tmp = gconf_client_get_string (client, BG_KEY_COLOR_TYPE, NULL);
	color_type_from_string (tmp, &ctype);
	g_free (tmp);

	/* Placement */
	tmp = gconf_client_get_string (client, BG_KEY_PICTURE_PLACEMENT, NULL);
	placement_from_string (tmp, &placement);
	g_free (tmp);

	gnome_bg_set_color (bg, ctype, &c1, &c2);
	gnome_bg_set_placement (bg, placement);
	gnome_bg_set_filename (bg, filename);

	g_free (filename);
}

void
gnome_bg_save_to_preferences (GnomeBG     *bg,
			      GConfClient *client)
{
	const char *color_type;
	const char *placement;
        const gchar *filename;
        gchar *primary;
        gchar *secondary;
        
	primary = color_to_string (&bg->primary);
	secondary = color_to_string (&bg->secondary);

	color_type = color_type_to_string (bg->color_type);

        if (bg->filename) {
		filename = bg->filename;
		placement = placement_to_string (bg->placement);
        }
        else {
                filename = "(none)";
                placement = "none";
        }

	gconf_client_set_string (client, BG_KEY_PICTURE_FILENAME, filename, NULL);
        gconf_client_set_string (client, BG_KEY_PRIMARY_COLOR, primary, NULL);
        gconf_client_set_string (client, BG_KEY_SECONDARY_COLOR, secondary, NULL);
        gconf_client_set_string (client, BG_KEY_COLOR_TYPE, color_type, NULL);
	gconf_client_set_string (client, BG_KEY_PICTURE_PLACEMENT, placement, NULL);

	g_free (primary);
	g_free (secondary);
}


static void
gnome_bg_init (GnomeBG *bg)
{
}

static void
gnome_bg_finalize (GObject *object)
{
	GnomeBG *bg = GNOME_BG (object);

	if (bg->changed_id != 0) {
		g_source_remove (bg->changed_id);
		bg->changed_id = 0;
	}

	if (bg->filename) {
		g_free (bg->filename);
		bg->filename = NULL;
	}

	clear_cache (bg);

	G_OBJECT_CLASS (gnome_bg_parent_class)->finalize (object);
}

static void
gnome_bg_class_init (GnomeBGClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gnome_bg_finalize;

	signals[CHANGED] = g_signal_new ("changed",
					 G_OBJECT_CLASS_TYPE (object_class),
					 G_SIGNAL_RUN_LAST,
					 0,
					 NULL, NULL,
					 g_cclosure_marshal_VOID__VOID,
					 G_TYPE_NONE, 0);

	signals[TRANSITIONED] = g_signal_new ("transitioned",
					 G_OBJECT_CLASS_TYPE (object_class),
					 G_SIGNAL_RUN_LAST,
					 0,
					 NULL, NULL,
					 g_cclosure_marshal_VOID__VOID,
					 G_TYPE_NONE, 0);
}

GnomeBG *
gnome_bg_new (void)
{
	return g_object_new (GNOME_TYPE_BG, NULL);
}

void
gnome_bg_set_color (GnomeBG *bg,
		    GnomeBGColorType type,
		    GdkColor *primary,
		    GdkColor *secondary)
{
	g_return_if_fail (bg != NULL);

	if (bg->color_type != type			||
	    !gdk_color_equal (&bg->primary, primary)			||
	    (secondary && !gdk_color_equal (&bg->secondary, secondary))) {

		bg->color_type = type;
		bg->primary = *primary;
		if (secondary) {
			bg->secondary = *secondary;
		}

		queue_changed (bg);
	}
}

void
gnome_bg_set_placement (GnomeBG          *bg,
			GnomeBGPlacement  placement)
{
	g_return_if_fail (bg != NULL);
	
	if (bg->placement != placement) {
		bg->placement = placement;
		
		queue_changed (bg);
	}
}

GnomeBGPlacement
gnome_bg_get_placement (GnomeBG *bg)
{
	g_return_val_if_fail (bg != NULL, -1);

	return bg->placement;
}

void
gnome_bg_get_color (GnomeBG               *bg,
		    GnomeBGColorType      *type,
		    GdkColor              *primary,
		    GdkColor              *secondary)
{
	g_return_if_fail (bg != NULL);

	if (type)
		*type = bg->color_type;

	if (primary)
		*primary = bg->primary;

	if (secondary)
		*secondary = bg->secondary;
}

const gchar *
gnome_bg_get_filename (GnomeBG *bg)
{
	g_return_val_if_fail (bg != NULL, NULL);

	return bg->filename;
}

static void
file_changed (GFileMonitor *file_monitor,
	      GFile *child,
	      GFile *other_file,
	      GFileMonitorEvent event_type,
	      gpointer user_data)
{
	GnomeBG *bg = GNOME_BG (user_data);

	clear_cache (bg);
	queue_changed (bg);
}

void
gnome_bg_set_filename (GnomeBG     *bg,
		       const char  *filename)
{
	g_return_if_fail (bg != NULL);
	
	if (is_different (bg, filename)) {
		char *tmp = g_strdup (filename);
		
		g_free (bg->filename);
		
		bg->filename = tmp;
		bg->file_mtime = get_mtime (bg->filename);

		if (bg->file_monitor) {
			g_object_unref (bg->file_monitor);
			bg->file_monitor = NULL;
		}

		if (bg->filename) {
			GFile *f = g_file_new_for_path (bg->filename);
			
			bg->file_monitor = g_file_monitor_file (f, 0, NULL, NULL);
			g_signal_connect (bg->file_monitor, "changed",
					  G_CALLBACK (file_changed), bg);

			g_object_unref (f);
		}
		
		clear_cache (bg);
		
		queue_changed (bg);
	}
}

static void
draw_color (GnomeBG *bg, GdkPixbuf *dest)
{
	guint32 pixel;
	
	switch (bg->color_type)
	{
	case GNOME_BG_COLOR_SOLID:
		pixel = ((bg->primary.red >> 8) << 24)      |
			((bg->primary.green >> 8) << 16)    |
			((bg->primary.blue >> 8) << 8)      |
			(0xff);
		
		gdk_pixbuf_fill (dest, pixel);
		break;
		
	case GNOME_BG_COLOR_H_GRADIENT:
		pixbuf_draw_gradient (dest, TRUE, &(bg->primary), &(bg->secondary));
		break;
		
	case GNOME_BG_COLOR_V_GRADIENT:
		pixbuf_draw_gradient (dest, FALSE, &(bg->primary), &(bg->secondary));
		break;
		
	default:
		break;
	}
}

static GdkPixbuf *
get_scaled_pixbuf (GnomeBGPlacement placement,
		   GdkPixbuf *pixbuf,
		   int width, int height,
		   int *x, int *y, int *w, int *h)
{
	GdkPixbuf *new;
	
#if 0
	g_print ("original_width: %d %d\n",
		 gdk_pixbuf_get_width (pixbuf),
		 gdk_pixbuf_get_height (pixbuf));
#endif
	
	switch (placement) {
	case GNOME_BG_PLACEMENT_ZOOMED:
		new = pixbuf_scale_to_min (pixbuf, width, height);
		break;
		
	case GNOME_BG_PLACEMENT_FILL_SCREEN:
		new = gdk_pixbuf_scale_simple (pixbuf, width, height,
					       GDK_INTERP_BILINEAR);
		break;
		
	case GNOME_BG_PLACEMENT_SCALED:
		new = pixbuf_scale_to_fit (pixbuf, width, height);
		break;
		
	case GNOME_BG_PLACEMENT_CENTERED:
	case GNOME_BG_PLACEMENT_TILED:
	default:
		new = g_object_ref (pixbuf);
		break;
	}
	
	*w = gdk_pixbuf_get_width (new);
	*h = gdk_pixbuf_get_height (new);
	*x = (width - *w) / 2;
	*y = (height - *h) / 2;
	
	return new;
}

static void
draw_image (GnomeBGPlacement  placement,
	    GdkPixbuf        *pixbuf,
	    GdkPixbuf        *dest)
{
	int dest_width = gdk_pixbuf_get_width (dest);
	int dest_height = gdk_pixbuf_get_height (dest);
	int x, y, w, h;
	GdkPixbuf *scaled;
	
	if (!pixbuf)
		return;
	
	scaled = get_scaled_pixbuf (
		placement, pixbuf, dest_width, dest_height, &x, &y, &w, &h);

	switch (placement) {
	case GNOME_BG_PLACEMENT_TILED:
		pixbuf_tile (scaled, dest);
		break;
	case GNOME_BG_PLACEMENT_ZOOMED:
	case GNOME_BG_PLACEMENT_CENTERED:
	case GNOME_BG_PLACEMENT_FILL_SCREEN:
	case GNOME_BG_PLACEMENT_SCALED:
		pixbuf_blend (scaled, dest, 0, 0, w, h, x, y, 1.0);
		break;
	default:
		g_assert_not_reached ();
		break;
	}
	
	g_object_unref (scaled);
}

void
gnome_bg_draw (GnomeBG *bg, GdkPixbuf *dest)
{
	if (!bg)
		return;
	
	draw_color (bg, dest);
	
	draw_image (bg->placement, get_pixbuf (bg), dest);
}

gboolean
gnome_bg_changes_with_size (GnomeBG *bg)
{
	SlideShow *show;

	g_return_val_if_fail (bg != NULL, FALSE);

	show = get_as_slideshow (bg, bg->filename);
	if (show) 
		return slideshow_changes_with_size (show);
	
	if (bg->color_type != GNOME_BG_COLOR_SOLID) {
		if (!get_pixbuf (bg))
			return TRUE;
		if (gdk_pixbuf_get_has_alpha (get_pixbuf (bg)))
			return TRUE;
		if (bg->placement == GNOME_BG_PLACEMENT_CENTERED)
			return TRUE;
		return FALSE;
	}
	else if (bg->placement == GNOME_BG_PLACEMENT_TILED) {
		return FALSE;
	}
	else {
		return TRUE;
	}
}

static void
gnome_bg_get_pixmap_size (GnomeBG  *bg,
			  int       width,
			  int       height,
			  int      *pixmap_width,
			  int      *pixmap_height)
{
	int dummy;
	int pb_width, pb_height;
	
	if (!pixmap_width)
		pixmap_width = &dummy;
	if (!pixmap_height)
		pixmap_height = &dummy;
	
	*pixmap_width = width;
	*pixmap_height = height;
	
	if (!get_pixbuf (bg)) {
		switch (bg->color_type) {
		case GNOME_BG_COLOR_SOLID:
			*pixmap_width = 1;
			*pixmap_height = 1;
			break;
			
		case GNOME_BG_COLOR_H_GRADIENT:
			*pixmap_width = width;
			*pixmap_height = GRADIENT_PIXMAP_TILE_SIZE;
			break;
			
		case GNOME_BG_COLOR_V_GRADIENT:
			*pixmap_width = GRADIENT_PIXMAP_TILE_SIZE;
			*pixmap_height = height;
			break;
		}
		
		return;
	}
	
	pb_width = gdk_pixbuf_get_width (get_pixbuf (bg));
	pb_height = gdk_pixbuf_get_height (get_pixbuf (bg));
	
	if (bg->placement == GNOME_BG_PLACEMENT_TILED) {
		if (gdk_pixbuf_get_has_alpha (get_pixbuf (bg)) &&
		    bg->color_type != GNOME_BG_COLOR_SOLID) {
			if (bg->color_type == GNOME_BG_COLOR_H_GRADIENT) {
				/* FIXME: Should this be
				 * MAX (GRADIENT_TILE_SIZE, pb_height)?
				 */
				*pixmap_height = pb_height; 
				*pixmap_width = width;
			}
			else {
				/* FIXME: Should this be
				 * MAX (GRAIDENT_TILE_SIZE, pb_width? */
				*pixmap_width = pb_width;
				*pixmap_height = height;
			}
		}
		else {
			*pixmap_width = pb_width;
			*pixmap_height = pb_height;
		}
	}
}

/**
 * gnome_bg_get_pixmap:
 * @bg: GnomeBG 
 * @window: 
 * @width: 
 * @height:
 *
 * Create a pixmap that can be set as background for @window. If @root is TRUE,
 * the pixmap created will be created by a temporary X server connection so
 * that if someone calls XKillClient on it, it won't affect the application who
 * created it.
 *
 * Since: 2.20
 **/
GdkPixmap *
gnome_bg_create_pixmap (GnomeBG	    *bg,
			GdkWindow   *window,
			int	     width,
			int	     height,
			gboolean     root)
{
	int pm_width, pm_height;
	GdkPixmap *pixmap;
	
	g_return_val_if_fail (bg != NULL, NULL);
	g_return_val_if_fail (window != NULL, NULL);

	if (bg->last_pixmap_width != width ||
	    bg->last_pixmap_height != height)  {
		if (bg->pixbuf_cache) {
			g_object_unref (bg->pixbuf_cache);
			bg->pixbuf_cache = NULL;
		}
	}
	bg->last_pixmap_width = width;
	bg->last_pixmap_height = height;

	gnome_bg_get_pixmap_size (bg, width, height, &pm_width, &pm_height);
	
	if (root) {
		pixmap = make_root_pixmap (gdk_drawable_get_screen (window),
					   pm_width, pm_height);
	}
	else {
		pixmap = gdk_pixmap_new (window, pm_width, pm_height, -1);
	}
	
	if (!get_pixbuf (bg) && bg->color_type == GNOME_BG_COLOR_SOLID) {
		GdkGC *gc = gdk_gc_new (pixmap);
		gdk_gc_set_rgb_fg_color (gc, &(bg->primary));
		
		gdk_draw_point (pixmap, gc, 0, 0);
		
		g_object_unref (gc);
	}
	else {
		GdkPixbuf *pixbuf;
		
		pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8,
					 width, height);
		gnome_bg_draw (bg, pixbuf);
		gdk_draw_pixbuf (pixmap, NULL, pixbuf,
				 0, 0,
				 0, 0, width, height,
				 GDK_RGB_DITHER_MAX, 0, 0);
		g_object_unref (pixbuf);
	}
	
	return pixmap;
}


/* determine if a background is darker or lighter than average, to help
 * clients know what colors to draw on top with
 */
gboolean
gnome_bg_is_dark (GnomeBG *bg)
{
	GdkColor color;
	int intensity;
	
	g_return_val_if_fail (bg != NULL, FALSE);
	
	if (bg->color_type == GNOME_BG_COLOR_SOLID) {
		color = bg->primary;
	} else {
		color.red = (bg->primary.red + bg->secondary.red) / 2;
		color.green = (bg->primary.green + bg->secondary.green) / 2;
		color.blue = (bg->primary.blue + bg->secondary.blue) / 2;
	}
	
	if (get_pixbuf (bg)) {
		guint32 argb = pixbuf_average_value (get_pixbuf (bg));
		guchar a = (argb >> 24) & 0xff;
		guchar r = (argb >> 16) & 0xff;
		guchar g = (argb >>  8) & 0xff;
		guchar b = (argb >>  0) & 0xff;
		
		color.red = (color.red * (0xFF - a) + r * 0x101 * a) / 0xFF;
		color.green = (color.green * (0xFF - a) + g * 0x101 * a) / 0xFF;
		color.blue = (color.blue * (0xFF - a) + b * 0x101 * a) / 0xFF;
	}
	
	intensity = (color.red * 77 +
		     color.green * 150 +
		     color.blue * 28) >> 16;
	
	return intensity < 160; /* biased slightly to be dark */
}

/* 
 * Create a persistent pixmap. We create a separate display
 * and set the closedown mode on it to RetainPermanent.
 */
static GdkPixmap *
make_root_pixmap (GdkScreen *screen, gint width, gint height)
{
	Display *display;
        const char *display_name;
	Pixmap result;
	GdkPixmap *gdk_pixmap;
	int screen_num;
	int depth;
	
	screen_num = gdk_screen_get_number (screen);
	
	gdk_flush ();
	
	display_name = gdk_display_get_name (gdk_screen_get_display (screen));
	display = XOpenDisplay (display_name);
	
        if (display == NULL) {
                g_warning ("Unable to open display '%s' when setting "
			   "background pixmap\n",
                           (display_name) ? display_name : "NULL");
                return NULL;
        }
	
	/* Desktop background pixmap should be created from 
	 * dummy X client since most applications will try to
	 * kill it with XKillClient later when changing pixmap
	 */
	
	XSetCloseDownMode (display, RetainPermanent);
	
	depth = DefaultDepth (display, screen_num);

	result = XCreatePixmap (display,
				RootWindow (display, screen_num),
				width, height, depth);
	
	XCloseDisplay (display);
	
	gdk_pixmap = gdk_pixmap_foreign_new_for_screen (screen, result,
							width, height, depth);

	gdk_drawable_set_colormap (
		GDK_DRAWABLE (gdk_pixmap),
		gdk_drawable_get_colormap (gdk_screen_get_root_window (screen)));
	
	return gdk_pixmap;
}

static gboolean
get_original_size (const char *filename,
		   int        *orig_width,
		   int        *orig_height)
{
	gboolean result;

        if (gdk_pixbuf_get_file_info (filename, orig_width, orig_height))
		result = TRUE;
	else
		result = FALSE;

	return result;
}

gboolean
gnome_bg_get_image_size (GnomeBG	       *bg,
			 GnomeDesktopThumbnailFactory *factory,
			 int		       *width,
			 int		       *height)
{
	GdkPixbuf *thumb;
	gboolean result = FALSE;
	const gchar *filename;
	
	g_return_val_if_fail (bg != NULL, FALSE);
	g_return_val_if_fail (factory != NULL, FALSE);
	
	if (!bg->filename)
		return FALSE;
	
	filename = bg->filename;
	thumb = create_thumbnail_for_filename (factory, filename);
	
	if (!thumb) {
		SlideShow *show = get_as_slideshow (bg, bg->filename);
		if (show) {
			double alpha;
			FileSize *fs;
			Slide *slide = get_current_slide (show, &alpha);
			fs = slide->file1->data;
			filename = fs->file;
			thumb = create_thumbnail_for_filename (factory, filename);
		}
	}

	if (thumb) {
		if (get_thumb_annotations (thumb, width, height))
			result = TRUE;
		
		g_object_unref (thumb);
	}

	if (!result) {
		if (get_original_size (filename, width, height))
			result = TRUE;
	}

	return result;
}

static double
fit_factor (int from_width, int from_height,
	    int to_width,   int to_height)
{
	return MIN (to_width  / (double) from_width, to_height / (double) from_height);
}

GdkPixbuf *
gnome_bg_create_thumbnail (GnomeBG               *bg,
			   GnomeDesktopThumbnailFactory *factory,
			   GdkScreen             *screen,
			   int                    dest_width,
			   int                    dest_height)
{
	GdkPixbuf *result;
	GdkPixbuf *thumb;
	
	g_return_val_if_fail (bg != NULL, NULL);
	
	result = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, dest_width, dest_height);
	
	draw_color (bg, result);
	
	thumb = create_img_thumbnail (bg, factory, screen, dest_width, dest_height);
	
	if (thumb) {
		draw_image (bg->placement, thumb, result);
		g_object_unref (thumb);
	}
	
	return result;
}

static void
gnome_bg_set_root_pixmap_id (GdkScreen *screen,
			     GdkPixmap *pixmap)
{
	int      result;
	gint     format;
	gulong   nitems;
	gulong   bytes_after;
	guchar  *data_esetroot;
	Pixmap   pixmap_id;
	Atom     type;
	Display *display;
	int      screen_num;

	screen_num = gdk_screen_get_number (screen);
	data_esetroot = NULL;

	display = GDK_DISPLAY_XDISPLAY (gdk_screen_get_display (screen));

	result = XGetWindowProperty (display,
				     RootWindow (display, screen_num),
				     gdk_x11_get_xatom_by_name ("ESETROOT_PMAP_ID"),
				     0L, 1L, False, XA_PIXMAP,
				     &type, &format, &nitems,
				     &bytes_after,
				     &data_esetroot);

	if (data_esetroot != NULL) {
		if (result == Success && type == XA_PIXMAP &&
		    format == 32 &&
		    nitems == 1) {
			gdk_error_trap_push ();
			XKillClient (display, *(Pixmap *)data_esetroot);
			gdk_flush ();
			gdk_error_trap_pop ();
		}
		XFree (data_esetroot);
	}
	
	pixmap_id = GDK_WINDOW_XWINDOW (pixmap);
	
	XChangeProperty (display, RootWindow (display, screen_num),
			 gdk_x11_get_xatom_by_name ("ESETROOT_PMAP_ID"),
			 XA_PIXMAP, 32, PropModeReplace,
			 (guchar *) &pixmap_id, 1);
	XChangeProperty (display, RootWindow (display, screen_num),
			 gdk_x11_get_xatom_by_name ("_XROOTPMAP_ID"), XA_PIXMAP,
			 32, PropModeReplace,
			 (guchar *) &pixmap_id, 1);
}

/* Set the root pixmap, and properties pointing to it. We
 * do this atomically with a server grab to make sure that
 * we won't leak the pixmap if somebody else it setting
 * it at the same time. (This assumes that they follow the
 * same conventions we do)
 */
void
gnome_bg_set_pixmap_as_root (GdkScreen *screen, GdkPixmap *pixmap)
{
	Display *display;
	int      screen_num;

	g_return_if_fail (screen != NULL);
	g_return_if_fail (pixmap != NULL);

	screen_num = gdk_screen_get_number (screen);

	display = GDK_DISPLAY_XDISPLAY (gdk_screen_get_display (screen));

	gdk_x11_display_grab (gdk_screen_get_display (screen));

	gnome_bg_set_root_pixmap_id (screen, pixmap);

	XSetWindowBackgroundPixmap (display, RootWindow (display, screen_num),
				    GDK_PIXMAP_XID (pixmap));
	XClearWindow (display, RootWindow (display, screen_num));

	gdk_display_flush (gdk_screen_get_display (screen));
	gdk_x11_display_ungrab (gdk_screen_get_display (screen));
}


/* Implementation of the pixbuf cache */
struct _SlideShow
{
	gint ref_count;
	double start_time;
	double total_duration;

	GQueue *slides;
	
	gboolean changes_with_size;

	/* used during parsing */
	struct tm start_tm;
	GQueue *stack;
};

static SlideShow *read_slideshow_file (const char *filename,
				       GError     **err);
static void       slideshow_ref       (SlideShow  *show);
static void       slideshow_unref     (SlideShow  *show);

static double
now (void)
{
	GTimeVal tv;

	g_get_current_time (&tv);

	return (double)tv.tv_sec + (tv.tv_usec / 1000000.0);
}

static Slide *
get_current_slide (SlideShow *show,
		   double    *alpha)
{
	double delta = fmod (now() - show->start_time, show->total_duration);
	GList *list;
	double elapsed;
	int i;

	if (delta < 0)
		delta += show->total_duration;

	elapsed = 0;
	i = 0;
	for (list = show->slides->head; list != NULL; list = list->next) {
		Slide *slide = list->data;

		if (elapsed + slide->duration > delta) {
			*alpha = (delta - elapsed) / (double)slide->duration;
			return slide;
		}

		i++;
		elapsed += slide->duration;
	}

	/* this should never happen since we have slides and we should always
	 * find a current slide for the elapsed time since beginning -- we're
	 * looping with fmod() */
	g_assert_not_reached ();

	return NULL;
}

static GdkPixbuf *
blend (GdkPixbuf *p1,
       GdkPixbuf *p2,
       double alpha)
{
	GdkPixbuf *result = gdk_pixbuf_copy (p1);
	GdkPixbuf *tmp;

	if (gdk_pixbuf_get_width (p2) != gdk_pixbuf_get_width (p1) ||
            gdk_pixbuf_get_height (p2) != gdk_pixbuf_get_height (p1)) {
		tmp = gdk_pixbuf_scale_simple (p2, 
					       gdk_pixbuf_get_width (p1),
					       gdk_pixbuf_get_height (p1),
					       GDK_INTERP_BILINEAR);
	}
        else {
		tmp = g_object_ref (p2);
	}
	
	pixbuf_blend (tmp, result, 0, 0, -1, -1, 0, 0, alpha);
        
        g_object_unref (tmp);	

	return result;
}

typedef	enum {
	PIXBUF,
	SLIDESHOW,
	THUMBNAIL
} FileType;

struct FileCacheEntry
{
	FileType type;
	char *filename;
	union {
		GdkPixbuf *pixbuf;
		SlideShow *slideshow;
		GdkPixbuf *thumbnail;
	} u;
};

static void
file_cache_entry_delete (FileCacheEntry *ent)
{
	g_free (ent->filename);
	
	switch (ent->type) {
	case PIXBUF:
		g_object_unref (ent->u.pixbuf);
		break;
	case SLIDESHOW:
		slideshow_unref (ent->u.slideshow);
		break;
	case THUMBNAIL:
		g_object_unref (ent->u.thumbnail);
		break;
	}

	g_free (ent);
}

static void
bound_cache (GnomeBG *bg)
{
      while (g_list_length (bg->file_cache) >= CACHE_SIZE) {
	      GList *last_link = g_list_last (bg->file_cache);
	      FileCacheEntry *ent = last_link->data;
	      
	      file_cache_entry_delete (ent);
	      
	      bg->file_cache = g_list_delete_link (bg->file_cache, last_link);
      }
}

static const FileCacheEntry *
file_cache_lookup (GnomeBG *bg, FileType type, const char *filename)
{
	GList *list;

	for (list = bg->file_cache; list != NULL; list = list->next) {
		FileCacheEntry *ent = list->data;

		if (ent && ent->type == type &&
		    strcmp (ent->filename, filename) == 0) {
			return ent;
		}
	}

	return NULL;
}

static FileCacheEntry *
file_cache_entry_new (GnomeBG *bg,
		      FileType type,
		      const char *filename)
{
	FileCacheEntry *ent = g_new0 (FileCacheEntry, 1);

	g_assert (!file_cache_lookup (bg, type, filename));
	
	ent->type = type;
	ent->filename = g_strdup (filename);

	bg->file_cache = g_list_prepend (bg->file_cache, ent);

	bound_cache (bg);
	
	return ent;
}

static void
file_cache_add_pixbuf (GnomeBG *bg,
		       const char *filename,
		       GdkPixbuf *pixbuf)
{
	FileCacheEntry *ent = file_cache_entry_new (bg, PIXBUF, filename);
	ent->u.pixbuf = pixbuf;
}

static void
file_cache_add_thumbnail (GnomeBG *bg,
			  const char *filename,
			  GdkPixbuf *pixbuf)
{
	FileCacheEntry *ent = file_cache_entry_new (bg, THUMBNAIL, filename);
	ent->u.thumbnail = pixbuf;
}

static void
file_cache_add_slide_show (GnomeBG *bg,
			   const char *filename,
			   SlideShow *show)
{
	FileCacheEntry *ent = file_cache_entry_new (bg, SLIDESHOW, filename);
	ent->u.slideshow = show;
}

static GdkPixbuf *
get_as_pixbuf (GnomeBG *bg, const char *filename)
{
	const FileCacheEntry *ent;
	if ((ent = file_cache_lookup (bg, PIXBUF, filename))) {
		return ent->u.pixbuf;
	}
	else {
		GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file (filename, NULL);

		if (pixbuf)
			file_cache_add_pixbuf (bg, filename, pixbuf);

		return pixbuf;
	}
}

static SlideShow *
get_as_slideshow (GnomeBG *bg, const char *filename)
{
	const FileCacheEntry *ent;
	if ((ent = file_cache_lookup (bg, SLIDESHOW, filename))) {
		return ent->u.slideshow;
	}
	else {
		SlideShow *show = read_slideshow_file (filename, NULL);

		if (show)
			file_cache_add_slide_show (bg, filename, show);

		return show;
	}
}

static GdkPixbuf *
get_as_thumbnail (GnomeBG *bg, GnomeDesktopThumbnailFactory *factory, const char *filename)
{
	const FileCacheEntry *ent;
	if ((ent = file_cache_lookup (bg, THUMBNAIL, filename))) {
		return ent->u.thumbnail;
	}
	else {
		GdkPixbuf *thumb = create_thumbnail_for_filename (factory, filename);

		if (thumb)
			file_cache_add_thumbnail (bg, filename, thumb);

		return thumb;
	}
}

static gboolean
on_timeout (gpointer data)
{
	GnomeBG *bg = data;

	if (bg->pixbuf_cache) {
		g_object_unref (bg->pixbuf_cache);
		bg->pixbuf_cache = NULL;
	}

	bg->timeout_id = 0;
	
	queue_transitioned (bg);

	return FALSE;
}

static void
ensure_timeout (GnomeBG *bg,
		Slide   *slide)
{
	if (!bg->timeout_id) {
		double timeout;
		
		if (slide->fixed) {
			timeout = slide->duration;
		}
		else {
			/* Maybe the number of steps should be configurable? */
			timeout = slide->duration / 255.0;
		}

		bg->timeout_id = g_timeout_add_full (
			G_PRIORITY_LOW,
			timeout * 1000, on_timeout, bg, NULL);

	}
}

static time_t
get_mtime (const char *filename)
{
	GFile     *file;
	GFileInfo *info;
	time_t     mtime;
	
	mtime = (time_t)-1;
	
	if (filename) {
		file = g_file_new_for_path (filename);
		info = g_file_query_info (file, G_FILE_ATTRIBUTE_TIME_MODIFIED,
					  G_FILE_QUERY_INFO_NONE, NULL, NULL);
		if (info) {
			mtime = g_file_info_get_attribute_uint64 (info,
								  G_FILE_ATTRIBUTE_TIME_MODIFIED);
			g_object_unref (info);
		}
		g_object_unref (file);
	}
	
	return mtime;
}

static GdkPixbuf *
scale_thumbnail (GnomeBGPlacement placement,
		 const char *filename,
		 GdkPixbuf *thumb,
		 GdkScreen *screen,
		 int	    dest_width,
		 int	    dest_height)
{
	int o_width;
	int o_height;
	
	if (placement != GNOME_BG_PLACEMENT_TILED &&
	    placement != GNOME_BG_PLACEMENT_CENTERED) {
		
		/* In this case, the pixbuf will be scaled to fit the screen anyway,
		 * so just return the pixbuf here
		 */
		return g_object_ref (thumb);
	}
	
	if (get_thumb_annotations (thumb, &o_width, &o_height)		||
	    (filename && get_original_size (filename, &o_width, &o_height))) {
		
		int scr_height = gdk_screen_get_height (screen);
		int scr_width = gdk_screen_get_width (screen);
		int thumb_width = gdk_pixbuf_get_width (thumb);
		int thumb_height = gdk_pixbuf_get_height (thumb);
		double screen_to_dest = fit_factor (scr_width, scr_height,
						    dest_width, dest_height);
		double thumb_to_orig  = fit_factor (thumb_width, thumb_height,
						    o_width, o_height);
		double f = thumb_to_orig * screen_to_dest;
		int new_width, new_height;
		
		new_width = floor (thumb_width * f + 0.5);
		new_height = floor (thumb_height * f + 0.5);

		if (placement == GNOME_BG_PLACEMENT_TILED) {
			/* Heuristic to make sure tiles don't become so small that
			 * they turn into a blur.
			 *
			 * This is strictly speaking incorrect, but the resulting
			 * thumbnail gives a much better idea what the background
			 * will actually look like.
			 */
			
			if ((new_width < 32 || new_height < 32) &&
			    (new_width < o_width / 4 || new_height < o_height / 4)) {
				new_width = o_width / 4;
				new_height = o_height / 4;
			}
		}
			
		thumb = gdk_pixbuf_scale_simple (thumb, new_width, new_height,
						 GDK_INTERP_BILINEAR);
	}
	else
		g_object_ref (thumb);
	
	return thumb;
}

static GdkPixbuf *
create_img_thumbnail (GnomeBG               *bg,
		      GnomeDesktopThumbnailFactory *factory,
		      GdkScreen             *screen,
		      int                    dest_width,
		      int                    dest_height)
{
	if (bg->filename) {
		GdkPixbuf *thumb = get_as_thumbnail (bg, factory, bg->filename);

		if (thumb) {
			return scale_thumbnail (
				bg->placement, bg->filename,
				thumb, screen, dest_width, dest_height);
		}
		else {
			SlideShow *show = get_as_slideshow (bg, bg->filename);
			
			if (show) {
				double alpha;
				Slide *slide;

				slideshow_ref (show);

				slide = get_current_slide (show, &alpha);

				if (slide->fixed) {
					GdkPixbuf *tmp;
					FileSize *fs;
					
					fs = slide->file1->data;
					tmp = get_as_thumbnail (bg, factory, fs->file);

					thumb = scale_thumbnail (
						bg->placement, fs->file,
						tmp, screen, dest_width, dest_height);
				}
				else {
					FileSize *fs;
					GdkPixbuf *p1, *p2;

					fs = slide->file1->data;
					p1 = get_as_thumbnail (bg, factory, fs->file);

					fs = slide->file2->data;
					p2 = get_as_thumbnail (bg, factory, fs->file);

					if (p1 && p2) {
						GdkPixbuf *thumb1, *thumb2;

						fs = slide->file1->data;
						thumb1 = scale_thumbnail (
							bg->placement, fs->file,
							p1, screen, dest_width, dest_height);

						fs = slide->file2->data;
						thumb2 = scale_thumbnail (
							bg->placement, fs->file,
							p2, screen, dest_width, dest_height);

						thumb = blend (thumb1, thumb2, alpha);

						g_object_unref (thumb1);
						g_object_unref (thumb2);
					}
				}

				ensure_timeout (bg, slide);

				slideshow_unref (show);
			}
		}

		return thumb;
	}

	return NULL;
}

/*
 * Find the FileSize that best matches the given size.
 * Do two passes; the first pass only considers FileSizes
 * that are larger than the given size.
 * We are looking for the image that best matches the aspect ratio.
 * When two images have the same aspect ratio, prefer the one whose
 * width is closer to the given width.
 */
static FileSize *
find_best_size (GSList *sizes, gint width, gint height)
{
	GSList *s;
	gdouble a, d, distance;
	FileSize *best = NULL;
	gint pass;

	a = width/(gdouble)height;
	distance = 10000.0;

	for (pass = 0; pass < 2; pass++) {
		for (s = sizes; s; s = s->next) {
			FileSize *size = s->data;

			if (pass == 0 && (size->width < width || size->height < height))
				continue;       

			d = fabs (a - size->width/(gdouble)size->height);
			if (d < distance) {
				distance = d;
				best = size;
			} 
			else if (d == distance) {
				if (abs (size->width - width) < abs (best->width - width)) {
					best = size;
				}
			}
		}

		if (best)
			break;
	}

	return best;
}

static GdkPixbuf *
get_pixbuf (GnomeBG *bg)
{
	/* FIXME: this ref=TRUE/FALSE stuff is crazy */
	
	gboolean ref = FALSE;
	
	if (!bg->pixbuf_cache && bg->filename) {
		ref = TRUE;
		bg->file_mtime = get_mtime (bg->filename);
		
		bg->pixbuf_cache = get_as_pixbuf (bg, bg->filename);
		if (!bg->pixbuf_cache) {
			SlideShow *show = get_as_slideshow (bg, bg->filename);

			if (show) {
				double alpha;
				Slide *slide;

				slideshow_ref (show);

				slide = get_current_slide (show, &alpha);

				if (slide->fixed) {
					FileSize *size;
					size = find_best_size (slide->file1, bg->last_pixmap_width, bg->last_pixmap_height);
					bg->pixbuf_cache = get_as_pixbuf (bg, size->file);
				}
				else {
					FileSize *size;
					GdkPixbuf *p1, *p2;
					size = find_best_size (slide->file1, bg->last_pixmap_width, bg->last_pixmap_height);
					p1 = get_as_pixbuf (bg, size->file);
					size = find_best_size (slide->file2, bg->last_pixmap_width, bg->last_pixmap_height);
					p2 = get_as_pixbuf (bg, size->file);


					if (p1 && p2) {
						bg->pixbuf_cache = blend (p1, p2, alpha);
						ref = FALSE;
					}
				}

				ensure_timeout (bg, slide);

				slideshow_unref (show);
			}
		}
	}

	if (bg->pixbuf_cache && ref)
		g_object_ref (bg->pixbuf_cache);
	
	return bg->pixbuf_cache;
}

static gboolean
is_different (GnomeBG    *bg,
	      const char *filename)
{
	if (!filename && bg->filename) {
		return TRUE;
	}
	else if (filename && !bg->filename) {
		return TRUE;
	}
	else if (!filename && !bg->filename) {
		return FALSE;
	}
	else {
		time_t mtime = get_mtime (filename);
		
		if (mtime != bg->file_mtime)
			return TRUE;
		
		if (strcmp (filename, bg->filename) != 0)
			return TRUE;
		
		return FALSE;
	}
}

static void
clear_cache (GnomeBG *bg)
{
	GList *list;

	if (bg->file_cache) {
		for (list = bg->file_cache; list != NULL; list = list->next) {
			FileCacheEntry *ent = list->data;
			
			file_cache_entry_delete (ent);
		}
		g_list_free (bg->file_cache);
		bg->file_cache = NULL;
	}
	
	if (bg->pixbuf_cache) {
		g_object_unref (bg->pixbuf_cache);
		
		bg->pixbuf_cache = NULL;
	}

	if (bg->timeout_id) {
		g_source_remove (bg->timeout_id);

		bg->timeout_id = 0;
	}
}

/* Pixbuf utilities */
static guint32
pixbuf_average_value (GdkPixbuf *pixbuf)
{
	guint64 a_total, r_total, g_total, b_total;
	guint row, column;
	int row_stride;
	const guchar *pixels, *p;
	int r, g, b, a;
	guint64 dividend;
	guint width, height;
	
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	row_stride = gdk_pixbuf_get_rowstride (pixbuf);
	pixels = gdk_pixbuf_get_pixels (pixbuf);
	
	/* iterate through the pixbuf, counting up each component */
	a_total = 0;
	r_total = 0;
	g_total = 0;
	b_total = 0;
	
	if (gdk_pixbuf_get_has_alpha (pixbuf)) {
		for (row = 0; row < height; row++) {
			p = pixels + (row * row_stride);
			for (column = 0; column < width; column++) {
				r = *p++;
				g = *p++;
				b = *p++;
				a = *p++;
				
				a_total += a;
				r_total += r * a;
				g_total += g * a;
				b_total += b * a;
			}
		}
		dividend = height * width * 0xFF;
		a_total *= 0xFF;
	} else {
		for (row = 0; row < height; row++) {
			p = pixels + (row * row_stride);
			for (column = 0; column < width; column++) {
				r = *p++;
				g = *p++;
				b = *p++;
				
				r_total += r;
				g_total += g;
				b_total += b;
			}
		}
		dividend = height * width;
		a_total = dividend * 0xFF;
	}
	
	return ((a_total + dividend / 2) / dividend) << 24
		| ((r_total + dividend / 2) / dividend) << 16
		| ((g_total + dividend / 2) / dividend) << 8
		| ((b_total + dividend / 2) / dividend);
}

static GdkPixbuf *
pixbuf_scale_to_fit (GdkPixbuf *src, int max_width, int max_height)
{
	double factor;
	int src_width, src_height;
	int new_width, new_height;
	
	src_width = gdk_pixbuf_get_width (src);
	src_height = gdk_pixbuf_get_height (src);
	
	factor = MIN (max_width  / (double) src_width, max_height / (double) src_height);
	
	new_width  = floor (src_width * factor + 0.5);
	new_height = floor (src_height * factor + 0.5);
	
	return gdk_pixbuf_scale_simple (src, new_width, new_height, GDK_INTERP_BILINEAR);	
}

static GdkPixbuf *
pixbuf_scale_to_min (GdkPixbuf *src, int min_width, int min_height)
{
	double factor;
	int src_width, src_height;
	int new_width, new_height;
	
	src_width = gdk_pixbuf_get_width (src);
	src_height = gdk_pixbuf_get_height (src);
	
	factor = MAX (min_width / (double) src_width, min_height / (double) src_height);
	
	new_width = floor (src_width * factor + 0.5);
	new_height = floor (src_height * factor + 0.5);
	
	return gdk_pixbuf_scale_simple (src, new_width, new_height, GDK_INTERP_BILINEAR);
}

static guchar *
create_gradient (const GdkColor *primary,
		 const GdkColor *secondary,
		 int	          n_pixels)
{
	guchar *result = g_malloc (n_pixels * 3);
	int i;
	
	for (i = 0; i < n_pixels; ++i) {
		double ratio = (i + 0.5) / n_pixels;
		
		result[3 * i + 0] = ((guint16) (primary->red * (1 - ratio) + secondary->red * ratio)) >> 8;
		result[3 * i + 1] = ((guint16) (primary->green * (1 - ratio) + secondary->green * ratio)) >> 8;
		result[3 * i + 2] = ((guint16) (primary->blue * (1 - ratio) + secondary->blue * ratio)) >> 8;
	}
	
	return result;
}	

static void
pixbuf_draw_gradient (GdkPixbuf *pixbuf,
		      gboolean   horizontal,
		      GdkColor  *primary,
		      GdkColor  *secondary)
{
	int width  = gdk_pixbuf_get_width (pixbuf);
	int height = gdk_pixbuf_get_height (pixbuf);
	int rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	guchar *dst = gdk_pixbuf_get_pixels (pixbuf);
	guchar *dst_limit = dst + height * rowstride;
	
	if (horizontal) {
		guchar *gradient = create_gradient (primary, secondary, width);
		int copy_bytes_per_row = width * 3;
		
		while (dst < dst_limit) {
			memcpy (dst, gradient, copy_bytes_per_row);
			dst += rowstride;
		}
		g_free (gradient);
	} else {
		guchar *gb, *gradient;
		
		gb = gradient = create_gradient (primary, secondary, height);
		while (dst < dst_limit) {
			int i;
			guchar *d = dst;
			guchar r = *gb++;
			guchar g = *gb++;
			guchar b = *gb++;
			for (i = 0; i < width; i++) {
				*d++ = r;
				*d++ = g;
				*d++ = b;
			}
			dst += rowstride;
		}
		g_free (gradient);
	}
}

static void
pixbuf_blend (GdkPixbuf *src,
	      GdkPixbuf *dest,
	      int	 src_x,
	      int	 src_y,
	      int	 width,
	      int        height,
	      int	 dest_x,
	      int	 dest_y,
	      double	 alpha)
{
	int dest_width = gdk_pixbuf_get_width (dest);
	int dest_height = gdk_pixbuf_get_height (dest);
	int offset_x = dest_x - src_x;
	int offset_y = dest_y - src_y;

	if (width < 0)
		width = gdk_pixbuf_get_width (src);

	if (height < 0)
		height = gdk_pixbuf_get_height (src);
	
	if (dest_x < 0)
		dest_x = 0;
	
	if (dest_y < 0)
		dest_y = 0;
	
	if (dest_x + width > dest_width) {
		width = dest_width - dest_x;
	}
	
	if (dest_y + height > dest_height) {
		height = dest_height - dest_y;
	}

	gdk_pixbuf_composite (src, dest,
			      dest_x, dest_y,
			      width, height,
			      offset_x, offset_y,
			      1, 1, GDK_INTERP_NEAREST,
			      alpha * 0xFF + 0.5);
}

static void
pixbuf_tile (GdkPixbuf *src, GdkPixbuf *dest)
{
	int x, y;
	int tile_width, tile_height;
	int dest_width = gdk_pixbuf_get_width (dest);
	int dest_height = gdk_pixbuf_get_height (dest);
	
	tile_width = gdk_pixbuf_get_width (src);
	tile_height = gdk_pixbuf_get_height (src);
	
	for (y = 0; y < dest_height; y += tile_height) {
		for (x = 0; x < dest_width; x += tile_width) {
			pixbuf_blend (src, dest, 0, 0,
				      tile_width, tile_height, x, y, 1.0);
		}
	}
}

static gboolean stack_is (SlideShow *parser, const char *s1, ...);

/* Parser for fading background */
static void
handle_start_element (GMarkupParseContext *context,
		      const gchar         *name,
		      const gchar        **attr_names,
		      const gchar        **attr_values,
		      gpointer             user_data,
		      GError             **err)
{
	SlideShow *parser = user_data;
	gint i;
	
	if (strcmp (name, "static") == 0 || strcmp (name, "transition") == 0) {
		Slide *slide = g_new0 (Slide, 1);
		
		if (strcmp (name, "static") == 0)
			slide->fixed = TRUE;
		
		g_queue_push_tail (parser->slides, slide);
	}
	else if (strcmp (name, "size") == 0) {
		Slide *slide = parser->slides->tail->data;
		FileSize *size = g_new0 (FileSize, 1);
		for (i = 0; attr_names[i]; i++) {
			if (strcmp (attr_names[i], "width") == 0)
				size->width = atoi (attr_values[i]);
			else if (strcmp (attr_names[i], "height") == 0)
				size->height = atoi (attr_values[i]);
		}
		if (parser->stack->tail &&
		    (strcmp (parser->stack->tail->data, "file") == 0 ||
		     strcmp (parser->stack->tail->data, "from") == 0)) {
			slide->file1 = g_slist_prepend (slide->file1, size);
		}
		else if (parser->stack->tail &&
			 strcmp (parser->stack->tail->data, "to") == 0) { 
			slide->file2 = g_slist_prepend (slide->file2, size);
		}
	}
	g_queue_push_tail (parser->stack, g_strdup (name));
}

static void
handle_end_element (GMarkupParseContext *context,
		    const gchar         *name,
		    gpointer             user_data,
		    GError             **err)
{
	SlideShow *parser = user_data;
	
	g_free (g_queue_pop_tail (parser->stack));
}

static gboolean
stack_is (SlideShow *parser,
	  const char *s1,
	  ...)
{
	GList *stack = NULL;
	const char *s;
	GList *l1, *l2;
	va_list args;
	
	stack = g_list_prepend (stack, (gpointer)s1);
	
	va_start (args, s1);
	
	s = va_arg (args, const char *);
	while (s) {
		stack = g_list_prepend (stack, (gpointer)s);
		s = va_arg (args, const char *);
	}
	
	l1 = stack;
	l2 = parser->stack->head;
	
	while (l1 && l2) {
		if (strcmp (l1->data, l2->data) != 0) {
			g_list_free (stack);
			return FALSE;
		}
		
		l1 = l1->next;
		l2 = l2->next;
	}

	g_list_free (stack);

	return (!l1 && !l2);
}

static int
parse_int (const char *text)
{
	return strtol (text, NULL, 0);
}

static void
handle_text (GMarkupParseContext *context,
	     const gchar         *text,
	     gsize                text_len,
	     gpointer             user_data,
	     GError             **err)
{
	SlideShow *parser = user_data;
	Slide *slide = parser->slides->tail? parser->slides->tail->data : NULL;
	FileSize *fs;
	gint i;

	if (stack_is (parser, "year", "starttime", "background", NULL)) {
		parser->start_tm.tm_year = parse_int (text) - 1900;
	}
	else if (stack_is (parser, "month", "starttime", "background", NULL)) {
		parser->start_tm.tm_mon = parse_int (text) - 1;
	}
	else if (stack_is (parser, "day", "starttime", "background", NULL)) {
		parser->start_tm.tm_mday = parse_int (text);
	}
	else if (stack_is (parser, "hour", "starttime", "background", NULL)) {
		parser->start_tm.tm_hour = parse_int (text) - 1;
	}
	else if (stack_is (parser, "minute", "starttime", "background", NULL)) {
		parser->start_tm.tm_min = parse_int (text);
	}
	else if (stack_is (parser, "second", "starttime", "background", NULL)) {
		parser->start_tm.tm_sec = parse_int (text);
	}
	else if (stack_is (parser, "duration", "static", "background", NULL) ||
		 stack_is (parser, "duration", "transition", "background", NULL)) {
		slide->duration = g_strtod (text, NULL);
		parser->total_duration += slide->duration;
	}
	else if (stack_is (parser, "file", "static", "background", NULL) ||
		 stack_is (parser, "from", "transition", "background", NULL)) {
		for (i = 0; text[i]; i++) {
			if (!g_ascii_isspace (text[i]))
				break;
		}
		if (text[i] == 0)
			return;
		fs = g_new (FileSize, 1);
		fs->width = -1;
		fs->height = -1;
		fs->file = g_strdup (text);
		slide->file1 = g_slist_prepend (slide->file1, fs);
		if (slide->file1->next != NULL)
			parser->changes_with_size = TRUE;                       
	}
	else if (stack_is (parser, "size", "file", "static", "background", NULL) ||
		 stack_is (parser, "size", "from", "transition", "background", NULL)) {
		fs = slide->file1->data;
		fs->file = g_strdup (text);
		if (slide->file1->next != NULL)
			parser->changes_with_size = TRUE; 
	}
	else if (stack_is (parser, "to", "transition", "background", NULL)) {
		for (i = 0; text[i]; i++) {
			if (!g_ascii_isspace (text[i]))
				break;
		}
		if (text[i] == 0)
			return;
		fs = g_new (FileSize, 1);
		fs->width = -1;
		fs->height = -1;
		fs->file = g_strdup (text);
		slide->file2 = g_slist_prepend (slide->file2, fs);
		if (slide->file2->next != NULL)
			parser->changes_with_size = TRUE;                       
	}
	else if (stack_is (parser, "size", "to", "transition", "background", NULL)) {
		fs = slide->file2->data;
		fs->file = g_strdup (text);
		if (slide->file2->next != NULL)
			parser->changes_with_size = TRUE;
	}
}

static void
slideshow_ref (SlideShow *show)
{
	show->ref_count++;
}

static void
slideshow_unref (SlideShow *show)
{
	GList *list;
	GSList *slist;
	FileSize *size;

	show->ref_count--;
	if (show->ref_count > 0)
		return;

	for (list = show->slides->head; list != NULL; list = list->next) {
		Slide *slide = list->data;

		for (slist = slide->file1; slist != NULL; slist = slist->next) {
			size = slist->data;
			g_free (size->file);
			g_free (size);
		}
		g_slist_free (slide->file1);

		for (slist = slide->file2; slist != NULL; slist = slist->next) {
			size = slist->data;
			g_free (size->file);
			g_free (size);
		}
		g_slist_free (slide->file2);

		g_free (slide);
	}

	g_queue_free (show->slides);
	
	g_list_foreach (show->stack->head, (GFunc) g_free, NULL);
	g_queue_free (show->stack);
	
	g_free (show);
}

static void
dump_bg (SlideShow *show)
{
#if 0
	GList *list;
	GSList *slist;
	
	for (list = show->slides->head; list != NULL; list = list->next)
	{
		Slide *slide = list->data;
		
		g_print ("\nSlide: %s\n", slide->fixed? "fixed" : "transition");
		g_print ("duration: %f\n", slide->duration);
		g_print ("File1:\n");
		for (slist = slide->file1; slist != NULL; slist = slist->next) {
			FileSize *size = slist->data;
			g_print ("\t%s (%dx%d)\n", 
				 size->file, size->width, size->height);
		}
		g_print ("File2:\n");
		for (slist = slide->file2; slist != NULL; slist = slist->next) {
			FileSize *size = slist->data;
			g_print ("\t%s (%dx%d)\n", 
				 size->file, size->width, size->height);
		}
	}
#endif
}

static void
threadsafe_localtime (time_t time, struct tm *tm)
{
	struct tm *res;
	
	G_LOCK_DEFINE_STATIC (localtime_mutex);

	G_LOCK (localtime_mutex);

	res = localtime (&time);
	if (tm) {
		*tm = *res;
	}
	
	G_UNLOCK (localtime_mutex);
}

static SlideShow *
read_slideshow_file (const char *filename,
		     GError     **err)
{
	GMarkupParser parser = {
		handle_start_element,
		handle_end_element,
		handle_text,
		NULL, /* passthrough */
		NULL, /* error */
	};
	
	GFile *file;
	char *contents = NULL;
	gsize len;
	SlideShow *show = NULL;
	GMarkupParseContext *context = NULL;
	time_t t;

	if (!filename)
		return NULL;

	file = g_file_new_for_path (filename);
	if (!g_file_load_contents (file, NULL, &contents, &len, NULL, NULL)) {
		g_object_unref (file);
		return NULL;
	}
	g_object_unref (file);
	
	show = g_new0 (SlideShow, 1);
	show->ref_count = 1;
	threadsafe_localtime ((time_t)0, &show->start_tm);
	show->stack = g_queue_new ();
	show->slides = g_queue_new ();
	
	context = g_markup_parse_context_new (&parser, 0, show, NULL);
	
	if (!g_markup_parse_context_parse (context, contents, len, err)) {
		slideshow_unref (show);
		show = NULL;
	}
	

	if (show) {
		if (!g_markup_parse_context_end_parse (context, err)) {
			slideshow_unref (show);
			show = NULL;
		}
	}
	
	g_markup_parse_context_free (context);

	if (show) {
		t = mktime (&show->start_tm);

		show->start_time = (double)t;
			
		dump_bg (show);

		/* no slides, that's not a slideshow */
		if (g_queue_get_length (show->slides) == 0) {
			slideshow_unref (show);
			show = NULL;
		}
	}

	g_free (contents);
	
	return show;
}

/* Thumbnail utilities */
static GdkPixbuf *
create_thumbnail_for_filename (GnomeDesktopThumbnailFactory *factory,
			       const char            *filename)
{
	char *thumb;
	time_t mtime;
	GdkPixbuf *orig, *result = NULL;
	char *uri;
	
	mtime = get_mtime (filename);
	
	if (mtime == (time_t)-1)
		return NULL;
	
	uri = g_filename_to_uri (filename, NULL, NULL);
	
	thumb = gnome_desktop_thumbnail_factory_lookup (factory, uri, mtime);
	
	if (thumb) {
		result = gdk_pixbuf_new_from_file (thumb, NULL);
		g_free (thumb);
	}
	else {
		orig = gdk_pixbuf_new_from_file (filename, NULL);
		if (orig) {
			int orig_width = gdk_pixbuf_get_width (orig);
			int orig_height = gdk_pixbuf_get_height (orig);
			
			result = pixbuf_scale_to_fit (orig, 128, 128);
			
			g_object_set_data_full (G_OBJECT (result), "gnome-thumbnail-height",
						g_strdup_printf ("%d", orig_height), g_free);
			g_object_set_data_full (G_OBJECT (result), "gnome-thumbnail-width",
						g_strdup_printf ("%d", orig_width), g_free);
			
			g_object_unref (orig);
			
			gnome_desktop_thumbnail_factory_save_thumbnail (factory, result, uri, mtime);
		}
		else {
			gnome_desktop_thumbnail_factory_create_failed_thumbnail (factory, uri, mtime);
		}
	}

	g_free (uri);

	return result;
}

static gboolean
get_thumb_annotations (GdkPixbuf *thumb,
		       int	 *orig_width,
		       int	 *orig_height)
{
	char *end;
	const char *wstr, *hstr;
	
	wstr = gdk_pixbuf_get_option (thumb, "tEXt::Thumb::Image::Width");
	hstr = gdk_pixbuf_get_option (thumb, "tEXt::Thumb::Image::Height");
	
	if (hstr && wstr) {
		*orig_width = strtol (wstr, &end, 10);
		if (*end != 0)
			return FALSE;
		
		*orig_height = strtol (hstr, &end, 10);
		if (*end != 0)
			return FALSE;
		
		return TRUE;
	}
	
	return FALSE;
}

static gboolean
slideshow_changes_with_size (SlideShow *show)
{
	return show->changes_with_size;
}

