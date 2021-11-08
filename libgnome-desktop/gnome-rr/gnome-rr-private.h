/* gnome-rr-private.h: Private type and API
 *
 * SPDX-FileCopyrightText: 2009  Novell Inc
 * SPDX-FileCopyrightText: 2014  Endless
 * SPDX-FileCopyrightText: 2009, 2014  Red Hat Inc
 * SPDX-FileCopyrightText: 2019  System76
 * SPDX-FileCopyrightText: 2021  Emmanuele Bassi
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#pragma once

#include "gnome-rr-types.h"
#include "gnome-rr-config.h"
#include "gnome-rr-output-info.h"

G_BEGIN_DECLS

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#else
enum wl_output_transform {
  WL_OUTPUT_TRANSFORM_NORMAL,
  WL_OUTPUT_TRANSFORM_90,
  WL_OUTPUT_TRANSFORM_180,
  WL_OUTPUT_TRANSFORM_270,
  WL_OUTPUT_TRANSFORM_FLIPPED,
  WL_OUTPUT_TRANSFORM_FLIPPED_90,
  WL_OUTPUT_TRANSFORM_FLIPPED_180,
  WL_OUTPUT_TRANSFORM_FLIPPED_270
};
#endif

#include "meta-xrandr-shared.h"
#include "meta-dbus-xrandr.h"

typedef struct _ScreenInfo ScreenInfo;

struct _ScreenInfo
{
    int	min_width;
    int	max_width;
    int	min_height;
    int	max_height;

    guint serial;
    
    GnomeRROutput **outputs;
    GnomeRRCrtc **crtcs;
    GnomeRRMode **modes;
    
    GnomeRRScreen *screen;

    GnomeRRMode **clone_modes;

    GnomeRROutput *primary;
};

typedef struct _GnomeRRScreenPrivate GnomeRRScreenPrivate;
struct _GnomeRRScreenPrivate
{
    GdkDisplay *gdk_display;
    ScreenInfo *info;

    int init_name_watch_id;
    MetaDBusDisplayConfig *proxy;
};

typedef struct _GnomeRRTile GnomeRRTile;

#define UNDEFINED_GROUP_ID 0
struct _GnomeRRTile
{
    guint group_id;
    guint flags;
    guint max_horiz_tiles;
    guint max_vert_tiles;
    guint loc_horiz;
    guint loc_vert;
    guint width;
    guint height;
};

struct _GnomeRROutputInfo
{
    char *name;

    gboolean active;
    int	width;
    int	height;
    int	rate;
    int	x;
    int	y;
    GnomeRRRotation rotation;
    GnomeRRRotation available_rotations;

    gboolean connected;
    char *vendor;
    char *product;
    char *serial;
    double aspect;
    int	pref_width;
    int	pref_height;
    char *display_name;
    char *connector_type;
    gboolean primary;
    gboolean underscanning;

    gboolean is_tiled;
    GnomeRRTile tile;

    int total_tiled_width;
    int total_tiled_height;

    /* weak pointer back to info */
    GnomeRRConfig *config;
};

struct _GnomeRRConfig
{
  gboolean clone;
  GnomeRRScreen *screen;
  GnomeRROutputInfo **outputs;
};

G_GNUC_INTERNAL
gboolean
gnome_rr_output_connector_type_is_builtin_display (const char *connector_type);

G_GNUC_INTERNAL
gboolean
gnome_rr_screen_apply_configuration (GnomeRRScreen *screen,
                                     gboolean persistent,
                                     GVariant *crtcs,
                                     GVariant *outputs,
                                     GError **error);

G_GNUC_INTERNAL
const char *
gnome_rr_output_get_connector_type (GnomeRROutput *output);

G_GNUC_INTERNAL
gboolean
gnome_rr_output_get_tile_info (const GnomeRROutput *output,
                               GnomeRRTile *tile);

G_GNUC_INTERNAL
gboolean
gnome_rr_output_get_tiled_display_size (const GnomeRROutput *output,
                                        int *tile_w,
                                        int *tile_h,
                                        int *width,
                                        int *height);

G_END_DECLS
