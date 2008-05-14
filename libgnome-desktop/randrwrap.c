#include "libgnomeui/randrwrap.h"
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
    
    RWOutput **		outputs;
    RWCrtc **		crtcs;
    RWMode **		modes;

    RWScreen *		screen;
};

struct RWScreen
{
    GdkScreen *		gdk_screen;
    GdkWindow *		gdk_root;
    Display *		xdisplay;
    Screen *		xscreen;
    Window		xroot;
    ScreenInfo *	info;
   
    int			randr_event_base;

    RWScreenChanged	callback;
    gpointer		data;
};

struct RWOutput
{
    ScreenInfo *	info;
    RROutput		id;
    
    char *		name;
    RWCrtc *		current_crtc;
    gboolean		connected;
    gulong		width_mm;
    gulong		height_mm;
    RWCrtc **		possible_crtcs;
    RWOutput **		clones;
    RWMode **		modes;
    int			n_preferred;
    guint8 *		edid_data;
};

struct RWOutputWrap
{
    RROutput		id;
};

struct RWCrtc
{
    ScreenInfo *	info;
    RRCrtc		id;
    
    RWMode *		current_mode;
    RWOutput **		current_outputs;
    RWOutput **		possible_outputs;
    int			x;
    int			y;

    RWRotation		current_rotation;
    RWRotation		rotations;
};

struct RWMode
{
    ScreenInfo *	info;
    RRMode		id;
    char *		name;
    int			width;
    int			height;
    int			freq;		/* in mHz */
};

/* RWCrtc */
static RWCrtc *  crtc_new             (ScreenInfo         *info,
				       RRCrtc              id);
static void      crtc_free            (RWCrtc             *crtc);
static void      crtc_initialize      (RWCrtc             *crtc,
				       XRRScreenResources *res);


/* RWOutput */
static RWOutput *output_new           (ScreenInfo         *info,
				       RROutput            id);
static void      output_initialize    (RWOutput           *output,
				       XRRScreenResources *res);
static void      output_free          (RWOutput           *output);


/* RWMode */
static RWMode *  mode_new             (ScreenInfo         *info,
				       RRMode              id);
static void      mode_initialize      (RWMode             *mode,
				       XRRModeInfo        *info);
static void      mode_free            (RWMode             *mode);


/* Screen */
static RWOutput *
rw_output_by_id (ScreenInfo *info, RROutput id)
{
    RWOutput **output;

    g_assert (info != NULL);

    for (output = info->outputs; *output; ++output)
    {
	if ((*output)->id == id)
	    return *output;
    }
    
    return NULL;
}

static RWCrtc *
crtc_by_id (ScreenInfo *info, RRCrtc id)
{
    RWCrtc **crtc;

    if (!info)
        return NULL;

    for (crtc = info->crtcs; *crtc; ++crtc)
    {
	if ((*crtc)->id == id)
	    return *crtc;
    }
    
    return NULL;
}

static RWMode *
mode_by_id (ScreenInfo *info, RRMode id)
{
    RWMode **mode;

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
    RWOutput **output;
    RWCrtc **crtc;
    RWMode **mode;

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

    g_free (info);
}

static gboolean
fill_out_screen_info (Display *xdisplay, Window xroot,
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
	RWCrtc **crtc;
	RWOutput **output;

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
	    RWCrtc *crtc = crtc_new (info, resources->crtcs[i]);
	    
	    g_ptr_array_add (a, crtc);
	}
	g_ptr_array_add (a, NULL);
	info->crtcs = (RWCrtc **)g_ptr_array_free (a, FALSE);
	
	a = g_ptr_array_new ();
	for (i = 0; i < resources->noutput; ++i)
	{
	    RWOutput *output = output_new (info, resources->outputs[i]);
	    
	    g_ptr_array_add (a, output);
	}
	g_ptr_array_add (a, NULL);
	info->outputs = (RWOutput **)g_ptr_array_free (a, FALSE);
	
	a = g_ptr_array_new ();
	for (i = 0;  i < resources->nmode; ++i)
	{
	    RWMode *mode = mode_new (info, resources->modes[i].id);
	    
	    g_ptr_array_add (a, mode);
	}
	g_ptr_array_add (a, NULL);
	info->modes = (RWMode **)g_ptr_array_free (a, FALSE);
	
	/* Initialize */
	for (crtc = info->crtcs; *crtc; ++crtc)
	    crtc_initialize (*crtc, resources);
	
	for (output = info->outputs; *output; ++output)
	    output_initialize (*output, resources);
	
	for (i = 0; i < resources->nmode; ++i)
	{
	    RWMode *mode = mode_by_id (info, resources->modes[i].id);
	    
	    mode_initialize (mode, &(resources->modes[i]));
	}

	return TRUE;
    }
    else
    {
	g_print ("Couldn't get screen resources\n");

	return FALSE;
    }
}

