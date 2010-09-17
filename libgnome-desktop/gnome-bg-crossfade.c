/* gnome-bg-crossfade.h - fade window background between two surfaces
 *
 * Copyright (C) 2008 Red Hat, Inc.
 * Copyright © 2010 Christian Persch
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Author: Ray Strode <rstrode@redhat.com>
*/

#include <config.h>

#include <string.h>
#include <math.h>
#include <stdarg.h>

#include <gio/gio.h>

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <gtk/gtk.h>

#include <cairo.h>
#include <cairo-xlib.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnomeui/gnome-bg.h>
#include "libgnomeui/gnome-bg-crossfade.h"

struct _GnomeBGCrossfadePrivate
{
	GdkWindow *window;
        cairo_surface_t *fading_surface;
        cairo_surface_t *end_surface;
	gdouble    start_time;
	gdouble    total_duration;
	guint      timeout_id;
	guint      is_first_frame : 1;
};

enum {
	PROP_0,
	PROP_WINDOW
};

enum {
	FINISHED,
	NUMBER_OF_SIGNALS
};

static guint signals[NUMBER_OF_SIGNALS] = { 0 };

G_DEFINE_TYPE (GnomeBGCrossfade, gnome_bg_crossfade, G_TYPE_OBJECT)
#define GNOME_BG_CROSSFADE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o),\
			                   GNOME_TYPE_BG_CROSSFADE,\
			                   GnomeBGCrossfadePrivate))

