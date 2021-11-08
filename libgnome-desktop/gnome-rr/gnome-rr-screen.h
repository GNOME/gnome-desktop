/* gnome-rr-screen.h: Display information
 *
 * SPDX-FileCopyrightText: 2007, 2008, Red Hat, Inc.
 * SPDX-FileCopyrightText: 2020 NVIDIA CORPORATION
 * SPDX-License-Identifier: LGPL-2.0-or-later
 * 
 * Author: Soren Sandmann <sandmann@redhat.com>
 */
#pragma once

#ifndef GNOME_DESKTOP_USE_UNSTABLE_API
# error GnomeRR is unstable API. You must define GNOME_DESKTOP_USE_UNSTABLE_API before including gnome-rr/gnome-rr.h
#endif

#include <gdk/gdk.h>
#include <gnome-rr/gnome-rr-types.h>

G_BEGIN_DECLS

#define GNOME_RR_TYPE_SCREEN (gnome_rr_screen_get_type())

G_DECLARE_DERIVABLE_TYPE (GnomeRRScreen, gnome_rr_screen, GNOME_RR, SCREEN, GObject)

struct _GnomeRRScreenClass
{
    /*< private >*/
    GObjectClass parent_class;

    /*< public >*/
    void        (*changed)              (GnomeRRScreen *screen);
    void        (*output_connected)     (GnomeRRScreen *screen,
                                         GnomeRROutput *output);
    void        (*output_disconnected)  (GnomeRRScreen *screen,
                                         GnomeRROutput *output);
};

/* Error codes */

#define GNOME_RR_ERROR (gnome_rr_error_quark ())

GQuark gnome_rr_error_quark (void);

typedef enum {
    GNOME_RR_ERROR_UNKNOWN,		/* generic "fail" */
    GNOME_RR_ERROR_NO_RANDR_EXTENSION,	/* RANDR extension is not present */
    GNOME_RR_ERROR_RANDR_ERROR,		/* generic/undescribed error from the underlying XRR API */
    GNOME_RR_ERROR_BOUNDS_ERROR,	/* requested bounds of a CRTC are outside the maximum size */
    GNOME_RR_ERROR_CRTC_ASSIGNMENT,	/* could not assign CRTCs to outputs */
    GNOME_RR_ERROR_NO_MATCHING_CONFIG,	/* none of the saved configurations matched the current configuration */
    GNOME_RR_ERROR_NO_DPMS_EXTENSION	/* DPMS extension is not present */
} GnomeRRError;

#define GNOME_RR_CONNECTOR_TYPE_PANEL   "Panel"  /* This is a built-in LCD */

#define GNOME_RR_TYPE_OUTPUT    (gnome_rr_output_get_type ())
#define GNOME_RR_TYPE_CRTC      (gnome_rr_crtc_get_type ())
#define GNOME_RR_TYPE_MODE      (gnome_rr_mode_get_type ())
#define GNOME_RR_TYPE_DPMS_MODE (gnome_rr_dpms_mode_get_type ())

GType gnome_rr_output_get_type (void);
GType gnome_rr_crtc_get_type (void);
GType gnome_rr_mode_get_type (void);
GType gnome_rr_dpms_mode_get_type (void);

/* GnomeRRScreen */
GnomeRRScreen * gnome_rr_screen_new                (GdkDisplay            *display,
						    GError               **error);
void            gnome_rr_screen_new_async          (GdkDisplay            *display,
                                                    GAsyncReadyCallback    callback,
                                                    gpointer               user_data);
GnomeRRScreen * gnome_rr_screen_new_finish         (GAsyncResult          *result,
                                                    GError               **error);
GnomeRROutput **gnome_rr_screen_list_outputs       (GnomeRRScreen         *screen);
GnomeRRCrtc **  gnome_rr_screen_list_crtcs         (GnomeRRScreen         *screen);
GnomeRRMode **  gnome_rr_screen_list_modes         (GnomeRRScreen         *screen);
GnomeRRMode **  gnome_rr_screen_list_clone_modes   (GnomeRRScreen	  *screen);
GnomeRRCrtc *   gnome_rr_screen_get_crtc_by_id     (GnomeRRScreen         *screen,
						    guint32                id);
gboolean        gnome_rr_screen_refresh            (GnomeRRScreen         *screen,
						    GError               **error);
GnomeRROutput * gnome_rr_screen_get_output_by_id   (GnomeRRScreen         *screen,
						    guint32                id);
GnomeRROutput * gnome_rr_screen_get_output_by_name (GnomeRRScreen         *screen,
						    const char            *name);
void            gnome_rr_screen_get_ranges         (GnomeRRScreen         *screen,
						    int                   *min_width,
						    int                   *max_width,
						    int                   *min_height,
						    int                   *max_height);

gboolean        gnome_rr_screen_get_dpms_mode      (GnomeRRScreen         *screen,
                                                    GnomeRRDpmsMode       *mode,
                                                    GError               **error);
