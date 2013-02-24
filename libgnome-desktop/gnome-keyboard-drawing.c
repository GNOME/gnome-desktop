/*
 * Copyright (C) 2006 Sergey V. Udaltsov <svu@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XKBgeom.h>
#include <X11/extensions/XKBrules.h>
#include <stdlib.h>
#include <memory.h>
#include <math.h>
#include <glib/gi18n-lib.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API

#include "gnome-xkb-info.h"
#include "gnome-keyboard-drawing.h"

#define noKBDRAW_DEBUG

#define INVALID_KEYCODE ((guint)(-1))

#define GTK_RESPONSE_PRINT 2

#define CAIRO_LINE_WIDTH 1.0

#define KEY_FONT_SIZE 12

typedef struct _GnomeKeyboardDrawingItem GnomeKeyboardDrawingItem;
typedef struct _GnomeKeyboardDrawingKey GnomeKeyboardDrawingKey;
typedef struct _GnomeKeyboardDrawingDoodad GnomeKeyboardDrawingDoodad;
typedef struct _GnomeKeyboardDrawingGroupLevel GnomeKeyboardDrawingGroupLevel;
typedef struct _GnomeKeyboardDrawingRenderContext GnomeKeyboardDrawingRenderContext;

typedef enum {
	GNOME_KEYBOARD_DRAWING_ITEM_TYPE_INVALID = 0,
	GNOME_KEYBOARD_DRAWING_ITEM_TYPE_KEY,
	GNOME_KEYBOARD_DRAWING_ITEM_TYPE_KEY_EXTRA,
	GNOME_KEYBOARD_DRAWING_ITEM_TYPE_DOODAD
} GnomeKeyboardDrawingItemType;

typedef enum {
	GNOME_KEYBOARD_DRAWING_POS_TOPLEFT,
	GNOME_KEYBOARD_DRAWING_POS_TOPRIGHT,
	GNOME_KEYBOARD_DRAWING_POS_BOTTOMLEFT,
	GNOME_KEYBOARD_DRAWING_POS_BOTTOMRIGHT,
	GNOME_KEYBOARD_DRAWING_POS_TOTAL,
	GNOME_KEYBOARD_DRAWING_POS_FIRST =
	GNOME_KEYBOARD_DRAWING_POS_TOPLEFT,
	GNOME_KEYBOARD_DRAWING_POS_LAST =
	GNOME_KEYBOARD_DRAWING_POS_BOTTOMRIGHT
} GnomeKeyboardDrawingGroupLevelPosition;

/* units are in xkb form */
struct _GnomeKeyboardDrawingItem {
	/*< private > */

	GnomeKeyboardDrawingItemType type;
	gint origin_x;
	gint origin_y;
	gint angle;
	guint priority;
};

/* units are in xkb form */
struct _GnomeKeyboardDrawingKey {
	/*< private > */

	GnomeKeyboardDrawingItemType type;
	gint origin_x;
	gint origin_y;
	gint angle;
	guint priority;

	XkbKeyRec *xkbkey;
	gboolean pressed;
	guint keycode;
};

/* units are in xkb form */
struct _GnomeKeyboardDrawingDoodad {
	/*< private > */

	GnomeKeyboardDrawingItemType type;
	gint origin_x;
	gint origin_y;
	gint angle;
	guint priority;

	XkbDoodadRec *doodad;
	gboolean on;		/* for indicator doodads */
};

struct _GnomeKeyboardDrawingGroupLevel {
	gint group;
	gint level;
};

struct _GnomeKeyboardDrawingRenderContext {
	cairo_t *cr;

	gint angle;		/* current angle pango is set to draw at, in tenths of a degree */
	PangoLayout *layout;
	PangoFontDescription *font_desc;

	gint scale_numerator;
	gint scale_denominator;

	GdkRGBA dark_color;
};

struct _GnomeKeyboardDrawing {
	/*< private > */

	GtkDrawingArea parent;

	XkbDescRec *xkb;
	gboolean xkbOnDisplay;
	guint l3mod;

	GnomeKeyboardDrawingRenderContext *renderContext;

	/* Indexed by keycode */
	GnomeKeyboardDrawingKey *keys;

	/* list of stuff to draw in priority order */
	GList *keyboard_items;

	GdkRGBA *colors;

	guint timeout;

	Display *display;
	gint screen_num;

	gint xkb_event_type;

	GnomeKeyboardDrawingDoodad **physical_indicators;
	gint physical_indicators_size;
};

struct _GnomeKeyboardDrawingClass {
	GtkDrawingAreaClass parent_class;

	/* we send this signal when the user presses a key that "doesn't exist"
	 * according to the keyboard geometry; it probably means their xkb
	 * configuration is incorrect */
	void (*bad_keycode) (GnomeKeyboardDrawing * drawing, guint keycode);
};

G_DEFINE_TYPE (GnomeKeyboardDrawing, gnome_keyboard_drawing, GTK_TYPE_DRAWING_AREA)

static GnomeKeyboardDrawingGroupLevel defaultGroupsLevels[] = {
	{0, 1},
	{0, 3},
	{0, 0},
	{0, 2}
};

static GnomeKeyboardDrawingGroupLevel *groupLevels[] = {
	defaultGroupsLevels,
	defaultGroupsLevels + 1,
	defaultGroupsLevels + 2,
	defaultGroupsLevels + 3
};

enum {
	BAD_KEYCODE = 0,
	NUM_SIGNALS
};

static guint gnome_keyboard_drawing_signals[NUM_SIGNALS] = { 0 };

static void gnome_keyboard_drawing_set_keyboard (GnomeKeyboardDrawing *kbdrawing,
                                                 XkbComponentNamesRec *names);

static gboolean gnome_keyboard_drawing_render (GnomeKeyboardDrawing *kbdrawing,
					       cairo_t              *cr,
					       PangoLayout          *layout,
					       double                x,
                                               double                y,
					       double                width,
                                               double                height,
					       gdouble               dpi_x,
                                               gdouble               dpi_y);

static gint
xkb_to_pixmap_coord (GnomeKeyboardDrawingRenderContext *context, gint n)
{
	return n * context->scale_numerator / context->scale_denominator;
}

static gdouble
xkb_to_pixmap_double (GnomeKeyboardDrawingRenderContext *context,
		      gdouble                            d)
{
	return d * context->scale_numerator / context->scale_denominator;
}


/* angle is in tenths of a degree; coordinates can be anything as (xkb,
 * pixels, pango) as long as they are all the same */
static void
rotate_coordinate (gint origin_x,
		   gint origin_y,
		   gint x,
		   gint y, gint angle, gint *rotated_x, gint *rotated_y)
{
	*rotated_x =
		origin_x + (x - origin_x) * cos (M_PI * angle / 1800.0) - (y -
									   origin_y)
		* sin (M_PI * angle / 1800.0);
	*rotated_y =
		origin_y + (x - origin_x) * sin (M_PI * angle / 1800.0) + (y -
									   origin_y)
		* cos (M_PI * angle / 1800.0);
}

static gdouble
length (gdouble x, gdouble y)
{
	return sqrt (x * x + y * y);
}

static gdouble
point_line_distance (gdouble ax, gdouble ay, gdouble nx, gdouble ny)
{
	return ax * nx + ay * ny;
}

static void
normal_form (gdouble ax, gdouble ay,
	     gdouble bx, gdouble by,
	     gdouble *nx, gdouble *ny, gdouble *d)
{
	gdouble l;

	*nx = by - ay;
	*ny = ax - bx;

	l = length (*nx, *ny);

	*nx /= l;
	*ny /= l;

	*d = point_line_distance (ax, ay, *nx, *ny);
}

static void
inverse (gdouble a, gdouble b, gdouble c, gdouble d,
	 gdouble *e, gdouble *f, gdouble *g, gdouble *h)
{
	gdouble det;

	det = a * d - b * c;

	*e = d / det;
	*f = -b / det;
	*g = -c / det;
	*h = a / det;
}

static void
multiply (gdouble a, gdouble b, gdouble c, gdouble d,
	  gdouble e, gdouble f, gdouble *x, gdouble *y)
{
	*x = a * e + b * f;
	*y = c * e + d * f;
}

static void
intersect (gdouble n1x, gdouble n1y, gdouble d1,
	   gdouble n2x, gdouble n2y, gdouble d2, gdouble *x, gdouble *y)
{
	gdouble e, f, g, h;

	inverse (n1x, n1y, n2x, n2y, &e, &f, &g, &h);
	multiply (e, f, g, h, d1, d2, x, y);
}


/* draw an angle from the current point to b and then to c,
 * with a rounded corner of the given radius.
 */
