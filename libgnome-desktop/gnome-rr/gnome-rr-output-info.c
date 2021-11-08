/* gnome-rr-output-info.c
 * -*- c-basic-offset: 4 -*-
 *
 * Copyright 2010 Giovanni Campagna
 *
 * This file is part of the Gnome Desktop Library.
 *
 * The Gnome Desktop Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Desktop Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#define GNOME_DESKTOP_USE_UNSTABLE_API

#include "config.h"

#include "gnome-rr-output-info.h"

#include "gnome-rr-config.h"
#include "gnome-rr-private.h"
#include "gnome-rr-screen.h"

G_DEFINE_TYPE (GnomeRROutputInfo, gnome_rr_output_info, G_TYPE_OBJECT)

static void
gnome_rr_output_info_finalize (GObject *gobject)
{
    GnomeRROutputInfo *self = GNOME_RR_OUTPUT_INFO (gobject);

    g_free (self->name);
    g_free (self->display_name);
    g_free (self->connector_type);
    g_free (self->product);
    g_free (self->serial);
    g_free (self->vendor);

    G_OBJECT_CLASS (gnome_rr_output_info_parent_class)->finalize (gobject);
}

static void
gnome_rr_output_info_class_init (GnomeRROutputInfoClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize = gnome_rr_output_info_finalize;
}

static void
gnome_rr_output_info_init (GnomeRROutputInfo *self)
{
    self->name = NULL;
    self->active = FALSE;
    self->rotation = GNOME_RR_ROTATION_0;
    self->display_name = NULL;
    self->connector_type = NULL;
}

/**
 * gnome_rr_output_info_get_name:
 * @self: the output information
 *
 * Retrieves the output name.
 *
 * Returns: (transfer none): the output name
 */
const char *
gnome_rr_output_info_get_name (GnomeRROutputInfo *self)
{
    g_return_val_if_fail (GNOME_RR_IS_OUTPUT_INFO (self), NULL);

    return self->name;
}

/**
 * gnome_rr_output_info_is_active:
 *
 * Returns: whether there is a CRTC assigned to this output (i.e. a signal is being sent to it)
 */
gboolean
gnome_rr_output_info_is_active (GnomeRROutputInfo *self)
{
    g_return_val_if_fail (GNOME_RR_IS_OUTPUT_INFO (self), FALSE);

    return self->active;
}

void
gnome_rr_output_info_set_active (GnomeRROutputInfo *self, gboolean active)
{
    g_return_if_fail (GNOME_RR_IS_OUTPUT_INFO (self));

    active = !!active;
    if (self->active != active) {
        self->active = active;
    }
}

static void
gnome_rr_output_info_get_tiled_geometry (GnomeRROutputInfo *self,
                                         int *x,
                                         int *y,
                                         int *width,
                                         int *height)
{
    GnomeRROutputInfo **outputs;
    gboolean active;
    int i;
    guint ht, vt;
    int total_w = 0, total_h = 0;

    outputs = gnome_rr_config_get_outputs (self->config);

    /*
     * iterate over all the outputs from 0,0 -> h,v
     * find the output for each tile,
     * if it is the 0 tile, store the x/y offsets.
     * if the tile is active, add the tile to the total w/h
     * for the output if the tile is in the 0 row or 0 column.
     */
    for (ht = 0; ht < self->tile.max_horiz_tiles; ht++)
    {
        for (vt = 0; vt < self->tile.max_vert_tiles; vt++)
        {
            for (i = 0; outputs[i]; i++)
            {
                GnomeRRTile *this_tile = &outputs[i]->tile;

                if (!outputs[i]->is_tiled)
                    continue;

                if (this_tile->group_id != self->tile.group_id)
                    continue;

                if (this_tile->loc_horiz != ht ||
                    this_tile->loc_vert != vt)
                    continue;

                if (vt == 0 && ht == 0)
                {
                    if (x)
                        *x = outputs[i]->x;
                    if (y)
                        *y = outputs[i]->y;
                }

                active = gnome_rr_output_info_is_active (outputs[i]);
                if (!active)
                    continue;

                if (this_tile->loc_horiz == 0)
                    total_h += outputs[i]->height;

                if (this_tile->loc_vert == 0)
                    total_w += outputs[i]->width;
            }
        }
    }

    if (width)
        *width = total_w;
    if (height)
        *height = total_h;
}

