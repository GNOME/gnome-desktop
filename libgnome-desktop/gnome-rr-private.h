#ifndef GNOME_RR_PRIVATE_H
#define GNOME_RR_PRIVATE_H

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
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

typedef struct ScreenInfo ScreenInfo;

struct ScreenInfo
{
    int			min_width;
    int			max_width;
    int			min_height;
    int			max_height;

    guint               serial;
    
    GnomeRROutput **	outputs;
    GnomeRRCrtc **	crtcs;
    GnomeRRMode **	modes;
    
    GnomeRRScreen *	screen;

    GnomeRRMode **	clone_modes;

    GnomeRROutput *     primary;
};

struct GnomeRRScreenPrivate
{
    GdkScreen *			gdk_screen;
    GdkWindow *			gdk_root;
    ScreenInfo *		info;

    int                         init_name_watch_id;
    MetaDBusDisplayConfig      *proxy;
};

struct _GnomeRROutputInfoPrivate
{
    char *		name;

    gboolean		on;
    int			width;
    int			height;
    int			rate;
    int			x;
    int			y;
    GnomeRRRotation	rotation;
    GnomeRRRotation	available_rotations;

    gboolean		connected;
    char *		vendor;
    char *		product;
    char *		serial;
    double		aspect;
    int			pref_width;
    int			pref_height;
    char *		display_name;
    gboolean            primary;
};

struct _GnomeRRConfigPrivate
{
  gboolean clone;
  GnomeRRScreen *screen;
  GnomeRROutputInfo **outputs;
};

gboolean _gnome_rr_output_name_is_builtin_display (const char *name);

gboolean _gnome_rr_screen_apply_configuration (GnomeRRScreen  *screen,
					       gboolean        persistent,
					       GVariant       *crtcs,
					       GVariant       *outputs,
					       GError        **error);

#endif
