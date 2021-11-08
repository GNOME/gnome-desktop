/* gnome-rr-types.h: Shared types
 *
 * SPDX-FileCopyrightText: 2021 Emmanuele Bassi
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#pragma once

#ifndef GNOME_DESKTOP_USE_UNSTABLE_API
# error GnomeRR is unstable API. You must define GNOME_DESKTOP_USE_UNSTABLE_API before including gnome-rr/gnome-rr.h
#endif

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum
{
    GNOME_RR_ROTATION_NEXT =	0,
    GNOME_RR_ROTATION_0 =	(1 << 0),
    GNOME_RR_ROTATION_90 =	(1 << 1),
    GNOME_RR_ROTATION_180 =	(1 << 2),
    GNOME_RR_ROTATION_270 =	(1 << 3),
    GNOME_RR_REFLECT_X =	(1 << 4),
    GNOME_RR_REFLECT_Y =	(1 << 5)
} GnomeRRRotation;

typedef enum {
    GNOME_RR_DPMS_ON,
    GNOME_RR_DPMS_STANDBY,
    GNOME_RR_DPMS_SUSPEND,
    GNOME_RR_DPMS_OFF,
    GNOME_RR_DPMS_UNKNOWN
} GnomeRRDpmsMode;

/* Identical to drm_color_ctm from <drm_mode.h> */
typedef struct {
    /*< private >*/

    /* Conversion matrix in S31.32 sign-magnitude (not two's complement!) format */
    guint64 matrix[9];
} GnomeRRCTM;

typedef struct _GnomeRRScreen GnomeRRScreen;
typedef struct _GnomeRROutput GnomeRROutput;
typedef struct _GnomeRRCrtc GnomeRRCrtc;
typedef struct _GnomeRRMode GnomeRRMode;

G_END_DECLS
