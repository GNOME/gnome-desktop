/* gnome-rr.c
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

#define GNOME_DESKTOP_USE_UNSTABLE_API

#include "libgnomeui/gnome-rr.h"
#include <string.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xatom.h>

#define DISPLAY(o) ((o)->info->screen->xdisplay)

typedef struct ScreenInfo ScreenInfo;

struct ScreenInfo
{
    int			min_width;
    int			max_width;
    int			min_height;
    int			max_height;

    XRRScreenResources *resources;
    
    GnomeRROutput **	outputs;
    GnomeRRCrtc **	crtcs;
    GnomeRRMode **	modes;
    
    GnomeRRScreen *	screen;

    GnomeRRMode **	clone_modes;
};

struct GnomeRRScreen
{
    GdkScreen *			gdk_screen;
    GdkWindow *			gdk_root;
    Display *			xdisplay;
    Screen *			xscreen;
    Window			xroot;
    ScreenInfo *		info;
    
    int				randr_event_base;
    
    GnomeRRScreenChanged	callback;
    gpointer			data;
};

struct GnomeRROutput
{
    ScreenInfo *	info;
    RROutput		id;
    
    char *		name;
    GnomeRRCrtc *	current_crtc;
    gboolean		connected;
    gulong		width_mm;
    gulong		height_mm;
    GnomeRRCrtc **	possible_crtcs;
    GnomeRROutput **	clones;
    GnomeRRMode **	modes;
    int			n_preferred;
    guint8 *		edid_data;
};

struct GnomeRROutputWrap
{
    RROutput		id;
};

struct GnomeRRCrtc
{
    ScreenInfo *	info;
    RRCrtc		id;
    
    GnomeRRMode *	current_mode;
    GnomeRROutput **	current_outputs;
    GnomeRROutput **	possible_outputs;
    int			x;
    int			y;
    
    GnomeRRRotation	current_rotation;
    GnomeRRRotation	rotations;
};

struct GnomeRRMode
{
    ScreenInfo *	info;
    RRMode		id;
    char *		name;
    int			width;
    int			height;
    int			freq;		/* in mHz */
};

/* GnomeRRCrtc */
static GnomeRRCrtc *  crtc_new          (ScreenInfo         *info,
					 RRCrtc              id);
static void           crtc_free         (GnomeRRCrtc        *crtc);
static void           crtc_initialize   (GnomeRRCrtc        *crtc,
					 XRRScreenResources *res);

/* GnomeRROutput */
static GnomeRROutput *output_new        (ScreenInfo         *info,
					 RROutput            id);
static void           output_initialize (GnomeRROutput      *output,
					 XRRScreenResources *res);
static void           output_free       (GnomeRROutput      *output);

/* GnomeRRMode */
static GnomeRRMode *  mode_new          (ScreenInfo         *info,
					 RRMode              id);
static void           mode_initialize   (GnomeRRMode        *mode,
					 XRRModeInfo        *info);
static void           mode_free         (GnomeRRMode        *mode);


/* Screen */
static GnomeRROutput *
gnome_rr_output_by_id (ScreenInfo *info, RROutput id)
{
    GnomeRROutput **output;
    
    g_assert (info != NULL);
    
    for (output = info->outputs; *output; ++output)
    {
	if ((*output)->id == id)
	    return *output;
    }
    
    return NULL;
}

static GnomeRRCrtc *
crtc_by_id (ScreenInfo *info, RRCrtc id)
{
    GnomeRRCrtc **crtc;
    
    if (!info)
        return NULL;
    
    for (crtc = info->crtcs; *crtc; ++crtc)
    {
	if ((*crtc)->id == id)
	    return *crtc;
    }
    
    return NULL;
}

static GnomeRRMode *
mode_by_id (ScreenInfo *info, RRMode id)
{
    GnomeRRMode **mode;
    
    g_assert (info != NULL);
    
    for (mode = info->modes; *mode; ++mode)
    {
	if ((*mode)->id == id)
	    return *mode;
    }
    
    return NULL;
}

