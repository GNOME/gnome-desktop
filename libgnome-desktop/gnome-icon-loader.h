/* GnomeIconLoaders - a loader for icon-themes
 * gnome-icon-loader.h Copyright (C) 2002 Red Hat, Inc.
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

#ifndef GNOME_ICON_LOADER_H
#define GNOME_ICON_LOADER_H

#include <glib-object.h>

#define GNOME_TYPE_ICON_LOADER             (gnome_icon_loader_get_type ())
#define GNOME_ICON_LOADER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_ICON_LOADER, GnomeIconLoader))
#define GNOME_ICON_LOADER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_TYPE_ICON_LOADER, GnomeIconLoaderClass))
#define GNOME_IS_ICON_LOADER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_TYPE_ICON_LOADER))
#define GNOME_IS_ICON_LOADER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_ICON_LOADER))
#define GNOME_ICON_LOADER_GET_CLASS(obj)   (G_TYPE_CHECK_GET_CLASS ((obj), GNOME_TYPE_ICON_LOADER, GnomeIconLoaderClass))

typedef struct _GnomeIconLoader GnomeIconLoader;
typedef struct _GnomeIconLoaderClass GnomeIconLoaderClass;
typedef struct _GnomeIconLoaderPrivate GnomeIconLoaderPrivate;

struct _GnomeIconLoader
{
  GObject parent_instance;

  GnomeIconLoaderPrivate *priv;
};

struct _GnomeIconLoaderClass
{
  GObjectClass parent_class;
};


typedef struct
{
  int x;
  int y;
} GnomeIconDataPoint;

typedef struct
{
  gboolean has_embedded_rect;
  int x0, y0, x1, y1;
  
  GnomeIconDataPoint *attach_points;
  int n_attach_points;
} GnomeIconData;


GType            gnome_icon_loader_get_type              (void) G_GNUC_CONST;

GnomeIconLoader *gnome_icon_loader_new                   (void);
void             gnome_icon_loader_set_search_path       (GnomeIconLoader      *loader,
							  const char           *path[],
							  int                   n_elements);
void             gnome_icon_loader_set_current_theme     (GnomeIconLoader      *loader,
							  const char           *theme_name);
char *           gnome_icon_loader_lookup_icon           (GnomeIconLoader      *loader,
							  const char           *icon_name,
							  int                   size,
							  const GnomeIconData **icon_data);
GList *          gnome_icon_loader_list_icons            (GnomeIconLoader      *loader,
							  const char           *context);
char *           gnome_icon_loader_get_example_icon_name (GnomeIconLoader      *loader);
gboolean         gnome_icon_loader_rescan_if_needed      (GnomeIconLoader      *loader);

#endif /* GNOME_ICON_LOADER_H */