static void
rounded_corner (cairo_t *cr,
		gdouble bx, gdouble by,
		gdouble cx, gdouble cy, gdouble radius)
{
	gdouble ax, ay;
	gdouble n1x, n1y, d1;
	gdouble n2x, n2y, d2;
	gdouble pd1, pd2;
	gdouble ix, iy;
	gdouble dist1, dist2;
	gdouble nx, ny, d;
	gdouble a1x, a1y, c1x, c1y;
	gdouble phi1, phi2;

	cairo_get_current_point (cr, &ax, &ay);
#ifdef KBDRAW_DEBUG
	printf ("        current point: (%f, %f), radius %f:\n", ax, ay,
		radius);
#endif

	/* make sure radius is not too large */
	dist1 = length (bx - ax, by - ay);
	dist2 = length (cx - bx, cy - by);

	radius = MIN (radius, MIN (dist1, dist2));

	/* construct normal forms of the lines */
	normal_form (ax, ay, bx, by, &n1x, &n1y, &d1);
	normal_form (bx, by, cx, cy, &n2x, &n2y, &d2);

	/* find which side of the line a,b the point c is on */
	if (point_line_distance (cx, cy, n1x, n1y) < d1)
		pd1 = d1 - radius;
	else
		pd1 = d1 + radius;

	/* find which side of the line b,c the point a is on */
	if (point_line_distance (ax, ay, n2x, n2y) < d2)
		pd2 = d2 - radius;
	else
		pd2 = d2 + radius;

	/* intersect the parallels to find the center of the arc */
	intersect (n1x, n1y, pd1, n2x, n2y, pd2, &ix, &iy);

	nx = (bx - ax) / dist1;
	ny = (by - ay) / dist1;
	d = point_line_distance (ix, iy, nx, ny);

	/* a1 is the point on the line a-b where the arc starts */
	intersect (n1x, n1y, d1, nx, ny, d, &a1x, &a1y);

	nx = (cx - bx) / dist2;
	ny = (cy - by) / dist2;
	d = point_line_distance (ix, iy, nx, ny);

	/* c1 is the point on the line b-c where the arc ends */
	intersect (n2x, n2y, d2, nx, ny, d, &c1x, &c1y);

	/* determine the first angle */
	if (a1x - ix == 0)
		phi1 = (a1y - iy > 0) ? M_PI_2 : 3 * M_PI_2;
	else if (a1x - ix > 0)
		phi1 = atan ((a1y - iy) / (a1x - ix));
	else
		phi1 = M_PI + atan ((a1y - iy) / (a1x - ix));

	/* determine the second angle */
	if (c1x - ix == 0)
		phi2 = (c1y - iy > 0) ? M_PI_2 : 3 * M_PI_2;
	else if (c1x - ix > 0)
		phi2 = atan ((c1y - iy) / (c1x - ix));
	else
		phi2 = M_PI + atan ((c1y - iy) / (c1x - ix));

	/* compute the difference between phi2 and phi1 mod 2pi */
	d = phi2 - phi1;
	while (d < 0)
		d += 2 * M_PI;
	while (d > 2 * M_PI)
		d -= 2 * M_PI;

#ifdef KBDRAW_DEBUG
	printf ("        line 1 to: (%f, %f):\n", a1x, a1y);
#endif
	if (!(isnan (a1x) || isnan (a1y)))
		cairo_line_to (cr, a1x, a1y);

	/* pick the short arc from phi1 to phi2 */
	if (d < M_PI)
		cairo_arc (cr, ix, iy, radius, phi1, phi2);
	else
		cairo_arc_negative (cr, ix, iy, radius, phi1, phi2);

#ifdef KBDRAW_DEBUG
	printf ("        line 2 to: (%f, %f):\n", cx, cy);
#endif
	cairo_line_to (cr, cx, cy);
}

static void
rounded_polygon (cairo_t  *cr,
		 gboolean  filled,
		 gdouble   radius,
                 GdkPoint *points,
                 gint      num_points)
{
	gint i, j;

	cairo_move_to (cr,
		       (gdouble) (points[num_points - 1].x +
				  points[0].x) / 2,
		       (gdouble) (points[num_points - 1].y +
				  points[0].y) / 2);


#ifdef KBDRAW_DEBUG
	printf ("    rounded polygon of radius %f:\n", radius);
#endif
	for (i = 0; i < num_points; i++) {
		j = (i + 1) % num_points;
		rounded_corner (cr, (gdouble) points[i].x,
				(gdouble) points[i].y,
				(gdouble) (points[i].x + points[j].x) / 2,
				(gdouble) (points[i].y + points[j].y) / 2,
				radius);
#ifdef KBDRAW_DEBUG
		printf ("      corner (%d, %d) -> (%d, %d):\n",
			points[i].x, points[i].y, points[j].x,
			points[j].y);
#endif
	};
	cairo_close_path (cr);

	if (filled)
		cairo_fill (cr);
	else {
		cairo_set_line_width (cr, CAIRO_LINE_WIDTH);
		cairo_stroke (cr);
	}
}

static void
draw_polygon (GnomeKeyboardDrawingRenderContext *context,
	      GdkRGBA                           *fill_color,
	      gint                               xkb_x,
	      gint                               xkb_y,
              XkbPointRec                       *xkb_points,
              guint                              num_points,
	      gdouble                            radius)
{
	GdkPoint *points;
	gboolean filled;
	gint i;

	if (fill_color) {
		filled = TRUE;
	} else {
		fill_color = &context->dark_color;
		filled = FALSE;
	}

	gdk_cairo_set_source_rgba (context->cr, fill_color);

	points = g_new (GdkPoint, num_points);

#ifdef KBDRAW_DEBUG
	printf ("    Polygon points:\n");
#endif
	for (i = 0; i < num_points; i++) {
		points[i].x =
			xkb_to_pixmap_coord (context, xkb_x + xkb_points[i].x);
		points[i].y =
			xkb_to_pixmap_coord (context, xkb_y + xkb_points[i].y);
#ifdef KBDRAW_DEBUG
		printf ("      %d, %d\n", points[i].x, points[i].y);
#endif
	}

	rounded_polygon (context->cr, filled,
			 xkb_to_pixmap_double (context, radius),
			 points, num_points);

	g_free (points);
}

static void
curve_rectangle (cairo_t * cr,
		 gdouble x0,
		 gdouble y0, gdouble width, gdouble height, gdouble radius)
{
	gdouble x1, y1;

	if (!width || !height)
		return;

	x1 = x0 + width;
	y1 = y0 + height;

	radius = MIN (radius, MIN (width / 2, height / 2));

	cairo_move_to (cr, x0, y0 + radius);
	cairo_arc (cr, x0 + radius, y0 + radius, radius, M_PI,
		   3 * M_PI / 2);
	cairo_line_to (cr, x1 - radius, y0);
	cairo_arc (cr, x1 - radius, y0 + radius, radius, 3 * M_PI / 2,
		   2 * M_PI);
	cairo_line_to (cr, x1, y1 - radius);
	cairo_arc (cr, x1 - radius, y1 - radius, radius, 0, M_PI / 2);
	cairo_line_to (cr, x0 + radius, y1);
	cairo_arc (cr, x0 + radius, y1 - radius, radius, M_PI / 2, M_PI);

	cairo_close_path (cr);
}

static void
draw_curve_rectangle (cairo_t  *cr,
		      gboolean  filled,
		      GdkRGBA  *color,
		      gint      x,
                      gint      y,
                      gint      width,
                      gint      height,
                      gint      radius)
{
	curve_rectangle (cr, x, y, width, height, radius);

	gdk_cairo_set_source_rgba (cr, color);

	if (filled)
		cairo_fill (cr);
	else {
		cairo_set_line_width (cr, CAIRO_LINE_WIDTH);
		cairo_stroke (cr);
	}
}

/* x, y, width, height are in the xkb coordinate system */
static void
draw_rectangle (GnomeKeyboardDrawingRenderContext *context,
		GdkRGBA                           *color,
		gint angle,
		gint xkb_x, gint xkb_y, gint xkb_width, gint xkb_height,
		gint radius)
{
	if (angle == 0) {
		gint x, y, width, height;
		gboolean filled;

		if (color) {
			filled = TRUE;
		} else {
			color = &context->dark_color;
			filled = FALSE;
		}

		x = xkb_to_pixmap_coord (context, xkb_x);
		y = xkb_to_pixmap_coord (context, xkb_y);
		width =
			xkb_to_pixmap_coord (context, xkb_x + xkb_width) - x;
		height =
			xkb_to_pixmap_coord (context, xkb_y + xkb_height) - y;

		draw_curve_rectangle (context->cr, filled, color,
				      x, y, width, height,
				      xkb_to_pixmap_double (context,
							    radius));
	} else {
		XkbPointRec points[4];
		gint x, y;

		points[0].x = xkb_x;
		points[0].y = xkb_y;
		rotate_coordinate (xkb_x, xkb_y, xkb_x + xkb_width, xkb_y,
				   angle, &x, &y);
		points[1].x = x;
		points[1].y = y;
		rotate_coordinate (xkb_x, xkb_y, xkb_x + xkb_width,
				   xkb_y + xkb_height, angle, &x, &y);
		points[2].x = x;
		points[2].y = y;
		rotate_coordinate (xkb_x, xkb_y, xkb_x, xkb_y + xkb_height,
				   angle, &x, &y);
		points[3].x = x;
		points[3].y = y;

		/* the points we've calculated are relative to 0,0 */
		draw_polygon (context, color, 0, 0, points, 4, radius);
	}
}

static void
draw_outline (GnomeKeyboardDrawingRenderContext *context,
	      XkbOutlineRec                     *outline,
	      GdkRGBA                           *color,
              gint                               angle,
              gint                               origin_x,
              gint                               origin_y)
{
#ifdef KBDRAW_DEBUG
	printf ("origin: %d, %d, num_points in %p: %d\n", origin_x,
		origin_y, outline, outline->num_points);
#endif

	if (outline->num_points == 1) {
#ifdef KBDRAW_DEBUG
		printf
			("1 point (rectangle): width, height: %d, %d, radius %d\n",
			 outline->points[0].x, outline->points[0].y,
			 outline->corner_radius);
#endif
		if (color)
			draw_rectangle (context, color, angle, origin_x,
					origin_y, outline->points[0].x,
					outline->points[0].y,
					outline->corner_radius);

		draw_rectangle (context, NULL, angle, origin_x,
				origin_y, outline->points[0].x,
				outline->points[0].y,
				outline->corner_radius);
	} else if (outline->num_points == 2) {
		gint rotated_x0, rotated_y0;

		rotate_coordinate (origin_x, origin_y,
				   origin_x + outline->points[0].x,
				   origin_y + outline->points[0].y,
				   angle, &rotated_x0, &rotated_y0);
#ifdef KBDRAW_DEBUG
		printf
			("2 points (rectangle): from %d, %d, width, height: %d, %d, radius %d\n",
			 rotated_x0, rotated_y0, outline->points[1].x,
			 outline->points[1].y, outline->corner_radius);
#endif
		if (color)
			draw_rectangle (context, color, angle, rotated_x0,
					rotated_y0, outline->points[1].x,
					outline->points[1].y,
					outline->corner_radius);
		draw_rectangle (context, NULL, angle, rotated_x0,
				rotated_y0, outline->points[1].x,
				outline->points[1].y,
				outline->corner_radius);
	} else {
#ifdef KBDRAW_DEBUG
		printf ("multiple points (%d) from %d %d, radius %d\n",
			outline->num_points, origin_x, origin_y,
			outline->corner_radius);
#endif
		if (color)
			draw_polygon (context, color, origin_x, origin_y,
				      outline->points, outline->num_points,
				      outline->corner_radius);
		draw_polygon (context, NULL, origin_x, origin_y,
			      outline->points, outline->num_points,
			      outline->corner_radius);
	}
}