static void
screen_info_free (ScreenInfo *info)
{
    GnomeRROutput **output;
    GnomeRRCrtc **crtc;
    GnomeRRMode **mode;
    
    g_assert (info != NULL);
    
    if (info->resources)
    {
	XRRFreeScreenResources (info->resources);
	
	info->resources = NULL;
    }
    
    if (info->outputs)
    {
	for (output = info->outputs; *output; ++output)
	    output_free (*output);
	g_free (info->outputs);
    }
    
    if (info->crtcs)
    {
	for (crtc = info->crtcs; *crtc; ++crtc)
	    crtc_free (*crtc);
	g_free (info->crtcs);
    }
    
    if (info->modes)
    {
	for (mode = info->modes; *mode; ++mode)
	    mode_free (*mode);
	g_free (info->modes);
    }

    if (info->clone_modes)
    {
	/* The modes themselves were freed above */
	g_free (info->clone_modes);
    }
    
    g_free (info);
}

static gboolean
has_similar_mode (GnomeRROutput *output, GnomeRRMode *mode)
{
    int i;
    GnomeRRMode **modes = gnome_rr_output_list_modes (output);
    int width = gnome_rr_mode_get_width (mode);
    int height = gnome_rr_mode_get_height (mode);

    for (i = 0; modes[i] != NULL; ++i)
    {
	GnomeRRMode *m = modes[i];

	if (gnome_rr_mode_get_width (m) == width	&&
	    gnome_rr_mode_get_height (m) == height)
	{
	    return TRUE;
	}
    }

    return FALSE;
}

static void
gather_clone_modes (ScreenInfo *info)
{
    int i;
    GPtrArray *result = g_ptr_array_new ();

    for (i = 0; info->outputs[i] != NULL; ++i)
    {
	int j;
	GnomeRROutput *output1, *output2;

	output1 = info->outputs[i];
	
	if (!output1->connected)
	    continue;
	
	for (j = 0; output1->modes[j] != NULL; ++j)
	{
	    GnomeRRMode *mode = output1->modes[j];
	    gboolean valid;
	    int k;

	    valid = TRUE;
	    for (k = 0; info->outputs[k] != NULL; ++k)
	    {
		output2 = info->outputs[k];
		
		if (!output2->connected)
		    continue;
		
		if (!has_similar_mode (output2, mode))
		{
		    valid = FALSE;
		    break;
		}
	    }

	    if (valid)
		g_ptr_array_add (result, mode);
	}
    }

    g_ptr_array_add (result, NULL);
    
    info->clone_modes = (GnomeRRMode **)g_ptr_array_free (result, FALSE);
}