/**
 * gnome_rr_output_info_get_geometry:
 * @self: a #GnomeRROutputInfo
 * @x: (out) (optional):
 * @y: (out) (optional):
 * @width: (out) (optional):
 * @height: (out) (optional):
 *
 * Get the geometry for the monitor connected to the specified output.
 *
 * If the monitor is a tiled monitor, it returns the geometry for the complete monitor.
 */
void
gnome_rr_output_info_get_geometry (GnomeRROutputInfo *self,
                                   int *x,
                                   int *y,
                                   int *width,
                                   int *height)
{
    g_return_if_fail (GNOME_RR_IS_OUTPUT_INFO (self));

    if (self->is_tiled) {
        gnome_rr_output_info_get_tiled_geometry (self, x, y, width, height);
        return;
    }

    if (x)
	*x = self->x;
    if (y)
	*y = self->y;
    if (width)
	*width = self->width;
    if (height)
	*height = self->height;
}

static void
gnome_rr_output_info_set_tiled_geometry (GnomeRROutputInfo *self,
                                         int x,
                                         int y,
                                         int width,
                                         int height)
{
    GnomeRROutputInfo **outputs;
    gboolean primary_tile_only = FALSE;
    guint ht, vt;
    int i, x_off;

    primary_tile_only = TRUE;

    if (width == self->total_tiled_width &&
        height == self->total_tiled_height)
        primary_tile_only = FALSE;

    outputs = gnome_rr_config_get_outputs (self->config);
    /*
     * iterate over all the outputs from 0,0 -> h,v
     * find the output for each tile,
     * if only the primary tile is being set, disable
     * the non-primary tiles, and set the output up
     * for tile 0 only.
     * if all tiles are being set, then store the
     * dimensions for this tile, and increase the offsets.
     * y_off is reset per column of tiles,
     * addx is incremented for the first row of tiles
     * to set the correct x offset.
     */
    x_off = 0;
    for (ht = 0; ht < self->tile.max_horiz_tiles; ht++)
    {
        int y_off = 0;
        int addx = 0;
        for (vt = 0; vt < self->tile.max_vert_tiles; vt++)
        {
            for (i = 0; outputs[i]; i++)
            {
                GnomeRRTile *this_tile = &outputs[i]->tile;

                if (!outputs[i]->is_tiled)
                    continue;

                if (this_tile->group_id != self->tile.group_id)
                    continue;

                if (this_tile->loc_horiz != ht ||
                    this_tile->loc_vert != vt)
                    continue;

                /* for primary tile only configs turn off non-primary
                   tiles - turn them on for tiled ones */
                if (ht != 0 || vt != 0)
                {
                    if (!self->active)
                        outputs[i]->active = FALSE;
                    else
                        outputs[i]->active = !primary_tile_only;
                }

                if (primary_tile_only)
                {
                        if (ht == 0 && vt == 0)
                        {
                            outputs[i]->x = x;
                            outputs[i]->y = y;
                            outputs[i]->width = width;
                            outputs[i]->height = height;
                        }
                }
                else
                {
                    outputs[i]->x = x + x_off;
                    outputs[i]->y = y + y_off;
                    outputs[i]->width = this_tile->width;
                    outputs[i]->height = this_tile->height;

                    y_off += this_tile->height;
                    if (vt == 0)
                        addx = this_tile->width;
                }
            }
        }
        x_off += addx;
    }
}

/**
 * gnome_rr_output_info_set_geometry:
 * @self: a #GnomeRROutputInfo
 * @x: x offset for monitor
 * @y: y offset for monitor
 * @width: monitor width
 * @height: monitor height
 *
 * Set the geometry for the monitor connected to the specified output.
 *
 * If the monitor is a tiled monitor, it sets the geometry for the complete monitor.
 */