/* see PSColorDef in xkbprint */
static gboolean
parse_xkb_color_spec (gchar *colorspec, GdkRGBA *color)
{
	glong level;

	color->alpha = 1;
	if (g_ascii_strcasecmp (colorspec, "black") == 0) {
		color->red = 0;
		color->green = 0;
		color->blue = 0;
	} else if (g_ascii_strcasecmp (colorspec, "white") == 0) {
		color->red = 1.0;
		color->green = 1.0;
		color->blue = 1.0;
	} else if (g_ascii_strncasecmp (colorspec, "grey", 4) == 0 ||
		   g_ascii_strncasecmp (colorspec, "gray", 4) == 0) {
		level = strtol (colorspec + 4, NULL, 10);

		color->red = 1.0 - level / 100.0;
		color->green = 1.0 - level / 100.0;
		color->blue = 1.0 - level / 100.0;
	} else if (g_ascii_strcasecmp (colorspec, "red") == 0) {
		color->red = 1.0;
		color->green = 0;
		color->blue = 0;
	} else if (g_ascii_strcasecmp (colorspec, "green") == 0) {
		color->red = 0;
		color->green = 1.0;
		color->blue = 0;
	} else if (g_ascii_strcasecmp (colorspec, "blue") == 0) {
		color->red = 0;
		color->green = 0;
		color->blue = 1.0;
	} else if (g_ascii_strncasecmp (colorspec, "red", 3) == 0) {
		level = strtol (colorspec + 3, NULL, 10);

		color->red = level / 100.0;
		color->green = 0;
		color->blue = 0;
	} else if (g_ascii_strncasecmp (colorspec, "green", 5) == 0) {
		level = strtol (colorspec + 5, NULL, 10);

		color->red = 0;
		color->green = level / 100.0;
		color->blue = 0;
	} else if (g_ascii_strncasecmp (colorspec, "blue", 4) == 0) {
		level = strtol (colorspec + 4, NULL, 10);

		color->red = 0;
		color->green = 0;
		color->blue = level / 100.0;
	} else
		return FALSE;

	return TRUE;
}


static guint
find_keycode (GnomeKeyboardDrawing *drawing, gchar *key_name)
{
#define KEYSYM_NAME_MAX_LENGTH 4
	guint keycode;
	gint i, j;
	XkbKeyNamePtr pkey;
	XkbKeyAliasPtr palias;
	guint is_name_matched;
	gchar *src, *dst;

	if (!drawing->xkb)
		return INVALID_KEYCODE;

#ifdef KBDRAW_DEBUG
	printf ("    looking for keycode for (%c%c%c%c)\n",
		key_name[0], key_name[1], key_name[2], key_name[3]);
#endif

	pkey = drawing->xkb->names->keys + drawing->xkb->min_key_code;
	for (keycode = drawing->xkb->min_key_code;
	     keycode <= drawing->xkb->max_key_code; keycode++) {
		is_name_matched = 1;
		src = key_name;
		dst = pkey->name;
		for (i = KEYSYM_NAME_MAX_LENGTH; --i >= 0;) {
			if ('\0' == *src)
				break;
			if (*src++ != *dst++) {
				is_name_matched = 0;
				break;
			}
		}
		if (is_name_matched) {
#ifdef KBDRAW_DEBUG
			printf ("      found keycode %u\n", keycode);
#endif
			return keycode;
		}
		pkey++;
	}

	palias = drawing->xkb->names->key_aliases;
	for (j = drawing->xkb->names->num_key_aliases; --j >= 0;) {
		is_name_matched = 1;
		src = key_name;
		dst = palias->alias;
		for (i = KEYSYM_NAME_MAX_LENGTH; --i >= 0;) {
			if ('\0' == *src)
				break;
			if (*src++ != *dst++) {
				is_name_matched = 0;
				break;
			}
		}

		if (is_name_matched) {
			keycode = find_keycode (drawing, palias->real);
#ifdef KBDRAW_DEBUG
			printf ("found alias keycode %u\n", keycode);
#endif
			return keycode;
		}
		palias++;
	}

	return INVALID_KEYCODE;
}

static void
set_markup (GnomeKeyboardDrawingRenderContext *context, gchar *txt)
{
	PangoLayout *layout = context->layout;
	txt = strcmp ("<", txt) ? txt : "&lt;";
	txt = strcmp ("&", txt) ? txt : "&amp;";
	if (g_utf8_strlen (txt, -1) > 1) {
		gchar *buf =
			g_strdup_printf ("<span size=\"x-small\">%s</span>",
					 txt);
		pango_layout_set_markup (layout, buf, -1);
		g_free (buf);
	} else {
		pango_layout_set_markup (layout, txt, -1);
	}
}

static void
set_key_label_in_layout (GnomeKeyboardDrawingRenderContext *context,
			 guint                              keyval)
{
	gchar buf[5];
	gunichar uc;

	switch (keyval) {
	case GDK_KEY_Scroll_Lock:
		set_markup (context, "Scroll\nLock");
		break;

	case GDK_KEY_space:
		set_markup (context, "");
		break;

	case GDK_KEY_Sys_Req:
		set_markup (context, "Sys Rq");
		break;

	case GDK_KEY_Page_Up:
		set_markup (context, "Page\nUp");
		break;

	case GDK_KEY_Page_Down:
		set_markup (context, "Page\nDown");
		break;

	case GDK_KEY_Num_Lock:
		set_markup (context, "Num\nLock");
		break;

	case GDK_KEY_KP_Page_Up:
		set_markup (context, "Pg Up");
		break;

	case GDK_KEY_KP_Page_Down:
		set_markup (context, "Pg Dn");
		break;

	case GDK_KEY_KP_Home:
		set_markup (context, "Home");
		break;

	case GDK_KEY_KP_Left:
		set_markup (context, "Left");
		break;

	case GDK_KEY_KP_End:
		set_markup (context, "End");
		break;

	case GDK_KEY_KP_Up:
		set_markup (context, "Up");
		break;

	case GDK_KEY_KP_Begin:
		set_markup (context, "Begin");
		break;

	case GDK_KEY_KP_Right:
		set_markup (context, "Right");
		break;

	case GDK_KEY_KP_Enter:
		set_markup (context, "Enter");
		break;

	case GDK_KEY_KP_Down:
		set_markup (context, "Down");
		break;

	case GDK_KEY_KP_Insert:
		set_markup (context, "Ins");
		break;

	case GDK_KEY_KP_Delete:
		set_markup (context, "Del");
		break;

	case GDK_KEY_dead_grave:
		set_markup (context, "ˋ");
		break;

	case GDK_KEY_dead_acute:
		set_markup (context, "ˊ");
		break;

	case GDK_KEY_dead_circumflex:
		set_markup (context, "ˆ");
		break;

	case GDK_KEY_dead_tilde:
		set_markup (context, "~");
		break;

	case GDK_KEY_dead_macron:
		set_markup (context, "ˉ");
		break;

	case GDK_KEY_dead_breve:
		set_markup (context, "˘");
		break;

	case GDK_KEY_dead_abovedot:
		set_markup (context, "˙");
		break;

	case GDK_KEY_dead_diaeresis:
		set_markup (context, "¨");
		break;

	case GDK_KEY_dead_abovering:
		set_markup (context, "˚");
		break;

	case GDK_KEY_dead_doubleacute:
		set_markup (context, "˝");
		break;

	case GDK_KEY_dead_caron:
		set_markup (context, "ˇ");
		break;

	case GDK_KEY_dead_cedilla:
		set_markup (context, "¸");
		break;

	case GDK_KEY_dead_ogonek:
		set_markup (context, "˛");
		break;

		/* case GDK_KEY_dead_iota:
		 * case GDK_KEY_dead_voiced_sound:
		 * case GDK_KEY_dead_semivoiced_sound: */

	case GDK_KEY_dead_belowdot:
		set_markup (context, " ̣");
		break;

	case GDK_KEY_horizconnector:
		set_markup (context, "horiz\nconn");
		break;

	case GDK_KEY_Mode_switch:
		set_markup (context, "AltGr");
		break;

	case GDK_KEY_Multi_key:
		set_markup (context, "Compose");
		break;

	case GDK_KEY_VoidSymbol:
		set_markup (context, "");
		break;

	default:
		uc = gdk_keyval_to_unicode (keyval);
		if (uc != 0 && g_unichar_isgraph (uc)) {
			buf[g_unichar_to_utf8 (uc, buf)] = '\0';
			set_markup (context, buf);
		} else {
			gchar *name = gdk_keyval_name (keyval);
			if (name) {
				gchar *fixed_name = g_strdup (name), *p;
				/* Replace underscores with spaces */
				for (p = fixed_name; *p; p++)
					if (*p == '_')
						*p = ' ';
				/* Get rid of scary ISO_ prefix */
				if (g_strstr_len (fixed_name, -1, "ISO "))
					set_markup (context,
						    fixed_name + 4);
				else
					set_markup (context, fixed_name);
				g_free (fixed_name);
			} else
				set_markup (context, "");
		}
	}
}