static gboolean
fill_out_screen_info (Display *xdisplay,
		      Window xroot,
		      ScreenInfo *info)
{
    XRRScreenResources *resources;
    
    g_assert (xdisplay != NULL);
    g_assert (info != NULL);
    
    gdk_error_trap_push ();
    
    if (!XRRGetScreenSizeRange (xdisplay, xroot,
                                &(info->min_width),
                                &(info->min_height),
                                &(info->max_width),
                                &(info->max_height))) {
        /* XRR caught an error */
        return False;
    }
    
    gdk_flush ();
    if (gdk_error_trap_pop ())
    {
        /* Unhandled X Error was generated */
        return False;
    }
    
#if 0
    g_print ("ranges: %d - %d; %d - %d\n",
	     screen->min_width, screen->max_width,
	     screen->min_height, screen->max_height);
#endif
    
    resources = XRRGetScreenResources (xdisplay, xroot);
    
    if (resources)
    {
	int i;
	GPtrArray *a;
	GnomeRRCrtc **crtc;
	GnomeRROutput **output;
	
#if 0
	g_print ("Resource Timestamp: %u\n", (guint32)resources->timestamp);
	g_print ("Resource Configuration Timestamp: %u\n", (guint32)resources->configTimestamp);
#endif
	
	info->resources = resources;
	
	/* We create all the structures before initializing them, so
	 * that they can refer to each other.
	 */
	a = g_ptr_array_new ();
	for (i = 0; i < resources->ncrtc; ++i)
	{
	    GnomeRRCrtc *crtc = crtc_new (info, resources->crtcs[i]);
	    
	    g_ptr_array_add (a, crtc);
	}
	g_ptr_array_add (a, NULL);
	info->crtcs = (GnomeRRCrtc **)g_ptr_array_free (a, FALSE);
	
	a = g_ptr_array_new ();
	for (i = 0; i < resources->noutput; ++i)
	{
	    GnomeRROutput *output = output_new (info, resources->outputs[i]);
	    
	    g_ptr_array_add (a, output);
	}
	g_ptr_array_add (a, NULL);
	info->outputs = (GnomeRROutput **)g_ptr_array_free (a, FALSE);
	
	a = g_ptr_array_new ();
	for (i = 0;  i < resources->nmode; ++i)
	{
	    GnomeRRMode *mode = mode_new (info, resources->modes[i].id);
	    
	    g_ptr_array_add (a, mode);
	}
	g_ptr_array_add (a, NULL);
	info->modes = (GnomeRRMode **)g_ptr_array_free (a, FALSE);
	
	/* Initialize */
	for (crtc = info->crtcs; *crtc; ++crtc)
	    crtc_initialize (*crtc, resources);
	
	for (output = info->outputs; *output; ++output)
	    output_initialize (*output, resources);
	
	for (i = 0; i < resources->nmode; ++i)
	{
	    GnomeRRMode *mode = mode_by_id (info, resources->modes[i].id);
	    
	    mode_initialize (mode, &(resources->modes[i]));
	}

	gather_clone_modes (info);
	
	return TRUE;
    }
    else
    {
#if 0
	g_print ("Couldn't get screen resources\n");
#endif
	
	return FALSE;
    }
}

static ScreenInfo *
screen_info_new (GnomeRRScreen *screen)
{
    ScreenInfo *info = g_new0 (ScreenInfo, 1);
    
    g_assert (screen != NULL);
    
    info->outputs = NULL;
    info->crtcs = NULL;
    info->modes = NULL;
    info->screen = screen;
    
    if (fill_out_screen_info (screen->xdisplay, screen->xroot, info))
    {
	return info;
    }
    else
    {
	g_free (info);
	return NULL;
    }
}

static gboolean
screen_update (GnomeRRScreen *screen, gboolean force_callback)
{
    ScreenInfo *info;
    gboolean changed = FALSE;
    
    g_assert (screen != NULL);
    
    info = screen_info_new (screen);
    if (info)
    {
	if (info->resources->configTimestamp != screen->info->resources->configTimestamp)
	    changed = TRUE;
	
	screen_info_free (screen->info);
	
	screen->info = info;
    }
    
    if ((changed || force_callback) && screen->callback)
	screen->callback (screen, screen->data);
    
    return changed;
}

static GdkFilterReturn
screen_on_event (GdkXEvent *xevent,
		 GdkEvent *event,
		 gpointer data)
{
    GnomeRRScreen *screen = data;
    XEvent *e = xevent;
    
    if (e && e->type - screen->randr_event_base == RRNotify)
    {
	XRRNotifyEvent *event = (XRRNotifyEvent *)e;
	
	switch (event->subtype)
	{
	default:
	    break;
	}
	
	/* FIXME: we may need to be more discriminating in
	 * what causes 'changed' events
	 */
	screen_update (screen, TRUE);
    }
    
    /* Pass the event on to GTK+ */
    return GDK_FILTER_CONTINUE;
}

/* Returns NULL if screen could not be created.  For instance, if
 * the driver does not support Xrandr 1.2.
 */