gboolean        gnome_rr_screen_set_dpms_mode      (GnomeRRScreen         *screen,
                                                    GnomeRRDpmsMode        mode,
                                                    GError              **error);

/* GnomeRROutput */
guint32         gnome_rr_output_get_id                          (const GnomeRROutput  *output);
const char *    gnome_rr_output_get_name                        (const GnomeRROutput  *output);
const char *    gnome_rr_output_get_display_name                (const GnomeRROutput  *output);
const guint8 *  gnome_rr_output_get_edid_data                   (GnomeRROutput        *output,
						                 gsize                *size);
void            gnome_rr_output_get_ids_from_edid               (const GnomeRROutput  *output,
                                                                 char                **vendor,
                                                                 char                **product,
                                                                 char                **serial);
void            gnome_rr_output_get_physical_size               (const GnomeRROutput  *output,
                                                                 int                  *width_mm,
                                                                 int                  *height_mm);

gint            gnome_rr_output_get_backlight                   (const GnomeRROutput  *output);
gint            gnome_rr_output_get_min_backlight_step          (const GnomeRROutput  *output);
gboolean        gnome_rr_output_set_backlight                   (GnomeRROutput        *output,
                                                                 int                   value,
                                                                 GError              **error);
gboolean        gnome_rr_output_set_color_transform             (GnomeRROutput        *output,
                                                                 GnomeRRCTM            ctm,
                                                                 GError              **error);

GnomeRRCrtc **  gnome_rr_output_get_possible_crtcs              (const GnomeRROutput  *output);
GnomeRRMode *   gnome_rr_output_get_current_mode                (const GnomeRROutput  *output);
GnomeRRCrtc *   gnome_rr_output_get_crtc                        (const GnomeRROutput  *output);
gboolean        gnome_rr_output_is_builtin_display              (const GnomeRROutput  *output);
void            gnome_rr_output_get_position                    (const GnomeRROutput  *output,
						                 int                  *x,
						                 int                  *y);
gboolean        gnome_rr_output_can_clone                       (const GnomeRROutput  *output,
						                 const GnomeRROutput  *clone);
GnomeRRMode **  gnome_rr_output_list_modes                      (const GnomeRROutput  *output);
GnomeRRMode *   gnome_rr_output_get_preferred_mode              (const GnomeRROutput  *output);
gboolean        gnome_rr_output_supports_mode                   (const GnomeRROutput  *output,
						                 const GnomeRRMode    *mode);
gboolean        gnome_rr_output_get_is_primary                  (const GnomeRROutput  *output);
gboolean        gnome_rr_output_get_is_underscanning            (const GnomeRROutput  *output);
gboolean        gnome_rr_output_supports_underscanning          (const GnomeRROutput  *output);
gboolean        gnome_rr_output_supports_color_transform        (const GnomeRROutput  *output);

/* GnomeRRMode */
guint32         gnome_rr_mode_get_id            (GnomeRRMode *mode);
guint           gnome_rr_mode_get_width         (GnomeRRMode *mode);
guint           gnome_rr_mode_get_height        (GnomeRRMode *mode);
int             gnome_rr_mode_get_freq          (GnomeRRMode *mode);
double          gnome_rr_mode_get_freq_f        (GnomeRRMode *mode);
gboolean        gnome_rr_mode_get_is_tiled      (GnomeRRMode *mode);
gboolean        gnome_rr_mode_get_is_interlaced (GnomeRRMode *mode);

/* GnomeRRCrtc */
guint32         gnome_rr_crtc_get_id                    (GnomeRRCrtc      *crtc);

gboolean        gnome_rr_crtc_can_drive_output          (GnomeRRCrtc      *crtc,
						         GnomeRROutput    *output);
GnomeRRMode *   gnome_rr_crtc_get_current_mode          (GnomeRRCrtc      *crtc);
void            gnome_rr_crtc_get_position              (GnomeRRCrtc      *crtc,
						         int              *x,
						         int              *y);
GnomeRRRotation gnome_rr_crtc_get_current_rotation      (GnomeRRCrtc      *crtc);
GnomeRRRotation gnome_rr_crtc_get_rotations             (GnomeRRCrtc      *crtc);
gboolean        gnome_rr_crtc_supports_rotation         (GnomeRRCrtc      *crtc,
						         GnomeRRRotation   rotation);

gboolean        gnome_rr_crtc_get_gamma                 (GnomeRRCrtc      *crtc,
						         int              *size,
						         unsigned short  **red,
						         unsigned short  **green,
						         unsigned short  **blue);
gboolean        gnome_rr_crtc_set_gamma                 (GnomeRRCrtc      *crtc,
						         int               size,
						         unsigned short   *red,
						         unsigned short   *green,
						         unsigned short   *blue);

G_END_DECLS