static void
draw_pango_layout (GnomeKeyboardDrawingRenderContext *context,
		   GnomeKeyboardDrawing              *drawing,
		   gint                               angle,
                   gint                               x,
                   gint                               y,
                   gboolean                           is_pressed)
{
	PangoLayout *layout = context->layout;
	GdkRGBA *pcolor, color;
	PangoLayoutLine *line;
	gint x_off, y_off;
	gint i;

	if (is_pressed) {
		GtkStyleContext *style_context =
			gtk_widget_get_style_context (GTK_WIDGET (drawing));
		pcolor = &color;
		gtk_style_context_get_color (style_context,
					     GTK_STATE_FLAG_SELECTED,
					     pcolor);
	} else {
		pcolor =
			drawing->colors + (drawing->xkb->geom->label_color -
					   drawing->xkb->geom->colors);
	}

	if (angle != context->angle) {
		PangoMatrix matrix = PANGO_MATRIX_INIT;
		pango_matrix_rotate (&matrix, -angle / 10.0);
		pango_context_set_matrix (pango_layout_get_context
					  (layout), &matrix);
		pango_layout_context_changed (layout);
		context->angle = angle;
	}

	i = 0;
	y_off = 0;
	for (line = pango_layout_get_line (layout, i);
	     line != NULL; line = pango_layout_get_line (layout, ++i)) {
		GSList *runp;
		PangoRectangle line_extents;

		x_off = 0;

		for (runp = line->runs; runp != NULL; runp = runp->next) {
			PangoGlyphItem *run = runp->data;
			gint j;

			for (j = 0; j < run->glyphs->num_glyphs; j++) {
				PangoGlyphGeometry *geometry;

				geometry =
					&run->glyphs->glyphs[j].geometry;

				x_off += geometry->width;
			}
		}

		pango_layout_line_get_extents (line, NULL, &line_extents);
		y_off +=
			line_extents.height +
			pango_layout_get_spacing (layout);
	}

	cairo_move_to (context->cr, x, y);
	gdk_cairo_set_source_rgba (context->cr, pcolor);
	pango_cairo_show_layout (context->cr, layout);
}

static void
draw_key_label_helper (GnomeKeyboardDrawingRenderContext      *context,
		       GnomeKeyboardDrawing                   *drawing,
		       KeySym                                  keysym,
		       gint                                    angle,
		       GnomeKeyboardDrawingGroupLevelPosition  glp,
		       gint                                    x,
		       gint                                    y,
                       gint                                    width,
                       gint                                    height,
                       gint                                    padding,
		       gboolean                                is_pressed)
{
	gint label_x, label_y, label_max_width, ycell;

	if (keysym == 0)
		return;
#ifdef KBDRAW_DEBUG
	printf ("keysym: %04X(%c) at glp: %d\n",
		(unsigned) keysym, (char) keysym, (int) glp);
#endif

	switch (glp) {
	case GNOME_KEYBOARD_DRAWING_POS_TOPLEFT:
	case GNOME_KEYBOARD_DRAWING_POS_BOTTOMLEFT:
		{
			ycell =
				glp == GNOME_KEYBOARD_DRAWING_POS_BOTTOMLEFT;

			rotate_coordinate (x, y, x + padding,
					   y + padding + (height -
							  2 * padding) *
					   ycell * 4 / 7, angle, &label_x,
					   &label_y);
			label_max_width =
				PANGO_SCALE * (width - 2 * padding);
			break;
		}
	case GNOME_KEYBOARD_DRAWING_POS_TOPRIGHT:
	case GNOME_KEYBOARD_DRAWING_POS_BOTTOMRIGHT:
		{
			ycell =
				glp == GNOME_KEYBOARD_DRAWING_POS_BOTTOMRIGHT;

			rotate_coordinate (x, y,
					   x + padding + (width -
							  2 * padding) *
					   4 / 7,
					   y + padding + (height -
							  2 * padding) *
					   ycell * 4 / 7, angle, &label_x,
					   &label_y);
			label_max_width =
				PANGO_SCALE * ((width - 2 * padding) -
					       (width - 2 * padding) * 4 / 7);
			break;
		}
	default:
		return;
	}
	set_key_label_in_layout (context, keysym);
	pango_layout_set_width (context->layout, label_max_width);
	label_y -= (pango_layout_get_line_count (context->layout) - 1) *
		(pango_font_description_get_size (context->font_desc) /
		 PANGO_SCALE);
	cairo_save (context->cr);
	cairo_rectangle (context->cr, x + padding / 2, y + padding / 2,
			 width - padding, height - padding);
	cairo_clip (context->cr);
	draw_pango_layout (context, drawing, angle, label_x, label_y,
			   is_pressed);
	cairo_restore (context->cr);
}

static void
draw_key_label (GnomeKeyboardDrawingRenderContext *context,
		GnomeKeyboardDrawing              *drawing,
		guint                              keycode,
		gint                               angle,
		gint                               xkb_origin_x,
		gint                               xkb_origin_y,
                gint                               xkb_width,
                gint                               xkb_height,
		gboolean                           is_pressed)
{
	gint x, y, width, height;
	gint padding;
	gint g, l, glp;

	if (!drawing->xkb)
		return;

	padding = 23 * context->scale_numerator / context->scale_denominator;	/* 2.3mm */

	x = xkb_to_pixmap_coord (context, xkb_origin_x);
	y = xkb_to_pixmap_coord (context, xkb_origin_y);
	width =
		xkb_to_pixmap_coord (context, xkb_origin_x + xkb_width) - x;
	height =
		xkb_to_pixmap_coord (context, xkb_origin_y + xkb_height) - y;

	for (glp = GNOME_KEYBOARD_DRAWING_POS_TOPLEFT;
	     glp < GNOME_KEYBOARD_DRAWING_POS_TOTAL; glp++) {
		KeySym keysym;

		if (groupLevels[glp] == NULL)
			continue;
		g = groupLevels[glp]->group;
		l = groupLevels[glp]->level;

		if (g < 0 || g >= XkbKeyNumGroups (drawing->xkb, keycode))
			continue;
		if (l < 0 || l >= XkbKeyGroupWidth (drawing->xkb, keycode, g))
			continue;

		/* Skip "exotic" levels like the "Ctrl" level in PC_SYSREQ */
		if (l > 0) {
			guint mods = XkbKeyKeyType (drawing->xkb, keycode, g)->mods.mask;
			if ((mods & (ShiftMask | drawing->l3mod)) == 0)
				continue;
		}

		keysym = XkbKeySymEntry (drawing->xkb, keycode, l, g);

		draw_key_label_helper (context, drawing, keysym,
				       angle, glp, x, y, width,
				       height, padding,
				       is_pressed);
	}
}

/*
 * The x offset is calculated for complex shapes. It is the rightmost of the vertical lines in the outline
 */
static gint
calc_shape_origin_offset_x (XkbOutlineRec *outline)
{
	gint rv = 0;
	gint i;
	XkbPointPtr point = outline->points;
	if (outline->num_points < 3)
		return 0;
	for (i = outline->num_points; --i > 0;) {
		gint x1 = point->x;
		gint y1 = point++->y;
		gint x2 = point->x;
		gint y2 = point->y;

		/*vertical, bottom to top (clock-wise), on the left */
		if ((x1 == x2) && (y1 > y2) && (x1 > rv)) {
			rv = x1;
		}
	}
	return rv;
}

/* groups are from 0-3 */
static void
draw_key (GnomeKeyboardDrawingRenderContext *context,
	  GnomeKeyboardDrawing              *drawing,
          GnomeKeyboardDrawingKey           *key)
{
	XkbShapeRec *shape;
	GdkRGBA *pcolor, color;
	XkbOutlineRec *outline;
	int origin_offset_x;
	/* gint i; */

	if (!drawing->xkb)
		return;

#ifdef KBDRAW_DEBUG
	printf ("shape: %p (base %p, index %d)\n",
		drawing->xkb->geom->shapes + key->xkbkey->shape_ndx,
		drawing->xkb->geom->shapes, key->xkbkey->shape_ndx);
#endif

	shape = drawing->xkb->geom->shapes + key->xkbkey->shape_ndx;

	if (key->pressed) {
		GtkStyleContext *style_context =
			gtk_widget_get_style_context (GTK_WIDGET (drawing));
		pcolor = &color;
		gtk_style_context_get_background_color (style_context,
							GTK_STATE_FLAG_SELECTED,
							pcolor);
	} else
		pcolor = drawing->colors + key->xkbkey->color_ndx;

#ifdef KBDRAW_DEBUG
	printf
		(" outlines base in the shape: %p (total: %d), origin: (%d, %d), angle %d, colored: %s\n",
		 shape->outlines, shape->num_outlines, key->origin_x,
		 key->origin_y, key->angle, pcolor ? "yes" : "no");
#endif

	/* draw the primary outline */
	outline = shape->primary ? shape->primary : shape->outlines;
	draw_outline (context, outline, pcolor, key->angle, key->origin_x,
		      key->origin_y);
#if 0
	/* don't draw other outlines for now, since
	 * the text placement does not take them into account 
	 */
	for (i = 0; i < shape->num_outlines; i++) {
		if (shape->outlines + i == shape->approx ||
		    shape->outlines + i == shape->primary)
			continue;
		draw_outline (context, shape->outlines + i, NULL,
			      key->angle, key->origin_x, key->origin_y);
	}
#endif
	origin_offset_x = calc_shape_origin_offset_x (outline);
	draw_key_label (context, drawing, key->keycode, key->angle,
			key->origin_x + origin_offset_x, key->origin_y,
			shape->bounds.x2, shape->bounds.y2, key->pressed);
}

