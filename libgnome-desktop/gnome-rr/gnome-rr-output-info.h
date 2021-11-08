/* gnome-rr-output-info.h: Display information
 *
 * SPDX-FileCopyrightText: 2007, 2008, Red Hat, Inc.
 * SPDX-FileCopyrightText: 2010 Giovanni Campagna
 * SPDX-License-Identifier: LGPL-2.0-or-later
 * 
 * Author: Soren Sandmann <sandmann@redhat.com>
 */
#pragma once

#ifndef GNOME_DESKTOP_USE_UNSTABLE_API
# error GnomeRR is unstable API. You must define GNOME_DESKTOP_USE_UNSTABLE_API before including gnome-rr/gnome-rr.h
#endif

#include <gnome-rr/gnome-rr-types.h>

G_BEGIN_DECLS

#define GNOME_RR_TYPE_OUTPUT_INFO (gnome_rr_output_info_get_type())

/**
 * GnomeRROutputInfo:
 *
 * The representation of an output, which can be used for
 * querying and setting display state.
 */
G_DECLARE_FINAL_TYPE (GnomeRROutputInfo, gnome_rr_output_info, GNOME_RR, OUTPUT_INFO, GObject)

const char *    gnome_rr_output_info_get_name                   (GnomeRROutputInfo *self);

gboolean        gnome_rr_output_info_is_active                  (GnomeRROutputInfo *self);
void            gnome_rr_output_info_set_active                 (GnomeRROutputInfo *self,
                                                                 gboolean active);

void            gnome_rr_output_info_get_geometry               (GnomeRROutputInfo *self,
                                                                 int *x,
                                                                 int *y,
                                                                 int *width,
                                                                 int *height);
void            gnome_rr_output_info_set_geometry               (GnomeRROutputInfo *self,
                                                                 int x,
                                                                 int y,
                                                                 int width,
                                                                 int height);

int             gnome_rr_output_info_get_refresh_rate           (GnomeRROutputInfo *self);
void            gnome_rr_output_info_set_refresh_rate           (GnomeRROutputInfo *self,
                                                                 int rate);

GnomeRRRotation gnome_rr_output_info_get_rotation               (GnomeRROutputInfo *self);
void            gnome_rr_output_info_set_rotation               (GnomeRROutputInfo *self,
                                                                 GnomeRRRotation rotation);
gboolean        gnome_rr_output_info_supports_rotation          (GnomeRROutputInfo *self,
                                                                 GnomeRRRotation rotation);

gboolean        gnome_rr_output_info_is_connected               (GnomeRROutputInfo *self);
const char *    gnome_rr_output_info_get_vendor                 (GnomeRROutputInfo *self);
const char *    gnome_rr_output_info_get_product                (GnomeRROutputInfo *self);
const char *    gnome_rr_output_info_get_serial                 (GnomeRROutputInfo *self);
double          gnome_rr_output_info_get_aspect_ratio           (GnomeRROutputInfo *self);
const char *    gnome_rr_output_info_get_display_name           (GnomeRROutputInfo *self);

gboolean        gnome_rr_output_info_get_primary                (GnomeRROutputInfo *self);
void            gnome_rr_output_info_set_primary                (GnomeRROutputInfo *self,
                                                                 gboolean primary);

int             gnome_rr_output_info_get_preferred_width        (GnomeRROutputInfo *self);
int             gnome_rr_output_info_get_preferred_height       (GnomeRROutputInfo *self);

gboolean        gnome_rr_output_info_get_underscanning          (GnomeRROutputInfo *self);
void            gnome_rr_output_info_set_underscanning          (GnomeRROutputInfo *self,
                                                                 gboolean underscanning);

gboolean        gnome_rr_output_info_is_primary_tile            (GnomeRROutputInfo *self);

G_END_DECLS