static void
gnome_bg_crossfade_set_property (GObject      *object,
				 guint         property_id,
				 const GValue *value,
				 GParamSpec   *pspec)
{
	GnomeBGCrossfade *fade = GNOME_BG_CROSSFADE (object);

	switch (property_id)
	{
        case PROP_WINDOW:
                fade->priv->window = g_value_get_object (value);
                g_assert (GDK_IS_WINDOW (fade->priv->window) && GDK_WINDOW_TYPE (fade->priv->window) != GDK_WINDOW_FOREIGN);
                break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
gnome_bg_crossfade_get_property (GObject    *object,
			     guint       property_id,
			     GValue     *value,
			     GParamSpec *pspec)
{
        GnomeBGCrossfade *fade = GNOME_BG_CROSSFADE (object);

	switch (property_id)
	{
        case PROP_WINDOW:
                g_value_set_object (value, fade->priv->window);
                break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
gnome_bg_crossfade_finalize (GObject *object)
{
	GnomeBGCrossfade *fade;

	fade = GNOME_BG_CROSSFADE (object);

	gnome_bg_crossfade_stop (fade);

	if (fade->priv->fading_surface != NULL) {
		cairo_surface_destroy (fade->priv->fading_surface);
		fade->priv->fading_surface = NULL;
	}

	if (fade->priv->end_surface != NULL) {
		cairo_surface_destroy (fade->priv->end_surface);
		fade->priv->end_surface = NULL;
	}
}

static void
gnome_bg_crossfade_class_init (GnomeBGCrossfadeClass *fade_class)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS (fade_class);

	gobject_class->get_property = gnome_bg_crossfade_get_property;
	gobject_class->set_property = gnome_bg_crossfade_set_property;
	gobject_class->finalize = gnome_bg_crossfade_finalize;

        /**
         * GnomeBGCrossfade:window:
         *
         * The #GdkWindow the crossfade is shown on.
         */
        g_object_class_install_property (gobject_class,
                                         PROP_WINDOW,
                                         g_param_spec_object ("window",
                                                              "Window",
                                                              "Window",
                                                              GDK_TYPE_WINDOW,
                                                              G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

	/**
	 * GnomeBGCrossfade::finished:
	 * @fade: the #GnomeBGCrossfade that received the signal
	 *
	 * When a crossfade finishes, @window will have a copy
	 * of the end pixmap as its background, and this signal will
	 * get emitted.
	 */
	signals[FINISHED] = g_signal_new ("finished",
					  G_OBJECT_CLASS_TYPE (gobject_class),
					  G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					  g_cclosure_marshal_VOID__VOID,
					  G_TYPE_NONE, 0);

	g_type_class_add_private (gobject_class, sizeof (GnomeBGCrossfadePrivate));
}

static void
gnome_bg_crossfade_init (GnomeBGCrossfade *fade)
{
	fade->priv = GNOME_BG_CROSSFADE_GET_PRIVATE (fade);

	fade->priv->fading_surface = NULL;
	fade->priv->end_surface = NULL;
	fade->priv->timeout_id = 0;
}

/**
 * gnome_bg_crossfade_new:
 * @window: a #GdkWindow
 *
 * Creates a new object to manage crossfading a
 * window background between two cairo surfaces.
 *
 * Return value: the new #GnomeBGCrossfade
 **/
GnomeBGCrossfade *
gnome_bg_crossfade_new (GdkWindow *window)
{
        g_return_val_if_fail (window == NULL || (GDK_IS_WINDOW (window) && GDK_WINDOW_TYPE (window) != GDK_WINDOW_FOREIGN), NULL);

        return g_object_new (GNOME_TYPE_BG_CROSSFADE,
                             "window", window,
                             NULL);
}

GdkWindow *
gnome_bg_crossfade_get_window (GnomeBGCrossfade *fade)
{
        g_return_val_if_fail (GNOME_IS_BG_CROSSFADE (fade), NULL);

        return fade->priv->window;
}

static void
clear_surface (cairo_surface_t **surface)
{
        if (*surface)
                cairo_surface_destroy (*surface);
        *surface = NULL;
}

static cairo_surface_t *
tile_surface (GnomeBGCrossfade *fade,
              cairo_surface_t *surface)
{
        GnomeBGCrossfadePrivate *priv = fade->priv;
        cairo_surface_t *copy;
        cairo_t *cr;
        int width, height;

        width = gdk_window_get_width (fade->priv->window);
        height = gdk_window_get_height (fade->priv->window);

        copy = gdk_window_create_similar_surface (priv->window,
                                                  surface ? cairo_surface_get_content (surface)
                                                          : CAIRO_CONTENT_COLOR,
                                                  width, height);
        cr = cairo_create (copy);

        if (surface) {
                cairo_pattern_t *pattern;
                cairo_set_source_surface (cr, surface, 0., 0.);
                pattern = cairo_get_source (cr);
                cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT);
	} else {
		GtkStyle *style;
		style = gtk_widget_get_default_style ();
		gdk_cairo_set_source_color (cr, &style->bg[GTK_STATE_NORMAL]);
	}

	cairo_paint (cr);

	if (cairo_status (cr) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy (copy);
		copy = NULL;
	}
	cairo_destroy (cr);

	return copy;
}

/**
 * gnome_bg_crossfade_set_start_surface:
 * @fade: a #GnomeBGCrossfade
 * @surface: a #cairo_surface_t to fade from
 *
 * Before initiating a crossfade with gnome_bg_crossfade_start()
 * start and end surfaces have to be set.  This function sets
 * the surface shown at the beginning of the crossfade effect.
 **/
void
gnome_bg_crossfade_set_start_surface (GnomeBGCrossfade *fade,
				      cairo_surface_t  *surface)
{
	g_return_if_fail (GNOME_IS_BG_CROSSFADE (fade));
        g_return_if_fail (fade->priv->window != NULL);

        clear_surface (&fade->priv->fading_surface);
        fade->priv->fading_surface = tile_surface (fade, surface);
}

static gdouble
get_current_time (void)
{
	const double microseconds_per_second = (double) G_USEC_PER_SEC;
	double timestamp;
	GTimeVal now;

	g_get_current_time (&now);

	timestamp = ((microseconds_per_second * now.tv_sec) + now.tv_usec) /
	            microseconds_per_second;

	return timestamp;
}

/**
 * gnome_bg_crossfade_set_end_surface:
 * @fade: a #GnomeBGCrossfade
 * @surface: a #cairo_surface_t to fade to
 *
 * Before initiating a crossfade with gnome_bg_crossfade_start()
 * start and end surfaces have to be set.  This function sets
 * the surface shown at the end of the crossfade effect.
 **/
void
gnome_bg_crossfade_set_end_surface (GnomeBGCrossfade *fade,
				    cairo_surface_t  *surface)
{
	g_return_if_fail (GNOME_IS_BG_CROSSFADE (fade));
        g_return_if_fail (fade->priv->window != NULL);

        clear_surface (&fade->priv->end_surface);
        fade->priv->end_surface = tile_surface (fade, surface);

        /* Reset timer in case we're called while animating
	 */
	fade->priv->start_time = get_current_time ();
}

static gboolean
animations_are_disabled (GnomeBGCrossfade *fade)
{
	GtkSettings *settings;
	GdkScreen *screen;
	gboolean are_enabled;

	g_assert (fade->priv->window != NULL);

	screen = gdk_window_get_screen (fade->priv->window);

	settings = gtk_settings_get_for_screen (screen);

	g_object_get (settings, "gtk-enable-animations", &are_enabled, NULL);

	return !are_enabled;
}

static void
update_background (GnomeBGCrossfade *fade)
{
        cairo_pattern_t *pattern;

        pattern = cairo_pattern_create_for_surface (fade->priv->fading_surface);
        gdk_window_set_background_pattern (fade->priv->window, pattern);
        cairo_pattern_destroy (pattern);

	if (GDK_WINDOW_TYPE (fade->priv->window) == GDK_WINDOW_ROOT) {
		GdkDisplay *display;
		display = gdk_window_get_display (fade->priv->window);
		/* FIXMEchpe: gdk_window_clear (fade->priv->window); */
		gdk_flush ();
	} else {
		gdk_window_invalidate_rect (fade->priv->window, NULL, FALSE);
		gdk_window_process_updates (fade->priv->window, FALSE);
	}
}

static gboolean
on_tick (GnomeBGCrossfade *fade)
{
	gdouble now, percent_done;
	cairo_t *cr;
	cairo_status_t status;

	g_return_val_if_fail (GNOME_IS_BG_CROSSFADE (fade), FALSE);

	now = get_current_time ();

	percent_done = (now - fade->priv->start_time) / fade->priv->total_duration;
	percent_done = CLAMP (percent_done, 0.0, 1.0);

	/* If it's taking a long time to get to the first frame,
	 * then lengthen the duration, so the user will get to see
	 * the effect.
	 */
	if (fade->priv->is_first_frame && percent_done > .33) {
		fade->priv->is_first_frame = FALSE;
		fade->priv->total_duration *= 1.5;
		return on_tick (fade);
	}

	if (fade->priv->fading_surface == NULL) {
		return FALSE;
	}

	if (animations_are_disabled (fade)) {
		return FALSE;
	}

	/* We accumulate the results in place for performance reasons.
	 *
	 * This means 1) The fade is exponential, not linear (looks good!)
	 * 2) The rate of fade is not independent of frame rate. Slower machines
	 * will get a slower fade (but never longer than .75 seconds), and
	 * even the fastest machines will get *some* fade because the framerate
	 * is capped.
	 */
	cr = cairo_create (fade->priv->fading_surface);

        cairo_set_source_surface (cr,fade->priv->end_surface, 0., 0.);
	cairo_paint_with_alpha (cr, percent_done);

	status = cairo_status (cr);
	cairo_destroy (cr);

	if (status == CAIRO_STATUS_SUCCESS) {
		update_background (fade);
	}
	return percent_done <= .99;
}

static void
on_finished (GnomeBGCrossfade *fade)
{
	if (fade->priv->timeout_id == 0)
		return;

	g_assert (fade->priv->end_surface != NULL);

	update_background (fade);

	clear_surface (&fade->priv->end_surface);
	g_assert (fade->priv->fading_surface != NULL);
        clear_surface (&fade->priv->fading_surface);

	fade->priv->timeout_id = 0;
	g_signal_emit (fade, signals[FINISHED], 0);
}

/**
 * gnome_bg_crossfade_start:
 * @fade: a #GnomeBGCrossfade
 *
 * This function initiates a quick crossfade between two surfaces on
 * the background of @window.  Before initiating the crossfade both
 * gnome_bg_crossfade_start() and gnome_bg_crossfade_end() need to
 * be called. If animations are disabled, the crossfade is skipped,
 * and the window background is set immediately to the end surface.
 **/
void
gnome_bg_crossfade_start (GnomeBGCrossfade *fade)
{
	GSource *source;
	GMainContext *context;

	g_return_if_fail (GNOME_IS_BG_CROSSFADE (fade));
	g_return_if_fail (fade->priv->window != NULL);
	g_return_if_fail (fade->priv->fading_surface != NULL);
	g_return_if_fail (fade->priv->end_surface != NULL);
	g_return_if_fail (!gnome_bg_crossfade_is_started (fade));

        /* FIXME shouldn't this use gdk_threads_add_timout_full ? */
	source = g_timeout_source_new (1000 / 60.0);
	g_source_set_callback (source,
			       (GSourceFunc) on_tick,
			       fade,
			       (GDestroyNotify) on_finished);
	context = g_main_context_default ();
	fade->priv->timeout_id = g_source_attach (source, context);
	g_source_unref (source);

        update_background (fade);

	fade->priv->is_first_frame = TRUE;
	fade->priv->total_duration = .75;
	fade->priv->start_time = get_current_time ();
}

/**
 * gnome_bg_crossfade_is_started:
 * @fade: a #GnomeBGCrossfade
 *
 * This function reveals whether or not @fade is currently
 * running on a window.  See gnome_bg_crossfade_start() for
 * information on how to initiate a crossfade.
 *
 * Return value: %TRUE if fading, or %FALSE if not fading
 **/
gboolean
gnome_bg_crossfade_is_started (GnomeBGCrossfade *fade)
{
	g_return_val_if_fail (GNOME_IS_BG_CROSSFADE (fade), FALSE);

	return fade->priv->timeout_id != 0;
}

/**
 * gnome_bg_crossfade_stop:
 * @fade: a #GnomeBGCrossfade
 *
 * This function stops any in progress crossfades that may be
 * happening.  It's harmless to call this function if @fade is
 * already stopped.
 **/
void
gnome_bg_crossfade_stop (GnomeBGCrossfade *fade)
{
	g_return_if_fail (GNOME_IS_BG_CROSSFADE (fade));

	if (!gnome_bg_crossfade_is_started (fade))
		return;

	g_assert (fade->priv->timeout_id != 0);
	g_source_remove (fade->priv->timeout_id);
	fade->priv->timeout_id = 0;
}