static void
invalidate_region (GnomeKeyboardDrawing * drawing,
		   gdouble angle,
		   gint origin_x, gint origin_y, XkbShapeRec * shape)
{
	GdkPoint points[4];
	GtkAllocation alloc;
	gint x_min, x_max, y_min, y_max;
	gint x, y, width, height;
	gint xx, yy;

	rotate_coordinate (0, 0, 0, 0, angle, &xx, &yy);
	points[0].x = xx;
	points[0].y = yy;
	rotate_coordinate (0, 0, shape->bounds.x2, 0, angle, &xx, &yy);
	points[1].x = xx;
	points[1].y = yy;
	rotate_coordinate (0, 0, shape->bounds.x2, shape->bounds.y2, angle,
			   &xx, &yy);
	points[2].x = xx;
	points[2].y = yy;
	rotate_coordinate (0, 0, 0, shape->bounds.y2, angle, &xx, &yy);
	points[3].x = xx;
	points[3].y = yy;

	x_min =
		MIN (MIN (points[0].x, points[1].x),
		     MIN (points[2].x, points[3].x));
	x_max =
		MAX (MAX (points[0].x, points[1].x),
		     MAX (points[2].x, points[3].x));
	y_min =
		MIN (MIN (points[0].y, points[1].y),
		     MIN (points[2].y, points[3].y));
	y_max =
		MAX (MAX (points[0].y, points[1].y),
		     MAX (points[2].y, points[3].y));

	x = xkb_to_pixmap_coord (drawing->renderContext,
				 origin_x + x_min) - 6;
	y = xkb_to_pixmap_coord (drawing->renderContext,
				 origin_y + y_min) - 6;
	width =
		xkb_to_pixmap_coord (drawing->renderContext,
				     x_max - x_min) + 12;
	height =
		xkb_to_pixmap_coord (drawing->renderContext,
				     y_max - y_min) + 12;

	gtk_widget_get_allocation (GTK_WIDGET (drawing), &alloc);
	gtk_widget_queue_draw_area (GTK_WIDGET (drawing),
				    x + alloc.x, y + alloc.y,
				    width, height);
}

static void
invalidate_indicator_doodad_region (GnomeKeyboardDrawing * drawing,
				    GnomeKeyboardDrawingDoodad * doodad)
{
	if (!drawing->xkb)
		return;

	invalidate_region (drawing,
			   doodad->angle,
			   doodad->origin_x +
			   doodad->doodad->indicator.left,
			   doodad->origin_y +
			   doodad->doodad->indicator.top,
			   &drawing->xkb->geom->shapes[doodad->doodad->
						       indicator.
						       shape_ndx]);
}

static void
invalidate_key_region (GnomeKeyboardDrawing * drawing,
		       GnomeKeyboardDrawingKey * key)
{
	if (!drawing->xkb)
		return;

	invalidate_region (drawing,
			   key->angle,
			   key->origin_x,
			   key->origin_y,
			   &drawing->xkb->geom->shapes[key->xkbkey->
						       shape_ndx]);
}

static void
draw_text_doodad (GnomeKeyboardDrawingRenderContext * context,
		  GnomeKeyboardDrawing * drawing,
		  GnomeKeyboardDrawingDoodad * doodad,
		  XkbTextDoodadRec * text_doodad)
{
	gint x, y;
	if (!drawing->xkb)
		return;

	x = xkb_to_pixmap_coord (context,
				 doodad->origin_x + text_doodad->left);
	y = xkb_to_pixmap_coord (context,
				 doodad->origin_y + text_doodad->top);

	set_markup (context, text_doodad->text);
	draw_pango_layout (context, drawing, doodad->angle, x, y, FALSE);
}

static void
draw_indicator_doodad (GnomeKeyboardDrawingRenderContext * context,
		       GnomeKeyboardDrawing * drawing,
		       GnomeKeyboardDrawingDoodad * doodad,
		       XkbIndicatorDoodadRec * indicator_doodad)
{
	GdkRGBA *color;
	XkbShapeRec *shape;
	gint i;

	if (!drawing->xkb)
		return;

	shape = drawing->xkb->geom->shapes + indicator_doodad->shape_ndx;

	color = drawing->colors + (doodad->on ?
				   indicator_doodad->on_color_ndx :
				   indicator_doodad->off_color_ndx);

	for (i = 0; i < 1; i++)
		draw_outline (context, shape->outlines + i, color,
			      doodad->angle,
			      doodad->origin_x + indicator_doodad->left,
			      doodad->origin_y + indicator_doodad->top);
}

static void
draw_shape_doodad (GnomeKeyboardDrawingRenderContext * context,
		   GnomeKeyboardDrawing * drawing,
		   GnomeKeyboardDrawingDoodad * doodad,
		   XkbShapeDoodadRec * shape_doodad)
{
	XkbShapeRec *shape;
	GdkRGBA *color;
	gint i;

	if (!drawing->xkb)
		return;

	shape = drawing->xkb->geom->shapes + shape_doodad->shape_ndx;
	color = drawing->colors + shape_doodad->color_ndx;

	/* draw the primary outline filled */
	draw_outline (context,
		      shape->primary ? shape->primary : shape->outlines,
		      color, doodad->angle,
		      doodad->origin_x + shape_doodad->left,
		      doodad->origin_y + shape_doodad->top);

	/* stroke the other outlines */
	for (i = 0; i < shape->num_outlines; i++) {
		if (shape->outlines + i == shape->approx ||
		    shape->outlines + i == shape->primary)
			continue;
		draw_outline (context, shape->outlines + i, NULL,
			      doodad->angle,
			      doodad->origin_x + shape_doodad->left,
			      doodad->origin_y + shape_doodad->top);
	}
}

static void
draw_doodad (GnomeKeyboardDrawingRenderContext * context,
	     GnomeKeyboardDrawing * drawing,
	     GnomeKeyboardDrawingDoodad * doodad)
{
	switch (doodad->doodad->any.type) {
	case XkbOutlineDoodad:
	case XkbSolidDoodad:
		draw_shape_doodad (context, drawing, doodad,
				   &doodad->doodad->shape);
		break;

	case XkbTextDoodad:
		draw_text_doodad (context, drawing, doodad,
				  &doodad->doodad->text);
		break;

	case XkbIndicatorDoodad:
		draw_indicator_doodad (context, drawing, doodad,
				       &doodad->doodad->indicator);
		break;

	case XkbLogoDoodad:
		/* g_print ("draw_doodad: logo: %s\n", doodad->doodad->logo.logo_name); */
		/* XkbLogoDoodadRec is essentially a subclass of XkbShapeDoodadRec */
		draw_shape_doodad (context, drawing, doodad,
				   &doodad->doodad->shape);
		break;
	}
}

typedef struct {
	GnomeKeyboardDrawing *drawing;
	GnomeKeyboardDrawingRenderContext *context;
} DrawKeyboardItemData;

static void
draw_keyboard_item (GnomeKeyboardDrawingItem * item,
		    DrawKeyboardItemData * data)
{
	GnomeKeyboardDrawing *drawing = data->drawing;
	GnomeKeyboardDrawingRenderContext *context = data->context;

	if (!drawing->xkb)
		return;

	switch (item->type) {
	case GNOME_KEYBOARD_DRAWING_ITEM_TYPE_INVALID:
		break;

	case GNOME_KEYBOARD_DRAWING_ITEM_TYPE_KEY:
	case GNOME_KEYBOARD_DRAWING_ITEM_TYPE_KEY_EXTRA:
		draw_key (context, drawing,
			  (GnomeKeyboardDrawingKey *) item);
		break;

	case GNOME_KEYBOARD_DRAWING_ITEM_TYPE_DOODAD:
		draw_doodad (context, drawing,
			     (GnomeKeyboardDrawingDoodad *) item);
		break;
	}
}

static void
draw_keyboard_to_context (GnomeKeyboardDrawingRenderContext * context,
			  GnomeKeyboardDrawing * drawing)
{
	DrawKeyboardItemData data = { drawing, context };
	g_list_foreach (drawing->keyboard_items,
			(GFunc) draw_keyboard_item, &data);
}

static gboolean
prepare_cairo (GnomeKeyboardDrawing * drawing, cairo_t * cr)
{
	GtkStateFlags state;
	GtkStyleContext *style_context;
	if (drawing == NULL)
		return FALSE;

	style_context =
		gtk_widget_get_style_context (GTK_WIDGET (drawing));
	drawing->renderContext->cr = cr;
	state = gtk_widget_get_state_flags (GTK_WIDGET (drawing));
	gtk_style_context_get_background_color (style_context, state,
						&drawing->
						renderContext->dark_color);

	/* same approach as gtk - dark color = background color * 0.7 */
	drawing->renderContext->dark_color.red *= 0.7;
	drawing->renderContext->dark_color.green *= 0.7;
	drawing->renderContext->dark_color.blue *= 0.7;
	return TRUE;
}

static void
draw_keyboard (GnomeKeyboardDrawing * drawing, cairo_t * cr)
{
	GtkStateFlags state =
		gtk_widget_get_state_flags (GTK_WIDGET (drawing));
	GtkStyleContext *style_context =
		gtk_widget_get_style_context (GTK_WIDGET (drawing));
	GtkAllocation allocation;

	if (!drawing->xkb)
		return;

	gtk_widget_get_allocation (GTK_WIDGET (drawing), &allocation);

	if (prepare_cairo (drawing, cr)) {
		/* blank background */
		GdkRGBA color;
		gtk_style_context_get_background_color (style_context,
							state, &color);
		gdk_cairo_set_source_rgba (cr, &color);
		cairo_paint (cr);
#ifdef KBDRAW_DEBUG
		GdkRGBA yellow = { 1.0, 1.0, 0, 1.0 };
		gdk_cairo_set_source_rgba (cr, &yellow);

		cairo_move_to (cr, 0, 0);
		cairo_line_to (cr, allocation.width, 0);
		cairo_line_to (cr, allocation.width, allocation.height);
		cairo_line_to (cr, 0, allocation.height);
		cairo_close_path (cr);

		cairo_stroke (cr);
#endif

		draw_keyboard_to_context (drawing->renderContext, drawing);
	}
}

static void
alloc_render_context (GnomeKeyboardDrawing * drawing)
{
	GnomeKeyboardDrawingRenderContext *context =
		drawing->renderContext =
		g_new0 (GnomeKeyboardDrawingRenderContext, 1);

	PangoContext *pangoContext =
		gtk_widget_get_pango_context (GTK_WIDGET (drawing));
	context->layout = pango_layout_new (pangoContext);
	pango_layout_set_ellipsize (context->layout, PANGO_ELLIPSIZE_END);

	context->font_desc =
		pango_font_description_copy (gtk_widget_get_style
					     (GTK_WIDGET
					      (drawing))->font_desc);
	context->angle = 0;
	context->scale_numerator = 1;
	context->scale_denominator = 1;
}

static void
free_render_context (GnomeKeyboardDrawing * drawing)
{
	GnomeKeyboardDrawingRenderContext *context = drawing->renderContext;

	g_object_unref (context->layout);
	pango_font_description_free (context->font_desc);

	g_free (drawing->renderContext);
}