GnomeRRScreen *
gnome_rr_screen_new (GdkScreen *gdk_screen,
		     GnomeRRScreenChanged callback,
		     gpointer data)
{
    Display *dpy = GDK_SCREEN_XDISPLAY (gdk_screen);
    int event_base;
    int ignore;
    
    if (XRRQueryExtension (dpy, &event_base, &ignore))
    {
	GnomeRRScreen *screen = g_new0 (GnomeRRScreen, 1);
	
	screen->gdk_screen = gdk_screen;
	screen->gdk_root = gdk_screen_get_root_window (gdk_screen);
	screen->xroot = gdk_x11_drawable_get_xid (screen->gdk_root);
	screen->xdisplay = dpy;
	screen->xscreen = gdk_x11_screen_get_xscreen (screen->gdk_screen);
	
	screen->callback = callback;
	screen->data = data;
	
	screen->randr_event_base = event_base;
	
	screen->info = screen_info_new (screen);
	
	if (!screen->info) {
	    g_free (screen);
	    return NULL;
	}
	
	XRRSelectInput (screen->xdisplay,
			screen->xroot,
			RRScreenChangeNotifyMask	|
			RRCrtcChangeNotifyMask		|
			RROutputPropertyNotifyMask);
	
	gdk_x11_register_standard_event_type (
	    gdk_screen_get_display (gdk_screen),
	    event_base,
	    RRNotify + 1);
	
	gdk_window_add_filter (screen->gdk_root, screen_on_event, screen);
	return screen;
    }
    
    return NULL;
}

void
gnome_rr_screen_destroy (GnomeRRScreen *screen)
{
	g_return_if_fail (screen != NULL);

	gdk_window_remove_filter (screen->gdk_root, screen_on_event, screen);

	screen_info_free (screen->info);
	screen->info = NULL;

	g_free (screen);
}

void
gnome_rr_screen_set_size (GnomeRRScreen *screen,
			  int	      width,
			  int       height,
			  int       mm_width,
			  int       mm_height)
{
    g_return_if_fail (screen != NULL);
    
    XRRSetScreenSize (screen->xdisplay, screen->xroot,
		      width, height, mm_width, mm_height);
}

void
gnome_rr_screen_get_ranges (GnomeRRScreen *screen,
			    int	          *min_width,
			    int	          *max_width,
			    int           *min_height,
			    int	          *max_height)
{
    g_return_if_fail (screen != NULL);
    
    if (min_width)
	*min_width = screen->info->min_width;
    
    if (max_width)
	*max_width = screen->info->max_width;
    
    if (min_height)
	*min_height = screen->info->min_height;
    
    if (max_height)
	*max_height = screen->info->max_height;
}

gboolean
gnome_rr_screen_refresh (GnomeRRScreen *screen)
{
    return screen_update (screen, FALSE);
}

GnomeRRMode **
gnome_rr_screen_list_modes (GnomeRRScreen *screen)
{
    g_return_val_if_fail (screen != NULL, NULL);
    g_return_val_if_fail (screen->info != NULL, NULL);
    
    return screen->info->modes;
}

GnomeRRMode **
gnome_rr_screen_list_clone_modes   (GnomeRRScreen *screen)
{
    g_return_val_if_fail (screen != NULL, NULL);
    g_return_val_if_fail (screen->info != NULL, NULL);

    return screen->info->clone_modes;
}

GnomeRRCrtc **
gnome_rr_screen_list_crtcs (GnomeRRScreen *screen)
{
    g_return_val_if_fail (screen != NULL, NULL);
    g_return_val_if_fail (screen->info != NULL, NULL);
    
    return screen->info->crtcs;
}

GnomeRROutput **
gnome_rr_screen_list_outputs (GnomeRRScreen *screen)
{
    g_return_val_if_fail (screen != NULL, NULL);
    g_return_val_if_fail (screen->info != NULL, NULL);
    
    return screen->info->outputs;
}

GnomeRRCrtc *
gnome_rr_screen_get_crtc_by_id (GnomeRRScreen *screen,
				guint32        id)
{
    int i;
    
    g_return_val_if_fail (screen != NULL, NULL);
    g_return_val_if_fail (screen->info != NULL, NULL);
    
    for (i = 0; screen->info->crtcs[i] != NULL; ++i)
    {
	if (screen->info->crtcs[i]->id == id)
	    return screen->info->crtcs[i];
    }
    
    return NULL;
}

