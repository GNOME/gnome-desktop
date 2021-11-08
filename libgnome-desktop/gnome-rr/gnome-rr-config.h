/* gnome-rr-config.h: Display configuration
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
#include <gnome-rr/gnome-rr-output-info.h>
#include <gnome-rr/gnome-rr-screen.h>

G_BEGIN_DECLS

#define GNOME_RR_TYPE_CONFIG (gnome_rr_config_get_type())

G_DECLARE_FINAL_TYPE (GnomeRRConfig, gnome_rr_config, GNOME_RR, CONFIG, GObject)

GnomeRRConfig *         gnome_rr_config_new_current             (GnomeRRScreen  *screen,
						                 GError        **error);
gboolean                gnome_rr_config_load_current            (GnomeRRConfig  *self,
						                 GError        **error);
gboolean                gnome_rr_config_match                   (GnomeRRConfig  *config1,
						                 GnomeRRConfig  *config2);
gboolean                gnome_rr_config_equal	                (GnomeRRConfig  *config1,
						                 GnomeRRConfig  *config2);
void                    gnome_rr_config_sanitize                (GnomeRRConfig  *self);
gboolean                gnome_rr_config_ensure_primary          (GnomeRRConfig  *self);

gboolean	        gnome_rr_config_apply                   (GnomeRRConfig  *self,
					                         GnomeRRScreen  *screen,
					                         GError        **error);
gboolean	        gnome_rr_config_apply_persistent        (GnomeRRConfig  *self,
						                 GnomeRRScreen  *screen,
						                 GError        **error);

gboolean                gnome_rr_config_applicable              (GnomeRRConfig  *self,
						                 GnomeRRScreen  *screen,
						                 GError        **error);

gboolean                gnome_rr_config_get_clone               (GnomeRRConfig  *self);
void                    gnome_rr_config_set_clone               (GnomeRRConfig  *self,
                                                                 gboolean        clone);
GnomeRROutputInfo **    gnome_rr_config_get_outputs             (GnomeRRConfig  *self);

G_END_DECLS