static gboolean
gnome_keyboard_drawing_draw (GtkWidget *widget,
			     cairo_t   *cr)
{
	GnomeKeyboardDrawing *drawing;

	drawing = GNOME_KEYBOARD_DRAWING (widget);

	if (!drawing->xkb)
		return FALSE;

	draw_keyboard (GNOME_KEYBOARD_DRAWING (drawing), cr);
	return FALSE;
}

static gboolean
context_setup_scaling (GnomeKeyboardDrawingRenderContext * context,
		       GnomeKeyboardDrawing * drawing,
		       gdouble width, gdouble height,
		       gdouble dpi_x, gdouble dpi_y)
{
	if (!drawing->xkb)
		return FALSE;

	if (drawing->xkb->geom->width_mm <= 0
	    || drawing->xkb->geom->height_mm <= 0) {
		g_critical
			("keyboard geometry reports width or height as zero!");
		return FALSE;
	}

	if (width * drawing->xkb->geom->height_mm <
	    height * drawing->xkb->geom->width_mm) {
		context->scale_numerator = width;
		context->scale_denominator = drawing->xkb->geom->width_mm;
	} else {
		context->scale_numerator = height;
		context->scale_denominator = drawing->xkb->geom->height_mm;
	}

	pango_font_description_set_size (context->font_desc,
					 72 * KEY_FONT_SIZE * dpi_x *
					 context->scale_numerator /
					 context->scale_denominator);
	pango_layout_set_spacing (context->layout,
				  -160 * dpi_y * context->scale_numerator /
				  context->scale_denominator);
	pango_layout_set_font_description (context->layout,
					   context->font_desc);

	return TRUE;
}

static void
gnome_keyboard_drawing_size_allocate (GtkWidget     *widget,
				      GtkAllocation *allocation)
{
	GnomeKeyboardDrawing *drawing;
	GnomeKeyboardDrawingRenderContext *context;

	drawing = GNOME_KEYBOARD_DRAWING (widget);
	context = drawing->renderContext;

	if (!context_setup_scaling (context, drawing,
				    allocation->width, allocation->height,
				    50, 50))
		return;

	gtk_widget_set_allocation (widget, allocation);
}

static gboolean
gnome_keyboard_drawing_key_event (GtkWidget   *widget,
				  GdkEventKey *event)
{
	GnomeKeyboardDrawing *drawing;
	GnomeKeyboardDrawingKey *key;

	drawing = GNOME_KEYBOARD_DRAWING (widget);

	if (!drawing->xkb)
		return FALSE;

	key = drawing->keys + event->hardware_keycode;

	if (event->hardware_keycode > drawing->xkb->max_key_code ||
	    event->hardware_keycode < drawing->xkb->min_key_code ||
	    key->xkbkey == NULL) {
		g_signal_emit (drawing,
			       gnome_keyboard_drawing_signals[BAD_KEYCODE],
			       0, event->hardware_keycode);
		return TRUE;
	}

	if ((event->type == GDK_KEY_PRESS && key->pressed) ||
	    (event->type == GDK_KEY_RELEASE && !key->pressed))
		return TRUE;
	/* otherwise this event changes the state we believed we had before */

	key->pressed = (event->type == GDK_KEY_PRESS);

	invalidate_key_region (drawing, key);
	return FALSE;
}

static gboolean
gnome_keyboard_drawing_button_press_event (GtkWidget      *widget,
					   GdkEventButton *event)
{
	GnomeKeyboardDrawing *drawing;

	drawing = GNOME_KEYBOARD_DRAWING (widget);

	if (!drawing->xkb)
		return FALSE;

	gtk_widget_grab_focus (widget);
	return FALSE;
}

static gboolean
unpress_keys (GnomeKeyboardDrawing * drawing)
{
	gint i;

	if (!drawing->xkb)
		return FALSE;

	for (i = drawing->xkb->min_key_code;
	     i <= drawing->xkb->max_key_code; i++)
		if (drawing->keys[i].pressed) {
			drawing->keys[i].pressed = FALSE;
			draw_key (drawing->renderContext, drawing,
				  drawing->keys + i);
			invalidate_key_region (drawing, drawing->keys + i);
		}

	return FALSE;
}

static gboolean
gnome_keyboard_drawing_focus_event (GtkWidget     *widget,
				    GdkEventFocus *event)
{
	GnomeKeyboardDrawing *drawing;

	drawing = GNOME_KEYBOARD_DRAWING (widget);

	if (event->in && drawing->timeout > 0) {
		g_source_remove (drawing->timeout);
		drawing->timeout = 0;
	} else
		drawing->timeout =
			g_timeout_add (120, (GSourceFunc) unpress_keys,
				       drawing);

	return FALSE;
}

static gint
compare_keyboard_item_priorities (GnomeKeyboardDrawingItem * a,
				  GnomeKeyboardDrawingItem * b)
{
	if (a->priority > b->priority)
		return 1;
	else if (a->priority < b->priority)
		return -1;
	else
		return 0;
}

static void
init_indicator_doodad (GnomeKeyboardDrawing * drawing,
		       XkbDoodadRec * xkbdoodad,
		       GnomeKeyboardDrawingDoodad * doodad)
{
	if (!drawing->xkb)
		return;

	if (xkbdoodad->any.type == XkbIndicatorDoodad) {
		gint index;
		Atom iname = 0;
		Atom sname = xkbdoodad->indicator.name;
		unsigned long phys_indicators =
			drawing->xkb->indicators->phys_indicators;
		Atom *pind = drawing->xkb->names->indicators;

#ifdef KBDRAW_DEBUG
		printf ("Looking for %d[%s]\n",
			(int) sname, XGetAtomName (drawing->display,
						   sname));
#endif

		for (index = 0; index < XkbNumIndicators; index++) {
			iname = *pind++;
			/* name matches and it is real */
			if (iname == sname
			    && (phys_indicators & (1 << index)))
				break;
			if (iname == 0)
				break;
		}
		if (iname == 0)
			g_warning ("Could not find indicator %d [%s]\n",
				   (int) sname,
				   XGetAtomName (drawing->display, sname));
		else {
#ifdef KBDRAW_DEBUG
			printf ("Found in xkbdesc as %d\n", index);
#endif
			drawing->physical_indicators[index] = doodad;
			/* Trying to obtain the real state, but if fail - just assume OFF */
			if (!XkbGetNamedIndicator
			    (drawing->display, sname, NULL, &doodad->on,
			     NULL, NULL))
				doodad->on = 0;
		}
	}
}

static void
init_keys_and_doodads (GnomeKeyboardDrawing * drawing)
{
	gint i, j, k;
	gint x, y;

	if (!drawing->xkb)
		return;

	for (i = 0; i < drawing->xkb->geom->num_doodads; i++) {
		XkbDoodadRec *xkbdoodad = drawing->xkb->geom->doodads + i;
		GnomeKeyboardDrawingDoodad *doodad =
			g_new (GnomeKeyboardDrawingDoodad, 1);

		doodad->type = GNOME_KEYBOARD_DRAWING_ITEM_TYPE_DOODAD;
		doodad->origin_x = 0;
		doodad->origin_y = 0;
		doodad->angle = 0;
		doodad->priority = xkbdoodad->any.priority * 256 * 256;
		doodad->doodad = xkbdoodad;

		init_indicator_doodad (drawing, xkbdoodad, doodad);

		drawing->keyboard_items =
			g_list_append (drawing->keyboard_items, doodad);
	}

	for (i = 0; i < drawing->xkb->geom->num_sections; i++) {
		XkbSectionRec *section = drawing->xkb->geom->sections + i;
		guint priority;

#ifdef KBDRAW_DEBUG
		printf ("initing section %d containing %d rows\n", i,
			section->num_rows);
#endif
		x = section->left;
		y = section->top;
		priority = section->priority * 256 * 256;

		for (j = 0; j < section->num_rows; j++) {
			XkbRowRec *row = section->rows + j;

#ifdef KBDRAW_DEBUG
			printf ("  initing row %d\n", j);
#endif
			x = section->left + row->left;
			y = section->top + row->top;

			for (k = 0; k < row->num_keys; k++) {
				XkbKeyRec *xkbkey = row->keys + k;
				GnomeKeyboardDrawingKey *key;
				XkbShapeRec *shape =
					drawing->xkb->geom->shapes +
					xkbkey->shape_ndx;
				guint keycode = find_keycode (drawing,
							      xkbkey->name.
							      name);

				if (keycode == INVALID_KEYCODE)
					continue;
#ifdef KBDRAW_DEBUG
				printf
					("    initing key %d, shape: %p(%p + %d), code: %u\n",
					 k, shape, drawing->xkb->geom->shapes,
					 xkbkey->shape_ndx, keycode);
#endif
				if (row->vertical)
					y += xkbkey->gap;
				else
					x += xkbkey->gap;

				if (keycode >= drawing->xkb->min_key_code
				    && keycode <=
				    drawing->xkb->max_key_code) {
					key = drawing->keys + keycode;
					if (key->type ==
					    GNOME_KEYBOARD_DRAWING_ITEM_TYPE_INVALID)
						{
							key->type =
								GNOME_KEYBOARD_DRAWING_ITEM_TYPE_KEY;
						} else {
						/* duplicate key for the same keycode, 
						   already defined as GNOME_KEYBOARD_DRAWING_ITEM_TYPE_KEY */
						key =
							g_new0
							(GnomeKeyboardDrawingKey,
							 1);
						key->type =
							GNOME_KEYBOARD_DRAWING_ITEM_TYPE_KEY_EXTRA;
					}
				} else {
					g_warning
						("key %4.4s: keycode = %u; not in range %d..%d\n",
						 xkbkey->name.name, keycode,
						 drawing->xkb->min_key_code,
						 drawing->xkb->max_key_code);

					key =
						g_new0 (GnomeKeyboardDrawingKey,
							1);
					key->type =
						GNOME_KEYBOARD_DRAWING_ITEM_TYPE_KEY_EXTRA;
				}

				key->xkbkey = xkbkey;
				key->angle = section->angle;
				rotate_coordinate (section->left,
						   section->top, x, y,
						   section->angle,
						   &key->origin_x,
						   &key->origin_y);
				key->priority = priority;
				key->keycode = keycode;

				drawing->keyboard_items =
					g_list_append (drawing->keyboard_items,
						       key);

				if (row->vertical)
					y += shape->bounds.y2;
				else
					x += shape->bounds.x2;

				priority++;
			}
		}

		for (j = 0; j < section->num_doodads; j++) {
			XkbDoodadRec *xkbdoodad = section->doodads + j;
			GnomeKeyboardDrawingDoodad *doodad =
				g_new (GnomeKeyboardDrawingDoodad, 1);

			doodad->type =
				GNOME_KEYBOARD_DRAWING_ITEM_TYPE_DOODAD;
			doodad->origin_x = x;
			doodad->origin_y = y;
			doodad->angle = section->angle;
			doodad->priority =
				priority + xkbdoodad->any.priority;
			doodad->doodad = xkbdoodad;

			init_indicator_doodad (drawing, xkbdoodad, doodad);

			drawing->keyboard_items =
				g_list_append (drawing->keyboard_items,
					       doodad);
		}
	}

	drawing->keyboard_items = g_list_sort (drawing->keyboard_items,
					       (GCompareFunc)
					       compare_keyboard_item_priorities);
}