GnomeRROutput *
gnome_rr_screen_get_output_by_id (GnomeRRScreen *screen,
				  guint32        id)
{
    int i;
    
    g_return_val_if_fail (screen != NULL, NULL);
    g_return_val_if_fail (screen->info != NULL, NULL);
    
    for (i = 0; screen->info->outputs[i] != NULL; ++i)
    {
	if (screen->info->outputs[i]->id == id)
	    return screen->info->outputs[i];
    }
    
    return NULL;
}

/* GnomeRROutput */
static GnomeRROutput *
output_new (ScreenInfo *info, RROutput id)
{
    GnomeRROutput *output = g_new0 (GnomeRROutput, 1);
    
    output->id = id;
    output->info = info;
    
    return output;
}

static guint8 *
get_property (Display *dpy,
	      RROutput output,
	      Atom atom,
	      int *len)
{
    unsigned char *prop;
    int actual_format;
    unsigned long nitems, bytes_after;
    Atom actual_type;
    guint8 *result;
    
    XRRGetOutputProperty (dpy, output, atom,
			  0, 100, False, False,
			  AnyPropertyType,
			  &actual_type, &actual_format,
			  &nitems, &bytes_after, &prop);
    
    if (actual_type == XA_INTEGER && actual_format == 8)
    {
	result = g_memdup (prop, nitems);
	if (len)
	    *len = nitems;
    }
    else
    {
	result = NULL;
    }
    
    XFree (prop);
    
    return result;
}

static guint8 *
read_edid_data (GnomeRROutput *output)
{
    Atom edid_atom = XInternAtom (DISPLAY (output), "EDID_DATA", FALSE);
    guint8 *result;
    int len;
    
    result = get_property (DISPLAY (output),
			   output->id, edid_atom, &len);
    
    if (result)
    {
	if (len % 128 == 0)
	    return result;
	else
	    g_free (result);
    }
    
    return NULL;
}

static void
output_initialize (GnomeRROutput *output, XRRScreenResources *res)
{
    XRROutputInfo *info = XRRGetOutputInfo (
	DISPLAY (output), res, output->id);
    GPtrArray *a;
    int i;
    
#if 0
    g_print ("Output %lx Timestamp: %u\n", output->id, (guint32)info->timestamp);
#endif
    
    if (!info || !output->info)
    {
	/* FIXME */
	return;
    }
    
    output->name = g_strdup (info->name); /* FIXME: what is nameLen used for? */
    output->current_crtc = crtc_by_id (output->info, info->crtc);
    output->width_mm = info->mm_width;
    output->height_mm = info->mm_height;
    output->connected = (info->connection == RR_Connected);
    
    /* Possible crtcs */
    a = g_ptr_array_new ();
    
    for (i = 0; i < info->ncrtc; ++i)
    {
	GnomeRRCrtc *crtc = crtc_by_id (output->info, info->crtcs[i]);
	
	if (crtc)
	    g_ptr_array_add (a, crtc);
    }
    g_ptr_array_add (a, NULL);
    output->possible_crtcs = (GnomeRRCrtc **)g_ptr_array_free (a, FALSE);
    
    /* Clones */
    a = g_ptr_array_new ();
    for (i = 0; i < info->nclone; ++i)
    {
	GnomeRROutput *gnome_rr_output = gnome_rr_output_by_id (output->info, info->clones[i]);
	
	if (gnome_rr_output)
	    g_ptr_array_add (a, gnome_rr_output);
    }
    g_ptr_array_add (a, NULL);
    output->clones = (GnomeRROutput **)g_ptr_array_free (a, FALSE);
    
    /* Modes */
    a = g_ptr_array_new ();
    for (i = 0; i < info->nmode; ++i)
    {
	GnomeRRMode *mode = mode_by_id (output->info, info->modes[i]);
	
	if (mode)
	    g_ptr_array_add (a, mode);
    }
    g_ptr_array_add (a, NULL);
    output->modes = (GnomeRRMode **)g_ptr_array_free (a, FALSE);
    
    output->n_preferred = info->npreferred;
    
    /* Edid data */
    output->edid_data = read_edid_data (output);
    
    XRRFreeOutputInfo (info);
}

