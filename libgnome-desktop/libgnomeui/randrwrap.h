/* randrwrap.h
 *
 * Copyright 2007, 2008, Red Hat, Inc.
 * 
 * This file is part of the Gnome Library.
 * 
 * The Gnome Library is free software; you can redistribute it and/or
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
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 * 
 * Author: Soren Sandmann <sandmann@redhat.com>
 */
#ifndef RANDR_WRAP_H
#define RANDR_WRAP_H

#ifndef GNOME_DESKTOP_USE_UNSTABLE_API
#error    randrwrap is unstable API. You must define GNOME_DESKTOP_USE_UNSTABLE_API before including randrwrap.h
#endif

#include <glib.h>
#include <gdk/gdk.h>

typedef struct RWScreen RWScreen;
typedef struct RWOutput RWOutput;
typedef struct RWCrtc RWCrtc;
typedef struct RWMode RWMode;

typedef void (* RWScreenChanged) (RWScreen *screen, gpointer data);

typedef enum
{
    RW_ROTATION_0 =	(1 << 0),
    RW_ROTATION_90 =	(1 << 1),
    RW_ROTATION_180 =	(1 << 2),
    RW_ROTATION_270 =	(1 << 3),
    RW_REFLECT_X =	(1 << 4),
    RW_REFLECT_Y =	(1 << 5)
} RWRotation;

/* RWScreen */
RWScreen *    rw_screen_new                (GdkScreen       *screen,
					    RWScreenChanged  callback,
					    gpointer         data);
RWOutput **   rw_screen_list_outputs       (RWScreen        *screen);
RWCrtc **     rw_screen_list_crtcs         (RWScreen        *screen);
RWMode **     rw_screen_list_modes         (RWScreen        *screen);
void          rw_screen_set_size           (RWScreen        *screen,
					    int		     width,
					    int              height,
					    int              mm_width,
					    int              mm_height);
RWCrtc *      rw_screen_get_crtc_by_id     (RWScreen        *screen,
					    guint32          id);
gboolean      rw_screen_refresh            (RWScreen        *screen);
RWOutput *    rw_screen_get_output_by_id   (RWScreen        *screen,
					    guint32          id);
RWOutput *    rw_screen_get_output_by_name (RWScreen        *screen,
					    const char      *name);
void	      rw_screen_get_ranges         (RWScreen	    *screen,
					    int		    *min_width,
					    int		    *max_width,
					    int             *min_height,
					    int		    *max_height);

/* RWOutput */
guint32       rw_output_get_id             (RWOutput        *output);
const char *  rw_output_get_name           (RWOutput        *output);
gboolean      rw_output_is_connected       (RWOutput        *output);
int           rw_output_get_size_inches    (RWOutput        *output);
int           rw_output_get_width_mm       (RWOutput        *outout);
int           rw_output_get_height_mm      (RWOutput        *output);
const guint8 *rw_output_get_edid_data      (RWOutput        *output);
RWCrtc **     rw_output_get_possible_crtcs (RWOutput        *output);
RWMode *      rw_output_get_current_mode   (RWOutput        *output);
RWCrtc *      rw_output_get_crtc           (RWOutput        *output);
void          rw_output_get_position       (RWOutput        *output,
					    int             *x,
					    int             *y);
gboolean      rw_output_can_clone          (RWOutput        *output,
					    RWOutput        *clone);
RWMode **     rw_output_list_modes         (RWOutput        *output);
RWMode *      rw_output_get_preferred_mode (RWOutput        *output);
gboolean      rw_output_supports_mode      (RWOutput        *output,
					    RWMode          *mode);

/* RWMode */
guint32	      rw_mode_get_id               (RWMode          *mode);
guint	      rw_mode_get_width		   (RWMode          *mode);
guint         rw_mode_get_height           (RWMode          *mode);
int           rw_mode_get_freq             (RWMode          *mode);

/* RWCrtc */
guint32       rw_crtc_get_id               (RWCrtc          *crtc);
gboolean      rw_crtc_set_config	   (RWCrtc          *crtc,
					    int		     x,
					    int		     y,
					    RWMode	    *mode,
					    RWRotation	     rotation,
					    RWOutput	   **outputs,
				  	    int              n_outputs);
gboolean      rw_crtc_can_drive_output     (RWCrtc          *crtc,
					    RWOutput        *output);
RWMode *      rw_crtc_get_current_mode     (RWCrtc          *crtc);
void          rw_crtc_get_position         (RWCrtc          *crtc,
					    int             *x,
					    int             *y);
RWRotation    rw_crtc_get_current_rotation (RWCrtc          *crtc);
RWRotation    rw_crtc_get_rotations        (RWCrtc          *crtc);
gboolean      rw_crtc_supports_rotation    (RWCrtc	    *crtc,
					    RWRotation	     rotation);

#endif
