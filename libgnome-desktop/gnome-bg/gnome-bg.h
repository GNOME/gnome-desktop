/* gnome-bg.h: Desktop background API
 *
 * SPDX-FileCopyrightText: 2007, Red Hat, Inc.
 * SPDX-License-Identifier: LGPL-2.0-or-later
 *
 * Author: Soren Sandmann <sandmann@redhat.com>
 */

#pragma once

#ifndef GNOME_DESKTOP_USE_UNSTABLE_API
# error GnomeBG is unstable API. You must define GNOME_DESKTOP_USE_UNSTABLE_API before including gnome-bg.h
#endif

#include <gdk/gdk.h>
#include <gio/gio.h>
#include <gdesktop-enums.h>
#include <libgnome-desktop/gnome-desktop-thumbnail.h>
#include <gdesktop-enums.h>

G_BEGIN_DECLS

#define GNOME_TYPE_BG                   (gnome_bg_get_type ())
#define GNOME_BG(obj)                   (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_BG, GnomeBG))
#define GNOME_BG_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST ((klass),  GNOME_TYPE_BG, GnomeBGClass))
#define GNOME_IS_BG(obj)                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_TYPE_BG))
#define GNOME_IS_BG_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass),  GNOME_TYPE_BG))
#define GNOME_BG_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj),  GNOME_TYPE_BG, GnomeBGClass))

typedef struct _GnomeBG         GnomeBG;
typedef struct _GnomeBGClass    GnomeBGClass;

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GnomeBG, g_object_unref)

GType            gnome_bg_get_type              (void);
GnomeBG *        gnome_bg_new                   (void);
void             gnome_bg_load_from_preferences (GnomeBG               *bg,
						 GSettings             *settings);
void             gnome_bg_save_to_preferences   (GnomeBG               *bg,
						 GSettings             *settings);
/* Setters */
void             gnome_bg_set_filename          (GnomeBG               *bg,
						 const char            *filename);
void             gnome_bg_set_placement         (GnomeBG               *bg,
						 GDesktopBackgroundStyle placement);
void             gnome_bg_set_rgba              (GnomeBG               *bg,
						 GDesktopBackgroundShading type,
						 GdkRGBA               *primary,
						 GdkRGBA               *secondary);

/* Getters */
GDesktopBackgroundStyle gnome_bg_get_placement  (GnomeBG               *bg);
void		 gnome_bg_get_rgba              (GnomeBG               *bg,
						 GDesktopBackgroundShading *type,
						 GdkRGBA               *primary,
						 GdkRGBA               *secondary);
const gchar *    gnome_bg_get_filename          (GnomeBG               *bg);

/* Drawing and thumbnailing */
void             gnome_bg_draw                  (GnomeBG               *bg,
						 GdkPixbuf             *dest);
cairo_surface_t *gnome_bg_create_surface        (GnomeBG               *bg,
						 GdkSurface            *window,
						 int                    width,
						 int                    height);
gboolean         gnome_bg_get_image_size        (GnomeBG               *bg,
						 GnomeDesktopThumbnailFactory *factory,
                                                 int                    best_width,
                                                 int                    best_height,
						 int                   *width,
						 int                   *height);
GdkPixbuf *      gnome_bg_create_thumbnail      (GnomeBG               *bg,
						 GnomeDesktopThumbnailFactory *factory,
                                                 const cairo_rectangle_int_t  *screen_area,
						 int                    dest_width,
						 int                    dest_height);
gboolean         gnome_bg_is_dark               (GnomeBG               *bg,
                                                 int                    dest_width,
						 int                    dest_height);
gboolean         gnome_bg_has_multiple_sizes    (GnomeBG               *bg);
gboolean         gnome_bg_changes_with_time     (GnomeBG               *bg);
GdkPixbuf *      gnome_bg_create_frame_thumbnail (GnomeBG              *bg,
						 GnomeDesktopThumbnailFactory *factory,
                                                 const cairo_rectangle_int_t  *screen_area,
						 int                    dest_width,
						 int                    dest_height,
						 int                    frame_num);

G_END_DECLS