static void
output_free (GnomeRROutput *output)
{
    g_free (output->clones);
    g_free (output->modes);
    g_free (output->possible_crtcs);
    g_free (output->edid_data);
    g_free (output->name);
    g_free (output);
}

guint32
gnome_rr_output_get_id (GnomeRROutput *output)
{
    g_assert(output != NULL);
    
    return output->id;
}

const guint8 *
gnome_rr_output_get_edid_data (GnomeRROutput *output)
{
    g_return_val_if_fail (output != NULL, NULL);
    
    return output->edid_data;
}

GnomeRROutput *
gnome_rr_screen_get_output_by_name (GnomeRRScreen *screen,
				    const char    *name)
{
    int i;
    
    g_return_val_if_fail (screen != NULL, NULL);
    g_return_val_if_fail (screen->info != NULL, NULL);
    
    for (i = 0; screen->info->outputs[i] != NULL; ++i)
    {
	GnomeRROutput *output = screen->info->outputs[i];
	
	if (strcmp (output->name, name) == 0)
	    return output;
    }
    
    return NULL;
}

GnomeRRCrtc *
gnome_rr_output_get_crtc (GnomeRROutput *output)
{
    g_return_val_if_fail (output != NULL, NULL);
    
    return output->current_crtc;
}

GnomeRRMode *
gnome_rr_output_get_current_mode (GnomeRROutput *output)
{
    GnomeRRCrtc *crtc;
    
    g_return_val_if_fail (output != NULL, NULL);
    
    if ((crtc = gnome_rr_output_get_crtc (output)))
	return gnome_rr_crtc_get_current_mode (crtc);
    
    return NULL;
}

void
gnome_rr_output_get_position (GnomeRROutput   *output,
			      int             *x,
			      int             *y)
{
    GnomeRRCrtc *crtc;
    
    g_return_if_fail (output != NULL);
    
    if ((crtc = gnome_rr_output_get_crtc (output)))
	gnome_rr_crtc_get_position (crtc, x, y);
}

const char *
gnome_rr_output_get_name (GnomeRROutput *output)
{
    g_assert (output != NULL);
    return output->name;
}

int
gnome_rr_output_get_width_mm (GnomeRROutput *output)
{
    g_assert (output != NULL);
    return output->width_mm;
}

int
gnome_rr_output_get_height_mm (GnomeRROutput *output)
{
    g_assert (output != NULL);
    return output->height_mm;
}

GnomeRRMode *
gnome_rr_output_get_preferred_mode (GnomeRROutput *output)
{
    g_return_val_if_fail (output != NULL, NULL);
    if (output->n_preferred)
	return output->modes[0];
    
    return NULL;
}

GnomeRRMode **
gnome_rr_output_list_modes (GnomeRROutput *output)
{
    g_return_val_if_fail (output != NULL, NULL);
    return output->modes;
}

gboolean
gnome_rr_output_is_connected (GnomeRROutput *output)
{
    g_return_val_if_fail (output != NULL, FALSE);
    return output->connected;
}

gboolean
gnome_rr_output_supports_mode (GnomeRROutput *output,
			       GnomeRRMode   *mode)
{
    int i;
    
    g_return_val_if_fail (output != NULL, FALSE);
    g_return_val_if_fail (mode != NULL, FALSE);
    
    for (i = 0; output->modes[i] != NULL; ++i)
    {
	if (output->modes[i] == mode)
	    return TRUE;
    }
    
    return FALSE;
}

gboolean
gnome_rr_output_can_clone (GnomeRROutput *output,
			   GnomeRROutput *clone)
{
    int i;
    
    g_return_val_if_fail (output != NULL, FALSE);
    g_return_val_if_fail (clone != NULL, FALSE);
    
    for (i = 0; output->clones[i] != NULL; ++i)
    {
	if (output->clones[i] == clone)
	    return TRUE;
    }
    
    return FALSE;
}

/* GnomeRRCrtc */
typedef struct
{
    Rotation xrot;
    GnomeRRRotation rot;
} RotationMap;