static ScreenInfo *
screen_info_new (RWScreen *screen)
{
    ScreenInfo *info = g_new0 (ScreenInfo, 1);
    RWOutput **o;

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

    for (o = info->outputs; *o; o++)
    {
	
    }
    
}

static gboolean
screen_update (RWScreen *screen, gboolean force_callback)
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
    RWScreen *screen = data;
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

/* Returns NULL if screen could not be created.  For instance, if driver
 * does not support Xrandr 1.2.
 */
RWScreen *
rw_screen_new (GdkScreen *gdk_screen,
	       RWScreenChanged callback,
	       gpointer data)
{
    Display *dpy = GDK_SCREEN_XDISPLAY (gdk_screen);
    int event_base;
    int ignore;
    
    if (XRRQueryExtension (dpy, &event_base, &ignore))
    {
	RWScreen *screen = g_new0 (RWScreen, 1);

	screen->gdk_screen = gdk_screen;
	screen->gdk_root = gdk_screen_get_root_window (gdk_screen);
	screen->xroot = gdk_x11_drawable_get_xid (screen->gdk_root);
	screen->xdisplay = dpy;
	screen->xscreen = gdk_x11_screen_get_xscreen (screen->gdk_screen);

	screen->callback = callback;
	screen->data = data;
	
	screen->randr_event_base = event_base;

	screen->info = screen_info_new (screen);

	if (!screen->info)
	    return NULL;
	
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
rw_screen_set_size (RWScreen *screen,
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
rw_screen_get_ranges (RWScreen	*screen,
		      int	*min_width,
		      int	*max_width,
		      int       *min_height,
		      int	*max_height)
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
rw_screen_refresh (RWScreen *screen)
{
    return screen_update (screen, FALSE);
}

RWMode **
rw_screen_list_modes (RWScreen *screen)
{
    g_return_val_if_fail (screen != NULL, NULL);
    g_return_val_if_fail (screen->info != NULL, NULL);

    return screen->info->modes;
}

RWCrtc **
rw_screen_list_crtcs (RWScreen *screen)
{
    g_return_val_if_fail (screen != NULL, NULL);
    g_return_val_if_fail (screen->info != NULL, NULL);

    return screen->info->crtcs;
}

RWOutput **
rw_screen_list_outputs (RWScreen *screen)
{
    g_return_val_if_fail (screen != NULL, NULL);
    g_return_val_if_fail (screen->info != NULL, NULL);

    return screen->info->outputs;
}

RWCrtc *
rw_screen_get_crtc_by_id (RWScreen *screen,
			  guint32   id)
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

RWOutput *
rw_screen_get_output_by_id (RWScreen *screen,
			    guint32 id)
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

/* RWOutput */
static RWOutput *
output_new (ScreenInfo *info, RROutput id)
{
    RWOutput *output = g_new0 (RWOutput, 1);

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
read_edid_data (RWOutput *output)
{
    Atom edid_atom = XInternAtom (DISPLAY (output), "EDID_DATA", FALSE);
    guint8 *result;
    int len;

    result = get_property (DISPLAY (output),
			   output->id, edid_atom, &len);

    if (result)
    {
	if (len == 128)
	    return result;
	else
	    g_free (result);
    }

    return NULL;
}

static void
output_initialize (RWOutput *output, XRRScreenResources *res)
{
    XRROutputInfo *info = XRRGetOutputInfo (
	DISPLAY (output), res, output->id);
    GPtrArray *a;
    int i;
    
    g_print ("Output %lx Timestamp: %u\n", output->id, (guint32)info->timestamp);
	
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
	RWCrtc *crtc = crtc_by_id (output->info, info->crtcs[i]);
	
	if (crtc)
	    g_ptr_array_add (a, crtc);
    }
    g_ptr_array_add (a, NULL);
    output->possible_crtcs = (RWCrtc **)g_ptr_array_free (a, FALSE);
    
    /* Clones */
    a = g_ptr_array_new ();
    for (i = 0; i < info->nclone; ++i)
    {
	RWOutput *rw_output = rw_output_by_id (output->info, info->clones[i]);
	
	if (rw_output)
	    g_ptr_array_add (a, rw_output);
    }
    g_ptr_array_add (a, NULL);
    output->clones = (RWOutput **)g_ptr_array_free (a, FALSE);
    
    /* Modes */
    a = g_ptr_array_new ();
    for (i = 0; i < info->nmode; ++i)
    {
	RWMode *mode = mode_by_id (output->info, info->modes[i]);
	
	if (mode)
	    g_ptr_array_add (a, mode);
    }
    g_ptr_array_add (a, NULL);
    output->modes = (RWMode **)g_ptr_array_free (a, FALSE);

    output->n_preferred = info->npreferred;
    
    /* Edid data */
    output->edid_data = read_edid_data (output);
    
    XRRFreeOutputInfo (info);
}

static void
output_free (RWOutput *output)
{
    g_free (output);
}

guint32
rw_output_get_id (RWOutput *output)
{
    g_assert(output != NULL);

    return output->id;
}

const guint8 *
rw_output_get_edid_data (RWOutput *output)
{
    g_return_val_if_fail (output != NULL, NULL);

    return output->edid_data;
}

RWOutput *
rw_screen_get_output_by_name (RWScreen        *screen,
			      const char      *name)
{
    int i;

    g_return_val_if_fail (screen != NULL, NULL);
    g_return_val_if_fail (screen->info != NULL, NULL);

    for (i = 0; screen->info->outputs[i] != NULL; ++i)
    {
	RWOutput *output = screen->info->outputs[i];

	if (strcmp (output->name, name) == 0)
	    return output;
    }

    return NULL;
}

RWCrtc *
rw_output_get_crtc (RWOutput *output)
{
    g_return_val_if_fail (output != NULL, NULL);

    return output->current_crtc;
}

RWMode *
rw_output_get_current_mode (RWOutput *output)
{
    RWCrtc *crtc;

    g_return_val_if_fail (output != NULL, NULL);

    if ((crtc = rw_output_get_crtc (output)))
	return rw_crtc_get_current_mode (crtc);

    return NULL;
}

void
rw_output_get_position (RWOutput        *output,
			int             *x,
			int             *y)
{
    RWCrtc *crtc;

    g_return_if_fail (output != NULL);

    if ((crtc = rw_output_get_crtc (output)))
	rw_crtc_get_position (crtc, x, y);
}

const char *
rw_output_get_name (RWOutput *output)
{
    g_assert (output != NULL);
    return output->name;
}

int
rw_output_get_width_mm (RWOutput *output)
{
    g_assert (output != NULL);
    return output->width_mm;
}

int
rw_output_get_height_mm (RWOutput *output)
{
    g_assert (output != NULL);
    return output->height_mm;
}

RWMode *
rw_output_get_preferred_mode (RWOutput *output)
{
    g_return_val_if_fail (output != NULL, NULL);
    if (output->n_preferred)
	return output->modes[0];

    return NULL;
}

RWMode **
rw_output_list_modes (RWOutput *output)
{
    g_return_val_if_fail (output != NULL, NULL);
    return output->modes;
}

gboolean
rw_output_is_connected (RWOutput *output)
{
    g_return_val_if_fail (output != NULL, FALSE);
    return output->connected;
}

gboolean
rw_output_supports_mode (RWOutput *output,
			 RWMode   *mode)
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
rw_output_can_clone (RWOutput *output,
		     RWOutput *clone)
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

/* RWCrtc */
typedef struct
{
    Rotation xrot;
    RWRotation rot;
} RotationMap;
static const RotationMap rotation_map[] =
{
    { RR_Rotate_0, RW_ROTATION_0 },
    { RR_Rotate_90, RW_ROTATION_90 },
    { RR_Rotate_180, RW_ROTATION_180 },
    { RR_Rotate_270, RW_ROTATION_270 },
    { RR_Reflect_X, RW_REFLECT_X },
    { RR_Reflect_Y, RW_REFLECT_Y },
};

static RWRotation
rw_rotation_from_xrotation (Rotation r)
{
    int i;
    RWRotation result = 0;

    for (i = 0; i < G_N_ELEMENTS (rotation_map); ++i)
    {
	if (r & rotation_map[i].xrot)
	    result |= rotation_map[i].rot;
    }

    return result;
}

static Rotation
xrotation_from_rotation (RWRotation r)
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
rw_crtc_set_config (RWCrtc    *crtc,
		    int        x,
		    int        y,
		    RWMode    *mode,
		    RWRotation rotation,
		    RWOutput **outputs,
		    int        n_outputs)
{
    ScreenInfo *info;
    GArray *output_ids;
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
    
    XRRSetCrtcConfig (DISPLAY (crtc), info->resources, crtc->id,
		      CurrentTime, 
		      x, y,
		      mode? mode->id : None,
		      xrotation_from_rotation (rotation),
		      (RROutput *)output_ids->data,
		      output_ids->len);

    g_array_free (output_ids, TRUE);
    
    return TRUE;
}

RWMode *
rw_crtc_get_current_mode (RWCrtc *crtc)
{
    g_return_val_if_fail (crtc != NULL, NULL);

    return crtc->current_mode;
}

guint32
rw_crtc_get_id (RWCrtc *crtc)
{
    g_return_val_if_fail (crtc != NULL, 0);

    return crtc->id;
}

gboolean
rw_crtc_can_drive_output (RWCrtc   *crtc,
			  RWOutput *output)
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
rw_crtc_get_position (RWCrtc          *crtc,
		      int             *x,
		      int             *y)
{
    g_return_if_fail (crtc != NULL);
    
    if (x)
	*x = crtc->x;

    if (y)
	*y = crtc->y;
}

/* FIXME: merge with get_mode()? */
RWRotation
rw_crtc_get_current_rotation (RWCrtc *crtc)
{
    g_assert(crtc != NULL);
    return crtc->current_rotation;
}

RWRotation
rw_crtc_get_rotations (RWCrtc *crtc)
{
    g_assert(crtc != NULL);
    return crtc->rotations;
}

gboolean
rw_crtc_supports_rotation (RWCrtc *   crtc,
			   RWRotation rotation)
{
    g_return_val_if_fail (crtc != NULL, FALSE);
    return (crtc->rotations & rotation);
}

static RWCrtc *
crtc_new (ScreenInfo *info, RROutput id)
{
    RWCrtc *crtc = g_new0 (RWCrtc, 1);

    crtc->id = id;
    crtc->info = info;
    
    return crtc;
}

static void
crtc_initialize (RWCrtc *crtc, XRRScreenResources *res)
{
    XRRCrtcInfo *info = XRRGetCrtcInfo (DISPLAY (crtc), res, crtc->id);
    GPtrArray *a;
    int i;
    
    g_print ("CRTC %lx Timestamp: %u\n", crtc->id, (guint32)info->timestamp);
	
    if (!info)
    {
	/* FIXME: We need to reaquire the screen resources */
	return;
    }
    
    /* RWMode */
    crtc->current_mode = mode_by_id (crtc->info, info->mode);
    
    crtc->x = info->x;
    crtc->y = info->y;
    
    /* Current outputs */
    a = g_ptr_array_new ();
    for (i = 0; i < info->noutput; ++i)
    {
	RWOutput *output = rw_output_by_id (crtc->info, info->outputs[i]);
	
	if (output)
	    g_ptr_array_add (a, output);
    }
    g_ptr_array_add (a, NULL);
    crtc->current_outputs = (RWOutput **)g_ptr_array_free (a, FALSE);
    
    /* Possible outputs */
    a = g_ptr_array_new ();
    for (i = 0; i < info->npossible; ++i)
    {
	RWOutput *output = rw_output_by_id (crtc->info, info->possible[i]);
	
	if (output)
	    g_ptr_array_add (a, output);
    }
    g_ptr_array_add (a, NULL);
    crtc->possible_outputs = (RWOutput **)g_ptr_array_free (a, FALSE);

    /* Rotations */
    crtc->current_rotation = rw_rotation_from_xrotation (info->rotation);
    crtc->rotations = rw_rotation_from_xrotation (info->rotations);

    XRRFreeCrtcInfo (info);
}

static void
crtc_free (RWCrtc *crtc)
{
    g_free (crtc->current_outputs);
    g_free (crtc->possible_outputs);
    g_free (crtc);
}

/* RWMode */
static RWMode *
mode_new (ScreenInfo *info, RRMode id)
{
    RWMode *mode = g_new0 (RWMode, 1);
    
    mode->id = id;
    mode->info = info;
    
    return mode;
}

guint32
rw_mode_get_id (RWMode *mode)
{
    g_return_val_if_fail (mode != NULL, 0);
    return mode->id;
}

guint
rw_mode_get_width (RWMode *mode)
{
    g_return_val_if_fail (mode != NULL, 0);
    return mode->width;
}

int
rw_mode_get_freq (RWMode *mode)
{
    g_return_val_if_fail (mode != NULL, 0);
    return (mode->freq) / 1000;
}

guint
rw_mode_get_height (RWMode *mode)
{
    g_return_val_if_fail (mode != NULL, 0);
    return mode->height;
}

static void
mode_initialize (RWMode *mode, XRRModeInfo *info)
{
    g_assert (mode != NULL);
    g_assert (info != NULL);

    mode->name = g_strdup (info->name);
    mode->width = info->width;
    mode->height = info->height;
    mode->freq = ((info->dotClock / (double)info->hTotal) / info->vTotal + 0.5) * 1000;
}

static void
mode_free (RWMode *mode)
{
    g_free (mode->name);
    g_free (mode);
}


#ifdef INCLUDE_MAIN
static void
on_screen_changed (RWScreen *screen, gpointer data)
{
    g_print ("Changed\n");
}

static gboolean
do_refresh (gpointer data)
{
    RWScreen *screen = data;

    rw_screen_refresh (screen);

    return TRUE;
}

int
main (int argc, char **argv)
{
    int i;
    
    gtk_init (&argc, &argv);
    
    RWScreen *screen = rw_screen_new (gdk_screen_get_default(),
				   on_screen_changed,
				   NULL);
    
    for (i = 0; screen->info->crtcs[i]; ++i)
    {
	RWCrtc *crtc = screen->info->crtcs[i];
	
	if (crtc->current_mode)
	{
	    g_print ("CRTC %p: (%d %d %d %d)\n",
		     crtc, crtc->x, crtc->y,
		     crtc->current_mode->width, crtc->current_mode->height);
	}
	else
	{
	    g_print ("CRTC %p: turned off\n", crtc);
	}
    }
    
    for (i = 0; screen->info->outputs[i]; ++i)
    {
	RWOutput *output = screen->info->outputs[i];
	
	g_print ("Output %s currently", output->name);
	
	if (!output->current_crtc)
	    g_print (" turned off\n");
	else
	    g_print (" driven by CRTC %p\n", output->current_crtc);
    }

    g_timeout_add (500, do_refresh, screen);
    
    gtk_main ();
    
    return 0;
}
#endif