static void
init_colors (GnomeKeyboardDrawing * drawing)
{
	gboolean result;
	gint i;

	if (!drawing->xkb)
		return;

	drawing->colors = g_new (GdkRGBA, drawing->xkb->geom->num_colors);

	for (i = 0; i < drawing->xkb->geom->num_colors; i++) {
		result =
			parse_xkb_color_spec (drawing->xkb->geom->colors[i].
					      spec, drawing->colors + i);

		if (!result)
			g_warning
				("init_colors: unable to parse color %s\n",
				 drawing->xkb->geom->colors[i].spec);
	}
}

static void
free_cdik (			/*colors doodads indicators keys */
	   GnomeKeyboardDrawing * drawing)
{
	GList *itemp;

	if (!drawing->xkb)
		return;

	for (itemp = drawing->keyboard_items; itemp; itemp = itemp->next) {
		GnomeKeyboardDrawingItem *item = itemp->data;

		switch (item->type) {
		case GNOME_KEYBOARD_DRAWING_ITEM_TYPE_INVALID:
		case GNOME_KEYBOARD_DRAWING_ITEM_TYPE_KEY:
			break;

		case GNOME_KEYBOARD_DRAWING_ITEM_TYPE_KEY_EXTRA:
		case GNOME_KEYBOARD_DRAWING_ITEM_TYPE_DOODAD:
			g_free (item);
			break;
		}
	}

	g_list_free (drawing->keyboard_items);
	drawing->keyboard_items = NULL;

	g_free (drawing->keys);
	g_free (drawing->colors);
}

static void
alloc_cdik (GnomeKeyboardDrawing * drawing)
{
	drawing->physical_indicators_size =
		drawing->xkb->indicators->phys_indicators + 1;
	drawing->physical_indicators =
		g_new0 (GnomeKeyboardDrawingDoodad *,
			drawing->physical_indicators_size);
	drawing->keys =
		g_new0 (GnomeKeyboardDrawingKey,
			drawing->xkb->max_key_code + 1);
}

static void
process_indicators_state_notify (XkbIndicatorNotifyEvent * iev,
				 GnomeKeyboardDrawing * drawing)
{
	/* Good question: should we track indicators when the keyboard is
	   NOT really taken from the screen */
	gint i;

	for (i = 0; i <= drawing->xkb->indicators->phys_indicators; i++) {
		if (drawing->physical_indicators[i] != NULL
		    && (iev->changed & 1 << i)) {
			gint state = (iev->state & 1 << i) != FALSE;

			if ((state && !drawing->physical_indicators[i]->on)
			    || (!state && drawing->physical_indicators[i]->on)) {
				drawing->physical_indicators[i]->on = state;
				invalidate_indicator_doodad_region (drawing,
								    drawing->physical_indicators[i]);
			}
		}
	}
}

static GdkFilterReturn
xkb_state_notify_event_filter (GdkXEvent            *gdkxev,
			       GdkEvent             *event,
			       GnomeKeyboardDrawing *drawing)
{
#define group_change_mask (XkbGroupStateMask | XkbGroupBaseMask | XkbGroupLatchMask | XkbGroupLockMask)
#define modifier_change_mask (XkbModifierStateMask | XkbModifierBaseMask | XkbModifierLatchMask | XkbModifierLockMask)

	if (!drawing->xkb)
		return GDK_FILTER_CONTINUE;

	if (((XEvent *) gdkxev)->type == drawing->xkb_event_type) {
		XkbEvent *kev = (XkbEvent *) gdkxev;

		if (kev->any.xkb_type == XkbIndicatorStateNotify) {
			process_indicators_state_notify (&kev->indicators,
							 drawing);
		}
	}

	return GDK_FILTER_CONTINUE;
}

static void
gnome_keyboard_drawing_dispose (GObject *object)
{
	GnomeKeyboardDrawing *drawing;

	drawing = GNOME_KEYBOARD_DRAWING (object);

	if (drawing->renderContext) {
		free_render_context (drawing);
		drawing->renderContext = NULL;
	}

	gdk_window_remove_filter (NULL, (GdkFilterFunc)
				  xkb_state_notify_event_filter, drawing);

	if (drawing->timeout > 0) {
		g_source_remove (drawing->timeout);
		drawing->timeout = 0;
	}

        G_OBJECT_CLASS (gnome_keyboard_drawing_parent_class)->dispose (object);
}

static void
gnome_keyboard_drawing_style_updated (GtkWidget *widget)
{
	GnomeKeyboardDrawing *drawing;

	drawing = GNOME_KEYBOARD_DRAWING (widget);

	pango_layout_context_changed (drawing->renderContext->layout);
}

static void
gnome_keyboard_drawing_init (GnomeKeyboardDrawing * drawing)
{
	gint opcode = 0, error = 0, major = 1, minor = 0;
	gint mask;

	drawing->display =
		GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

	if (!XkbQueryExtension
	    (drawing->display, &opcode, &drawing->xkb_event_type, &error,
	     &major, &minor))
		g_critical
			("XkbQueryExtension failed! Stuff probably won't work.");

	/* XXX: this stuff probably doesn't matter.. also, gdk_screen_get_default can fail */
	if (gtk_widget_has_screen (GTK_WIDGET (drawing)))
		drawing->screen_num =
			gdk_screen_get_number (gtk_widget_get_screen
					       (GTK_WIDGET (drawing)));
	else
		drawing->screen_num =
			gdk_screen_get_number (gdk_screen_get_default ());

	alloc_render_context (drawing);

	drawing->keyboard_items = NULL;
	drawing->colors = NULL;

	gtk_widget_set_double_buffered (GTK_WIDGET (drawing), FALSE);
	gtk_widget_set_has_window (GTK_WIDGET (drawing), FALSE);

	/* XXX: XkbClientMapMask | XkbIndicatorMapMask | XkbNamesMask | XkbGeometryMask */
	drawing->xkb = XkbGetKeyboard (drawing->display,
				       XkbGBN_GeometryMask |
				       XkbGBN_KeyNamesMask |
				       XkbGBN_OtherNamesMask |
				       XkbGBN_SymbolsMask |
				       XkbGBN_IndicatorMapMask,
				       XkbUseCoreKbd);
	if (drawing->xkb == NULL) {
		g_critical
			("XkbGetKeyboard failed to get keyboard from the server!");
		return;
	}

	XkbGetNames (drawing->display, XkbAllNamesMask, drawing->xkb);
	drawing->l3mod = XkbKeysymToModifiers (drawing->display,
					       GDK_KEY_ISO_Level3_Shift);

	drawing->xkbOnDisplay = TRUE;

	alloc_cdik (drawing);

	XkbSelectEventDetails (drawing->display, XkbUseCoreKbd,
			       XkbIndicatorStateNotify,
			       drawing->xkb->indicators->phys_indicators,
			       drawing->xkb->indicators->phys_indicators);

	mask =
		(XkbStateNotifyMask | XkbNamesNotifyMask |
		 XkbControlsNotifyMask | XkbIndicatorMapNotifyMask |
		 XkbNewKeyboardNotifyMask);
	XkbSelectEvents (drawing->display, XkbUseCoreKbd, mask, mask);

	mask = XkbGroupStateMask | XkbModifierStateMask;
	XkbSelectEventDetails (drawing->display, XkbUseCoreKbd,
			       XkbStateNotify, mask, mask);

	mask = (XkbGroupNamesMask | XkbIndicatorNamesMask);
	XkbSelectEventDetails (drawing->display, XkbUseCoreKbd,
			       XkbNamesNotify, mask, mask);
	init_keys_and_doodads (drawing);
	init_colors (drawing);

	/* required to get key events */
	gtk_widget_set_can_focus (GTK_WIDGET (drawing), TRUE);

	gtk_widget_set_events (GTK_WIDGET (drawing),
			       GDK_EXPOSURE_MASK | GDK_KEY_PRESS_MASK |
			       GDK_KEY_RELEASE_MASK | GDK_BUTTON_PRESS_MASK
			       | GDK_FOCUS_CHANGE_MASK);

	gdk_window_add_filter (NULL, (GdkFilterFunc)
			       xkb_state_notify_event_filter, drawing);
}

GtkWidget *
gnome_keyboard_drawing_new (void)
{
	return g_object_new (GNOME_TYPE_KEYBOARD_DRAWING, NULL);
}