static const RotationMap rotation_map[] =
{
    { RR_Rotate_0, GNOME_RR_ROTATION_0 },
    { RR_Rotate_90, GNOME_RR_ROTATION_90 },
    { RR_Rotate_180, GNOME_RR_ROTATION_180 },
    { RR_Rotate_270, GNOME_RR_ROTATION_270 },
    { RR_Reflect_X, GNOME_RR_REFLECT_X },
    { RR_Reflect_Y, GNOME_RR_REFLECT_Y },
};

static GnomeRRRotation
gnome_rr_rotation_from_xrotation (Rotation r)
{
    int i;
    GnomeRRRotation result = 0;
    
    for (i = 0; i < G_N_ELEMENTS (rotation_map); ++i)
    {
	if (r & rotation_map[i].xrot)
	    result |= rotation_map[i].rot;
    }
    
    return result;
}

static Rotation
xrotation_from_rotation (GnomeRRRotation r)
{
    int i;
    Rotation result = 0;
    
    for (i = 0; i < G_N_ELEMENTS (rotation_map); ++i)
    {
	if (r & rotation_map[i].rot)
	    result |= rotation_map[i].xrot;
    }
    
    return result;
}

gboolean
gnome_rr_crtc_set_config (GnomeRRCrtc      *crtc,
			  int               x,
			  int               y,
			  GnomeRRMode      *mode,
			  GnomeRRRotation   rotation,
			  GnomeRROutput   **outputs,
			  int               n_outputs)
{
    ScreenInfo *info;
    GArray *output_ids;
    gboolean result;
    int i;
    
    g_return_val_if_fail (crtc != NULL, FALSE);
    g_return_val_if_fail (mode != NULL || outputs == NULL || n_outputs == 0, FALSE);
    
    info = crtc->info;
    
    if (mode)
    {
	g_return_val_if_fail (x + mode->width <= info->max_width, FALSE);
	g_return_val_if_fail (y + mode->height <= info->max_height, FALSE);
    }
    
    output_ids = g_array_new (FALSE, FALSE, sizeof (RROutput));
    
    if (outputs)
    {
	for (i = 0; i < n_outputs; ++i)
	    g_array_append_val (output_ids, outputs[i]->id);
    }
    
    result = XRRSetCrtcConfig (DISPLAY (crtc), info->resources, crtc->id,
			       CurrentTime, 
			       x, y,
			       mode ? mode->id : None,
			       xrotation_from_rotation (rotation),
			       (RROutput *)output_ids->data,
			       output_ids->len) == RRSetConfigSuccess;
    
    g_array_free (output_ids, TRUE);
    
    return result;
}

GnomeRRMode *
gnome_rr_crtc_get_current_mode (GnomeRRCrtc *crtc)
{
    g_return_val_if_fail (crtc != NULL, NULL);
    
    return crtc->current_mode;
}

guint32
gnome_rr_crtc_get_id (GnomeRRCrtc *crtc)
{
    g_return_val_if_fail (crtc != NULL, 0);
    
    return crtc->id;
}

gboolean
gnome_rr_crtc_can_drive_output (GnomeRRCrtc   *crtc,
				GnomeRROutput *output)
{
    int i;
    
    g_return_val_if_fail (crtc != NULL, FALSE);
    g_return_val_if_fail (output != NULL, FALSE);
    
    for (i = 0; crtc->possible_outputs[i] != NULL; ++i)
    {
	if (crtc->possible_outputs[i] == output)
	    return TRUE;
    }
    
    return FALSE;
}

/* FIXME: merge with get_mode()? */
void
gnome_rr_crtc_get_position (GnomeRRCrtc *crtc,
			    int         *x,
			    int         *y)
{
    g_return_if_fail (crtc != NULL);
    
    if (x)
	*x = crtc->x;
    
    if (y)
	*y = crtc->y;
}

/* FIXME: merge with get_mode()? */
GnomeRRRotation
gnome_rr_crtc_get_current_rotation (GnomeRRCrtc *crtc)
{
    g_assert(crtc != NULL);
    return crtc->current_rotation;
}

GnomeRRRotation
gnome_rr_crtc_get_rotations (GnomeRRCrtc *crtc)
{
    g_assert(crtc != NULL);
    return crtc->rotations;
}