void
gnome_rr_output_info_set_geometry (GnomeRROutputInfo *self,
                                   int x,
                                   int y,
                                   int width,
                                   int height)
{
    g_return_if_fail (GNOME_RR_IS_OUTPUT_INFO (self));

    if (self->is_tiled) {
        gnome_rr_output_info_set_tiled_geometry (self, x, y, width, height);
    } else {
        self->x = x;
        self->y = y;
        self->width = width;
        self->height = height;
    }
}

int
gnome_rr_output_info_get_refresh_rate (GnomeRROutputInfo *self)
{
    g_return_val_if_fail (GNOME_RR_IS_OUTPUT_INFO (self), 0);

    return self->rate;
}

void
gnome_rr_output_info_set_refresh_rate (GnomeRROutputInfo *self,
                                       int rate)
{
    g_return_if_fail (GNOME_RR_IS_OUTPUT_INFO (self));

    if (self->rate != rate) {
        self->rate = rate;
    }
}

GnomeRRRotation
gnome_rr_output_info_get_rotation (GnomeRROutputInfo *self)
{
    g_return_val_if_fail (GNOME_RR_IS_OUTPUT_INFO (self), GNOME_RR_ROTATION_0);

    return self->rotation;
}

static void
gnome_rr_output_info_set_tiled_rotation (GnomeRROutputInfo *self,
                                         GnomeRRRotation rotation)
{
    GnomeRROutputInfo **outputs = gnome_rr_config_get_outputs (self->config);

    int base_x = 0, base_y = 0;
    int x_off = 0;

    /*
     * iterate over all the outputs from 0,0 -> h,v
     * find the output for each tile,
     * for all tiles set the rotation,
     * for the 0 tile use the base X/Y offsets
     * for non-0 tile, rotate the offsets of each
     * tile so things display correctly.
     */
    for (guint ht = 0; ht < self->tile.max_horiz_tiles; ht++) {
        int y_off = 0;
        int addx = 0;

        for (guint vt = 0; vt < self->tile.max_vert_tiles; vt++) {
            for (int i = 0; outputs[i] != NULL; i++) {
                GnomeRRTile *this_tile = &outputs[i]->tile;
                int new_x, new_y;

                if (!outputs[i]->is_tiled)
                    continue;

                if (this_tile->group_id != self->tile.group_id)
                    continue;

                if (this_tile->loc_horiz != ht ||
                    this_tile->loc_vert != vt)
                    continue;

                /* set tile rotation */
                outputs[i]->rotation = rotation;

                /* for non-zero tiles - change the offsets */
                if (ht == 0 && vt == 0) {
                    base_x = outputs[i]->x;
                    base_y = outputs[i]->y;
                } else {
                    if ((rotation & GNOME_RR_ROTATION_90) ||
                        (rotation & GNOME_RR_ROTATION_270)) {
                        new_x = base_x + y_off;
                        new_y = base_y + x_off;
                    } else {
                        new_x = base_x + x_off;
                        new_y = base_y + y_off;
                    }

                    outputs[i]->x = new_x;
                    outputs[i]->y = new_y;
                    outputs[i]->width = this_tile->width;
                    outputs[i]->height = this_tile->height;
                }

                y_off += this_tile->height;
                if (vt == 0) {
                    addx = this_tile->width;
                }
            }
        }
        x_off += addx;
    }
}

void
gnome_rr_output_info_set_rotation (GnomeRROutputInfo *self,
                                   GnomeRRRotation rotation)
{
    g_return_if_fail (GNOME_RR_IS_OUTPUT_INFO (self));

    if (self->is_tiled) {
        gnome_rr_output_info_set_tiled_rotation (self, rotation);
    } else {
        if (self->rotation != rotation) {
                self->rotation = rotation;
        }
    }
}

gboolean
gnome_rr_output_info_supports_rotation (GnomeRROutputInfo *self,
                                        GnomeRRRotation rotation)
{
    g_return_val_if_fail (GNOME_RR_IS_OUTPUT_INFO (self), FALSE);

    return (self->available_rotations & rotation);
}