static GtkSizeRequestMode
gnome_keyboard_drawing_get_request_mode (GtkWidget *widget)
{
	return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
gnome_keyboard_drawing_get_preferred_width (GtkWidget *widget,
                                            gint      *minimum_width,
                                            gint      *natural_width)
{
	GdkRectangle rect;
	gint w, monitor;
	GdkDisplay *display = gtk_widget_get_display (widget);
	GdkDeviceManager *gdm = gdk_display_get_device_manager (display);
	GdkScreen *scr = NULL;
	GList *devices =
		gdk_device_manager_list_devices (gdm, GDK_SOURCE_KEYBOARD);

	if (g_list_length (devices) > 0) {
		gint x, y;
		GdkDevice *dev = GDK_DEVICE (devices->data);
		gdk_device_get_position (dev, &scr, &x, &y);
		monitor = gdk_screen_get_monitor_at_point (scr, x, y);
	} else {
		scr = gdk_screen_get_default ();
		monitor = gdk_screen_get_primary_monitor (scr);
	}

	gdk_screen_get_monitor_geometry (scr, monitor, &rect);
	w = rect.width;
	*minimum_width = *natural_width = w - (w >> 2);
}

static void
gnome_keyboard_drawing_get_preferred_height_for_width (GtkWidget *widget,
                                                       gint       width,
                                                       gint      *minimum_height,
                                                       gint      *natural_height)
{
	GnomeKeyboardDrawing *drawing = GNOME_KEYBOARD_DRAWING (widget);
	*minimum_height = *natural_height =
		width * drawing->xkb->geom->height_mm /
		drawing->xkb->geom->width_mm;
}

static void
gnome_keyboard_drawing_class_init (GnomeKeyboardDrawingClass * klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = gnome_keyboard_drawing_dispose;

	widget_class->get_preferred_height_for_width = gnome_keyboard_drawing_get_preferred_height_for_width;
	widget_class->get_preferred_width = gnome_keyboard_drawing_get_preferred_width;
	widget_class->get_request_mode = gnome_keyboard_drawing_get_request_mode;
	widget_class->size_allocate = gnome_keyboard_drawing_size_allocate;
	widget_class->style_updated = gnome_keyboard_drawing_style_updated;
	widget_class->draw = gnome_keyboard_drawing_draw;

	widget_class->key_press_event = gnome_keyboard_drawing_key_event;
	widget_class->key_release_event = gnome_keyboard_drawing_key_event;
	widget_class->button_press_event = gnome_keyboard_drawing_button_press_event;
	widget_class->focus_out_event = gnome_keyboard_drawing_focus_event;
	widget_class->focus_in_event = gnome_keyboard_drawing_focus_event;

	gnome_keyboard_drawing_signals[BAD_KEYCODE] =
		g_signal_new ("bad-keycode",
			      GNOME_TYPE_KEYBOARD_DRAWING,
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GnomeKeyboardDrawingClass,
					       bad_keycode),
			      NULL, NULL,
			      g_cclosure_marshal_generic,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_UINT);
}

static gboolean
gnome_keyboard_drawing_render (GnomeKeyboardDrawing *kbdrawing,
			       cairo_t              *cr,
			       PangoLayout          *layout,
			       double                x,
			       double                y,
			       double                width,
			       double                height,
			       double                dpi_x,
			       double                dpi_y)
{
	GtkStateFlags state =
		gtk_widget_get_state_flags (GTK_WIDGET (kbdrawing));
	GtkStyleContext *style_context =
		gtk_widget_get_style_context (GTK_WIDGET (kbdrawing));
	GnomeKeyboardDrawingRenderContext context = {
		cr,
		kbdrawing->renderContext->angle,
		layout,
		pango_font_description_copy (gtk_widget_get_style
					     (GTK_WIDGET
					      (kbdrawing))->font_desc),
		1, 1
	};

	gtk_style_context_get_background_color (style_context, state,
						&context.dark_color);

	if (!context_setup_scaling (&context, kbdrawing, width, height,
				    dpi_x, dpi_y))
		return FALSE;
	cairo_translate (cr, x, y);

	draw_keyboard_to_context (&context, kbdrawing);

	pango_font_description_free (context.font_desc);

	return TRUE;
}

static void
gnome_keyboard_drawing_set_keyboard (GnomeKeyboardDrawing * drawing,
                                     XkbComponentNamesRec * names)
{
	free_cdik (drawing);
	if (drawing->xkb)
		XkbFreeKeyboard (drawing->xkb, 0, TRUE);	/* free_all = TRUE */
	drawing->xkb = NULL;

	if (names) {
		drawing->xkb =
			XkbGetKeyboardByName (drawing->display, XkbUseCoreKbd,
					      names, 0,
					      XkbGBN_GeometryMask |
					      XkbGBN_KeyNamesMask |
					      XkbGBN_OtherNamesMask |
					      XkbGBN_ClientSymbolsMask |
					      XkbGBN_IndicatorMapMask, FALSE);
		drawing->xkbOnDisplay = FALSE;
	} else {
		drawing->xkb = XkbGetKeyboard (drawing->display,
					       XkbGBN_GeometryMask |
					       XkbGBN_KeyNamesMask |
					       XkbGBN_OtherNamesMask |
					       XkbGBN_SymbolsMask |
					       XkbGBN_IndicatorMapMask,
					       XkbUseCoreKbd);
		XkbGetNames (drawing->display, XkbAllNamesMask,
			     drawing->xkb);
		drawing->xkbOnDisplay = TRUE;
	}

	if (drawing->xkb == NULL)
		return;

	alloc_cdik (drawing);

	init_keys_and_doodads (drawing);
	init_colors (drawing);

	gtk_widget_queue_resize (GTK_WIDGET (drawing));
}

typedef struct {
	GnomeKeyboardDrawing *drawing;
	const gchar *description;
} XkbLayoutPreviewPrintData;

static void
gnome_keyboard_drawing_begin_print (GtkPrintOperation         *operation,
				    GtkPrintContext           *context,
				    XkbLayoutPreviewPrintData *data)
{
	/* We always print single-page documents */
	GtkPrintSettings *settings =
		gtk_print_operation_get_print_settings (operation);
	gtk_print_operation_set_n_pages (operation, 1);
	if (!gtk_print_settings_has_key
	    (settings, GTK_PRINT_SETTINGS_ORIENTATION))
		gtk_print_settings_set_orientation (settings,
						    GTK_PAGE_ORIENTATION_LANDSCAPE);
}

static void
gnome_keyboard_drawing_draw_page (GtkPrintOperation         *operation,
				  GtkPrintContext           *context,
				  gint                       page_nr,
				  XkbLayoutPreviewPrintData *data)
{
	cairo_t *cr = gtk_print_context_get_cairo_context (context);
	PangoLayout *layout =
		gtk_print_context_create_pango_layout (context);
	PangoFontDescription *desc =
		pango_font_description_from_string ("sans 8");
	gdouble width = gtk_print_context_get_width (context);
	gdouble height = gtk_print_context_get_height (context);
	gdouble dpi_x = gtk_print_context_get_dpi_x (context);
	gdouble dpi_y = gtk_print_context_get_dpi_y (context);
	gchar *header;

	gtk_print_operation_set_unit (operation, GTK_UNIT_PIXEL);

	header = g_strdup_printf
		(_("Keyboard layout \"%s\"\n"
		   "Copyright &#169; X.Org Foundation and "
		   "XKeyboardConfig contributors\n"
		   "For licensing see package metadata"), data->description);
	pango_layout_set_markup (layout, header, -1);
	pango_layout_set_font_description (layout, desc);
	pango_font_description_free (desc);
	pango_layout_set_width (layout, pango_units_from_double (width));
	pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);
	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_move_to (cr, 0, 0);
	pango_cairo_show_layout (cr, layout);

	gnome_keyboard_drawing_render (data->drawing, cr, layout, 0.0,
				       0.0, width, height, dpi_x, dpi_y);

	g_object_unref (layout);
}

void
gnome_keyboard_drawing_print (GnomeKeyboardDrawing *drawing,
			      GtkWindow            *parent_window,
			      const gchar          *description)
{
	GtkPrintOperation *print;
	GtkPrintOperationResult res;
	static GtkPrintSettings *settings = NULL;
	XkbLayoutPreviewPrintData data = { drawing, description };

	print = gtk_print_operation_new ();

	if (settings != NULL)
		gtk_print_operation_set_print_settings (print, settings);

	g_signal_connect (print, "begin_print",
			  G_CALLBACK (gnome_keyboard_drawing_begin_print),
			  &data);
	g_signal_connect (print, "draw_page",
			  G_CALLBACK (gnome_keyboard_drawing_draw_page),
			  &data);

	res = gtk_print_operation_run (print,
				       GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
				       parent_window, NULL);

	if (res == GTK_PRINT_OPERATION_RESULT_APPLY) {
		if (settings != NULL)
			g_object_unref (settings);
		settings = gtk_print_operation_get_print_settings (print);
		g_object_ref (settings);
	}

	g_object_unref (print);
}

static void
free_components (XkbComponentNamesRec *components)
{
	free (components->keymap);
	free (components->keycodes);
	free (components->types);
	free (components->compat);
	free (components->symbols);
	free (components->geometry);
}

void
gnome_keyboard_drawing_set_layout (GnomeKeyboardDrawing *drawing,
				   const char           *layout,
				   const char           *variant)
{
	XkbRF_VarDefsRec *var_defs;
        const char *locale;
	char *rules_file;
	XkbRF_RulesPtr rules;
	XkbComponentNamesRec component_names;

	gnome_xkb_info_get_var_defs (&rules_file, &var_defs);

	if (var_defs->layout)
	        free (var_defs->layout);
	if (var_defs->variant)
		free (var_defs->variant);

	var_defs->layout = strdup (layout);
	var_defs->variant = strdup (variant);

        locale = setlocale (LC_ALL, NULL);
	rules = XkbRF_Load (rules_file, (char*) locale, True, True);

	if (!rules) {
		g_warning ("Failed to load XKB rules file %s", rules_file);

		goto out;
	}

	XkbRF_GetComponents (rules, var_defs, &component_names);

	gnome_keyboard_drawing_set_keyboard (drawing, &component_names);

 out:
	g_free (rules_file);
	gnome_xkb_info_free_var_defs (var_defs);
	free_components (&component_names);
}