gboolean
gnome_rr_crtc_supports_rotation (GnomeRRCrtc *   crtc,
				 GnomeRRRotation rotation)
{
    g_return_val_if_fail (crtc != NULL, FALSE);
    return (crtc->rotations & rotation);
}

static GnomeRRCrtc *
crtc_new (ScreenInfo *info, RROutput id)
{
    GnomeRRCrtc *crtc = g_new0 (GnomeRRCrtc, 1);
    
    crtc->id = id;
    crtc->info = info;
    
    return crtc;
}

static void
crtc_initialize (GnomeRRCrtc        *crtc,
		 XRRScreenResources *res)
{
    XRRCrtcInfo *info = XRRGetCrtcInfo (DISPLAY (crtc), res, crtc->id);
    GPtrArray *a;
    int i;
    
#if 0
    g_print ("CRTC %lx Timestamp: %u\n", crtc->id, (guint32)info->timestamp);
#endif
    
    if (!info)
    {
	/* FIXME: We need to reaquire the screen resources */
	return;
    }
    
    /* GnomeRRMode */
    crtc->current_mode = mode_by_id (crtc->info, info->mode);
    
    crtc->x = info->x;
    crtc->y = info->y;
    
    /* Current outputs */
    a = g_ptr_array_new ();
    for (i = 0; i < info->noutput; ++i)
    {
	GnomeRROutput *output = gnome_rr_output_by_id (crtc->info, info->outputs[i]);
	
	if (output)
	    g_ptr_array_add (a, output);
    }
    g_ptr_array_add (a, NULL);
    crtc->current_outputs = (GnomeRROutput **)g_ptr_array_free (a, FALSE);
    
    /* Possible outputs */
    a = g_ptr_array_new ();
    for (i = 0; i < info->npossible; ++i)
    {
	GnomeRROutput *output = gnome_rr_output_by_id (crtc->info, info->possible[i]);
	
	if (output)
	    g_ptr_array_add (a, output);
    }
    g_ptr_array_add (a, NULL);
    crtc->possible_outputs = (GnomeRROutput **)g_ptr_array_free (a, FALSE);
    
    /* Rotations */
    crtc->current_rotation = gnome_rr_rotation_from_xrotation (info->rotation);
    crtc->rotations = gnome_rr_rotation_from_xrotation (info->rotations);
    
    XRRFreeCrtcInfo (info);
}

static void
crtc_free (GnomeRRCrtc *crtc)
{
    g_free (crtc->current_outputs);
    g_free (crtc->possible_outputs);
    g_free (crtc);
}

/* GnomeRRMode */
static GnomeRRMode *
mode_new (ScreenInfo *info, RRMode id)
{
    GnomeRRMode *mode = g_new0 (GnomeRRMode, 1);
    
    mode->id = id;
    mode->info = info;
    
    return mode;
}

guint32
gnome_rr_mode_get_id (GnomeRRMode *mode)
{
    g_return_val_if_fail (mode != NULL, 0);
    return mode->id;
}

guint
gnome_rr_mode_get_width (GnomeRRMode *mode)
{
    g_return_val_if_fail (mode != NULL, 0);
    return mode->width;
}

int
gnome_rr_mode_get_freq (GnomeRRMode *mode)
{
    g_return_val_if_fail (mode != NULL, 0);
    return (mode->freq) / 1000;
}

guint
gnome_rr_mode_get_height (GnomeRRMode *mode)
{
    g_return_val_if_fail (mode != NULL, 0);
    return mode->height;
}

static void
mode_initialize (GnomeRRMode *mode, XRRModeInfo *info)
{
    g_assert (mode != NULL);
    g_assert (info != NULL);
    
    mode->name = g_strdup (info->name);
    mode->width = info->width;
    mode->height = info->height;
    mode->freq = ((info->dotClock / (double)info->hTotal) / info->vTotal + 0.5) * 1000;
}

static void
mode_free (GnomeRRMode *mode)
{
    g_free (mode->name);
    g_free (mode);
}