/**
 * gnome_rr_output_info_is_connected:
 *
 * Returns: whether the output is physically connected to a monitor
 */
gboolean gnome_rr_output_info_is_connected (GnomeRROutputInfo *self)
{
    g_return_val_if_fail (GNOME_RR_IS_OUTPUT_INFO (self), FALSE);

    return self->connected;
}

/**
 * gnome_rr_output_info_get_vendor:
 * @self: the output information
 *
 * Returns: (transfer none): the output's vendor string
 */
const char *
gnome_rr_output_info_get_vendor (GnomeRROutputInfo *self)
{
    g_return_val_if_fail (GNOME_RR_IS_OUTPUT_INFO (self), NULL);

    return self->vendor;
}

const char *
gnome_rr_output_info_get_product (GnomeRROutputInfo *self)
{
    g_return_val_if_fail (GNOME_RR_IS_OUTPUT_INFO (self), NULL);

    return self->product;
}

const char *
gnome_rr_output_info_get_serial (GnomeRROutputInfo *self)
{
    g_return_val_if_fail (GNOME_RR_IS_OUTPUT_INFO (self), NULL);

    return self->serial;
}

double
gnome_rr_output_info_get_aspect_ratio (GnomeRROutputInfo *self)
{
    g_return_val_if_fail (GNOME_RR_IS_OUTPUT_INFO (self), 0);

    return self->aspect;
}

/**
 * gnome_rr_output_info_get_display_name:
 *
 * Returns: (transfer none): the display name of this output
 */
const char *
gnome_rr_output_info_get_display_name (GnomeRROutputInfo *self)
{
    g_return_val_if_fail (GNOME_RR_IS_OUTPUT_INFO (self), NULL);

    return self->display_name;
}

gboolean
gnome_rr_output_info_get_primary (GnomeRROutputInfo *self)
{
    g_return_val_if_fail (GNOME_RR_IS_OUTPUT_INFO (self), FALSE);

    return self->primary;
}

void
gnome_rr_output_info_set_primary (GnomeRROutputInfo *self,
                                  gboolean primary)
{
    g_return_if_fail (GNOME_RR_IS_OUTPUT_INFO (self));

    primary = !!primary;

    if (self->primary != primary) {
        self->primary = primary;
    }
}

int
gnome_rr_output_info_get_preferred_width (GnomeRROutputInfo *self)
{
    g_return_val_if_fail (GNOME_RR_IS_OUTPUT_INFO (self), 0);

    return self->pref_width;
}

int
gnome_rr_output_info_get_preferred_height (GnomeRROutputInfo *self)
{
    g_return_val_if_fail (GNOME_RR_IS_OUTPUT_INFO (self), 0);

    return self->pref_height;
}

gboolean
gnome_rr_output_info_get_underscanning (GnomeRROutputInfo *self)
{
    g_return_val_if_fail (GNOME_RR_IS_OUTPUT_INFO (self), FALSE);

    return self->underscanning;
}

void
gnome_rr_output_info_set_underscanning (GnomeRROutputInfo *self,
                                        gboolean underscanning)
{
    g_return_if_fail (GNOME_RR_IS_OUTPUT_INFO (self));

    underscanning = !!underscanning;

    if (self->underscanning != underscanning) {
        self->underscanning = underscanning;
    }
}

/**
 * gnome_rr_output_info_is_primary_tile
 * @self: a #GnomeRROutputInfo
 *
 * Returns: %TRUE if the specified output is connected to
 * the primary tile of a monitor or to an untiled monitor,
 * %FALSE if the output is connected to a secondary tile.
 */
gboolean
gnome_rr_output_info_is_primary_tile (GnomeRROutputInfo *self)
{
    g_return_val_if_fail (GNOME_RR_IS_OUTPUT_INFO (self), FALSE);

    if (!self->is_tiled)
        return TRUE;

    if (self->tile.loc_horiz == 0 &&
        self->tile.loc_vert == 0)
        return TRUE;

    return FALSE;
}
