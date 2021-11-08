/* gnome-rr-config.c: Display configuration
 *
 * SPDX-FileCopyrightText: 2007, 2008, 2013 Red Hat, Inc.
 * SPDX-FileCopyrightText: 2010 Giovanni Campagna <scampa.giovanni@gmail.com>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 * 
 * Author: Soren Sandmann <sandmann@redhat.com>
 */

#define GNOME_DESKTOP_USE_UNSTABLE_API

#include "config.h"

#include "gnome-rr-config.h"

#include "gnome-rr-output-info.h"
#include "gnome-rr-private.h"

#include <glib/gi18n-lib.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>

#define CONFIG_INTENDED_BASENAME "monitors.xml"
#define CONFIG_BACKUP_BASENAME "monitors.xml.backup"

/* Look for DPI_FALLBACK in:
 * http://git.gnome.org/browse/gnome-settings-daemon/tree/plugins/xsettings/gsd-xsettings-manager.c
 * for the reasoning */
#define DPI_FALLBACK 96.0

/* In version 0 of the config file format, we had several <configuration>
 * toplevel elements and no explicit version number.  So, the filed looked
 * like
 *
 *   <configuration>
 *     ...
 *   </configuration>
 *   <configuration>
 *     ...
 *   </configuration>
 *
 * Since version 1 of the config file, the file has a toplevel <monitors>
 * element to group all the configurations.  That element has a "version"
 * attribute which is an integer. So, the file looks like this:
 *
 *   <monitors version="1">
 *     <configuration>
 *       ...
 *     </configuration>
 *     <configuration>
 *       ...
 *     </configuration>
 *   </monitors>
 */

typedef struct CrtcAssignment CrtcAssignment;

static gboolean         crtc_assignment_apply (CrtcAssignment   *assign,
					       gboolean          persistent,
					       GError          **error);
static CrtcAssignment  *crtc_assignment_new   (GnomeRRScreen      *screen,
					       GnomeRROutputInfo **outputs,
					       GError            **error);
static void             crtc_assignment_free  (CrtcAssignment   *assign);

enum {
  PROP_0,
  PROP_SCREEN,
  PROP_LAST
};

G_DEFINE_TYPE (GnomeRRConfig, gnome_rr_config, G_TYPE_OBJECT)

static void
gnome_rr_config_init (GnomeRRConfig *self)
{
    self->clone = FALSE;
    self->screen = NULL;
    self->outputs = NULL;
}

static void
gnome_rr_config_set_property (GObject *gobject,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *property)
{
    GnomeRRConfig *self = GNOME_RR_CONFIG (gobject);

    switch (property_id) {
	case PROP_SCREEN:
	    self->screen = g_value_dup_object (value);
	    return;
	default:
	    G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, property_id, property);
    }
}

static void
gnome_rr_config_finalize (GObject *gobject)
{
    GnomeRRConfig *self = GNOME_RR_CONFIG (gobject);

    g_clear_object (&self->screen);

    if (self->outputs) {
        for (int i = 0; self->outputs[i] != NULL; i++) {
	    GnomeRROutputInfo *output = self->outputs[i];
	    g_object_unref (output);
	}

	g_free (self->outputs);
    }

    G_OBJECT_CLASS (gnome_rr_config_parent_class)->finalize (gobject);
}

gboolean
gnome_rr_config_load_current (GnomeRRConfig *config,
                              GError **error)
{
    GPtrArray *a;
    GnomeRROutput **rr_outputs;
    int clone_width = -1;
    int clone_height = -1;
    int last_x;

    g_return_val_if_fail (GNOME_RR_IS_CONFIG (config), FALSE);

    a = g_ptr_array_new ();
    rr_outputs = gnome_rr_screen_list_outputs (config->screen);

    config->clone = FALSE;
    
    for (int i = 0; rr_outputs[i] != NULL; ++i) {
	GnomeRROutput *rr_output = rr_outputs[i];
	GnomeRROutputInfo *output = g_object_new (GNOME_RR_TYPE_OUTPUT_INFO, NULL);
	GnomeRRMode *mode = NULL;
	GnomeRRCrtc *crtc;

	output->name = g_strdup (gnome_rr_output_get_name (rr_output));
	output->connected = TRUE;
	output->display_name = g_strdup (gnome_rr_output_get_display_name (rr_output));
	output->connector_type = g_strdup (gnome_rr_output_get_connector_type (rr_output));
	output->config = config;
	output->is_tiled = gnome_rr_output_get_tile_info (rr_output, &output->tile);
	if (output->is_tiled) {
            gnome_rr_output_get_tiled_display_size (rr_output,
                                                    NULL,
                                                    NULL,
                                                    &output->total_tiled_width,
                                                    &output->total_tiled_height);
	}

	if (!output->connected)	{
	    output->x = -1;
	    output->y = -1;
	    output->width = -1;
	    output->height = -1;
	    output->rate = -1;
	} else {
	    gnome_rr_output_get_ids_from_edid (rr_output,
					       &output->vendor,
					       &output->product,
					       &output->serial);
		
	    crtc = gnome_rr_output_get_crtc (rr_output);
	    mode = crtc ? gnome_rr_crtc_get_current_mode (crtc) : NULL;

	    if (crtc && mode) {
		output->active = TRUE;

		gnome_rr_crtc_get_position (crtc, &output->x, &output->y);

		output->width = gnome_rr_mode_get_width (mode);
		output->height = gnome_rr_mode_get_height (mode);
		output->rate = gnome_rr_mode_get_freq (mode);
		output->rotation = gnome_rr_crtc_get_current_rotation (crtc);
                output->available_rotations = gnome_rr_crtc_get_rotations (crtc);

		if (output->x == 0 && output->y == 0) {
		    if (clone_width == -1) {
			clone_width = output->width;
			clone_height = output->height;
		    } else if (clone_width == output->width &&
			       clone_height == output->height) {
			config->clone = TRUE;
		    }
		}
	    } else {
		output->active = FALSE;
		config->clone = FALSE;
	    }

	    /* Get preferred size for the monitor */
	    mode = gnome_rr_output_get_preferred_mode (rr_output);
	    output->pref_width = gnome_rr_mode_get_width (mode);
	    output->pref_height = gnome_rr_mode_get_height (mode);
	}

        output->primary = gnome_rr_output_get_is_primary (rr_output);
        output->underscanning = gnome_rr_output_get_is_underscanning (rr_output);

	g_ptr_array_add (a, output);
    }

    g_ptr_array_add (a, NULL);
    
    config->outputs = (GnomeRROutputInfo **) g_ptr_array_free (a, FALSE);

    /* Walk the outputs computing the right-most edge of all
     * lit-up displays
     */
    last_x = 0;
    for (int i = 0; config->outputs[i] != NULL; ++i) {
	GnomeRROutputInfo *output = config->outputs[i];

	if (output->active) {
	    last_x = MAX (last_x, output->x + output->width);
	}
    }

    /* Now position all off displays to the right of the
     * on displays
     */
    for (int i = 0; config->outputs[i] != NULL; ++i) {
	GnomeRROutputInfo *output = config->outputs[i];

	if (output->connected && !output->active) {
	    output->x = last_x;
	    last_x = output->x + output->width;
	}
    }
    
    g_assert (gnome_rr_config_match (config, config));

    return TRUE;
}

static void
gnome_rr_config_class_init (GnomeRRConfigClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->set_property = gnome_rr_config_set_property;
    gobject_class->finalize = gnome_rr_config_finalize;

    g_object_class_install_property (gobject_class, PROP_SCREEN,
				     g_param_spec_object ("screen",
                                                          "Screen",
                                                          "The GnomeRRScreen this config applies to",
                                                          GNOME_RR_TYPE_SCREEN,
							  G_PARAM_WRITABLE |
                                                          G_PARAM_CONSTRUCT_ONLY |
                                                          G_PARAM_STATIC_NICK |
                                                          G_PARAM_STATIC_BLURB));
}

GnomeRRConfig *
gnome_rr_config_new_current (GnomeRRScreen *screen,
                             GError **error)
{
    g_return_val_if_fail (GNOME_RR_IS_SCREEN (screen), NULL);

    GnomeRRConfig *self = g_object_new (GNOME_RR_TYPE_CONFIG, "screen", screen, NULL);

    if (gnome_rr_config_load_current (self, error))
      return self;
    else
      {
	g_object_unref (self);
	return NULL;
      }
}

static gboolean
output_match (GnomeRROutputInfo *output1,
              GnomeRROutputInfo *output2)
{
    g_assert (GNOME_RR_IS_OUTPUT_INFO (output1));
    g_assert (GNOME_RR_IS_OUTPUT_INFO (output2));

    if (g_strcmp0 (output1->name, output2->name) != 0)
	return FALSE;

    if (g_strcmp0 (output1->vendor, output2->vendor) != 0)
	return FALSE;

    if (g_strcmp0 (output1->product, output2->product) != 0)
	return FALSE;

    if (g_strcmp0 (output1->serial, output2->serial) != 0)
	return FALSE;
    
    return TRUE;
}

static gboolean
output_equal (GnomeRROutputInfo *output1,
              GnomeRROutputInfo *output2)
{
    g_assert (GNOME_RR_IS_OUTPUT_INFO (output1));
    g_assert (GNOME_RR_IS_OUTPUT_INFO (output2));

    if (!output_match (output1, output2))
	return FALSE;

    if (output1->active != output2->active)
	return FALSE;

    if (output1->active) {
	if (output1->width != output2->width)
	    return FALSE;
	
	if (output1->height != output2->height)
	    return FALSE;
	
	if (output1->rate != output2->rate)
	    return FALSE;
	
	if (output1->x != output2->x)
	    return FALSE;
	
	if (output1->y != output2->y)
	    return FALSE;
	
	if (output1->rotation != output2->rotation)
	    return FALSE;

	if (output1->underscanning != output2->underscanning)
	    return FALSE;
    }

    return TRUE;
}

static GnomeRROutputInfo *
find_output (GnomeRRConfig *config,
             const char *name)
{
    for (int i = 0; config->outputs[i] != NULL; ++i) {
	GnomeRROutputInfo *output = config->outputs[i];
	
	if (strcmp (name, output->name) == 0)
	    return output;
    }

    return NULL;
}

/* Match means "these configurations apply to the same hardware
 * setups"
 */
gboolean
gnome_rr_config_match (GnomeRRConfig *c1,
                       GnomeRRConfig *c2)
{
    g_return_val_if_fail (GNOME_RR_IS_CONFIG (c1), FALSE);
    g_return_val_if_fail (GNOME_RR_IS_CONFIG (c2), FALSE);

    for (int i = 0; c1->outputs[i] != NULL; ++i) {
	GnomeRROutputInfo *output1 = c1->outputs[i];
	GnomeRROutputInfo *output2;

	output2 = find_output (c2, output1->name);
	if (!output2 || !output_match (output1, output2))
	    return FALSE;
    }
    
    return TRUE;
}

/* Equal means "the configurations will result in the same
 * modes being set on the outputs"
 */
gboolean
gnome_rr_config_equal (GnomeRRConfig *c1,
		       GnomeRRConfig *c2)
{
    g_return_val_if_fail (GNOME_RR_IS_CONFIG (c1), FALSE);
    g_return_val_if_fail (GNOME_RR_IS_CONFIG (c2), FALSE);

    for (int i = 0; c1->outputs[i] != NULL; ++i) {
	GnomeRROutputInfo *output1 = c1->outputs[i];
	GnomeRROutputInfo *output2;

	output2 = find_output (c2, output1->name);
	if (!output2 || !output_equal (output1, output2))
	    return FALSE;
    }
    
    return TRUE;
}

static GnomeRROutputInfo **
make_outputs (GnomeRRConfig *config)
{
    GPtrArray *outputs;
    GnomeRROutputInfo *first_active = NULL;

    outputs = g_ptr_array_new ();

    for (int i = 0; config->outputs[i] != NULL; ++i) {
	GnomeRROutputInfo *old = config->outputs[i];
	GnomeRROutputInfo *new = g_object_new (GNOME_RR_TYPE_OUTPUT_INFO, NULL);
	*(new) = *(old);

        new->name = g_strdup (old->name);
        new->display_name = g_strdup (old->display_name);
        new->connector_type = g_strdup (old->connector_type);
        new->vendor = g_strdup (old->vendor);
        new->product = g_strdup (old->product);
        new->serial = g_strdup (old->serial);

	if (old->active && !first_active)
	    first_active = old;
	
	if (config->clone && new->active) {
	    g_assert (first_active);

	    new->width = first_active->width;
	    new->height = first_active->height;
	    new->rotation = first_active->rotation;
	    new->x = 0;
	    new->y = 0;
	}

	g_ptr_array_add (outputs, new);
    }

    g_ptr_array_add (outputs, NULL);

    return (GnomeRROutputInfo **) g_ptr_array_free (outputs, FALSE);
}

gboolean
gnome_rr_config_applicable (GnomeRRConfig  *configuration,
			    GnomeRRScreen  *screen,
			    GError        **error)
{
    GnomeRROutputInfo **outputs;
    CrtcAssignment *assign;
    gboolean result;

    g_return_val_if_fail (GNOME_RR_IS_CONFIG (configuration), FALSE);
    g_return_val_if_fail (GNOME_RR_IS_SCREEN (screen), FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    outputs = make_outputs (configuration);
    assign = crtc_assignment_new (screen, outputs, error);

    if (assign) {
	result = TRUE;
	crtc_assignment_free (assign);
    } else {
	result = FALSE;
    }

    for (int i = 0; outputs[i] != NULL; i++) {
	 g_object_unref (outputs[i]);
    }

    g_free (outputs);

    return result;
}

/* Database management */

void
gnome_rr_config_sanitize (GnomeRRConfig *config)
{
    int x_offset, y_offset;

    /* Offset everything by the top/left-most coordinate to
     * make sure the configuration starts at (0, 0)
     */
    x_offset = y_offset = G_MAXINT;
    for (int i = 0; config->outputs[i]; ++i)
    {
	GnomeRROutputInfo *output = config->outputs[i];

	if (output->active) {
	    x_offset = MIN (x_offset, output->x);
	    y_offset = MIN (y_offset, output->y);
	}
    }

    for (int i = 0; config->outputs[i]; ++i) {
	GnomeRROutputInfo *output = config->outputs[i];
	
	if (output->active) {
	    output->x -= x_offset;
	    output->y -= y_offset;
	}
    }

    /* Only one primary, please */
    gboolean found = FALSE;
    for (int i = 0; config->outputs[i]; ++i) {
        if (config->outputs[i]->primary) {
            if (found) {
                config->outputs[i]->primary = FALSE;
            } else {
                found = TRUE;
            }
        }
    }
}

gboolean
gnome_rr_config_ensure_primary (GnomeRRConfig *self)
{
    int i;
    GnomeRROutputInfo *builtin_display;
    GnomeRROutputInfo *top_left;
    gboolean found;

    g_return_val_if_fail (GNOME_RR_IS_CONFIG (self), FALSE);

    builtin_display = NULL;
    top_left = NULL;
    found = FALSE;

    for (i = 0; self->outputs[i] != NULL; ++i) {
            GnomeRROutputInfo *info = self->outputs[i];

            if (!info->active) {
                   info->primary = FALSE;
                   continue;
            }

            /* ensure only one */
            if (info->primary) {
                    if (found) {
                            info->primary = FALSE;
                    } else {
                            found = TRUE;
                    }
            }

            if (top_left == NULL
                || (info->x < top_left->x
                    && info->y < top_left->y)) {
                    top_left = info;
            }
            if (builtin_display == NULL
                && gnome_rr_output_connector_type_is_builtin_display (info->connector_type)) {
                    builtin_display = info;
            }
    }

    if (!found) {
            if (builtin_display != NULL) {
                    builtin_display->primary = TRUE;
            } else if (top_left != NULL) {
                    /* Note: top_left can be NULL if all outputs are off */
                    top_left->primary = TRUE;
            }
    }

    return !found;
}

static gboolean
gnome_rr_config_apply_helper (GnomeRRConfig *config,
			      GnomeRRScreen *screen,
			      gboolean       persistent,
			      GError       **error)
{
    CrtcAssignment *assignment;
    GnomeRROutputInfo **outputs;
    gboolean result = FALSE;
    int i;

    g_return_val_if_fail (GNOME_RR_IS_CONFIG (config), FALSE);
    g_return_val_if_fail (GNOME_RR_IS_SCREEN (screen), FALSE);

    outputs = make_outputs (config);

    assignment = crtc_assignment_new (screen, outputs, error);

    if (assignment)
    {
	if (crtc_assignment_apply (assignment, persistent, error))
	    result = TRUE;

	crtc_assignment_free (assignment);
    }

    for (i = 0; outputs[i] != NULL; i++)
	g_object_unref (outputs[i]);
    g_free (outputs);

    return result;
}

gboolean
gnome_rr_config_apply (GnomeRRConfig *config,
		       GnomeRRScreen *screen,
		       GError       **error)
{
    return gnome_rr_config_apply_helper (config, screen, FALSE, error);
}

gboolean
gnome_rr_config_apply_persistent (GnomeRRConfig *config,
				  GnomeRRScreen *screen,
				  GError       **error)
{
    return gnome_rr_config_apply_helper (config, screen, TRUE, error);
}

/**
 * gnome_rr_config_get_outputs:
 *
 * Returns: (array zero-terminated=1) (element-type GnomeRROutputInfo) (transfer none): the output configuration for this #GnomeRRConfig
 */
GnomeRROutputInfo **
gnome_rr_config_get_outputs (GnomeRRConfig *self)
{
    g_return_val_if_fail (GNOME_RR_IS_CONFIG (self), NULL);

    return self->outputs;
}

/**
 * gnome_rr_config_get_clone:
 *
 * Returns: whether at least two outputs are at (0, 0) offset and they
 * have the same width/height.  Those outputs are of course connected and on
 * (i.e. they have a CRTC assigned).
 */
gboolean
gnome_rr_config_get_clone (GnomeRRConfig *self)
{
    g_return_val_if_fail (GNOME_RR_IS_CONFIG (self), FALSE);

    return self->clone;
}

void
gnome_rr_config_set_clone (GnomeRRConfig *self, gboolean clone)
{
    g_return_if_fail (GNOME_RR_IS_CONFIG (self));

    self->clone = clone;
}

/*
 * CRTC assignment
 */
typedef struct CrtcInfo CrtcInfo;

struct CrtcInfo
{
    GnomeRRMode    *mode;
    int        x;
    int        y;
    GnomeRRRotation rotation;
    GPtrArray *outputs;
};

struct CrtcAssignment
{
    GnomeRROutputInfo **outputs;
    GnomeRRScreen *screen;
    GHashTable *info;
    GnomeRROutput *primary;
};

static gboolean
can_clone (CrtcInfo *info,
	   GnomeRROutput *output)
{
    guint i;

    for (i = 0; i < info->outputs->len; ++i)
    {
	GnomeRROutput *clone = info->outputs->pdata[i];

	if (!gnome_rr_output_can_clone (clone, output))
	    return FALSE;
    }

    return TRUE;
}

static gboolean
crtc_assignment_assign (CrtcAssignment   *assign,
			GnomeRRCrtc      *crtc,
			GnomeRRMode      *mode,
			int               x,
			int               y,
			GnomeRRRotation   rotation,
                        gboolean          primary,
			GnomeRROutput    *output,
			GError          **error)
{
    CrtcInfo *info = g_hash_table_lookup (assign->info, crtc);
    guint32 crtc_id;
    const char *output_name;

    crtc_id = gnome_rr_crtc_get_id (crtc);
    output_name = gnome_rr_output_get_name (output);

    if (!gnome_rr_crtc_can_drive_output (crtc, output))
    {
	g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_CRTC_ASSIGNMENT,
		     _("CRTC %d cannot drive output %s"), crtc_id, output_name);
	return FALSE;
    }

    if (!gnome_rr_output_supports_mode (output, mode))
    {
	g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_CRTC_ASSIGNMENT,
		     _("output %s does not support mode %dx%d@%dHz"),
		     output_name,
		     gnome_rr_mode_get_width (mode),
		     gnome_rr_mode_get_height (mode),
		     gnome_rr_mode_get_freq (mode));
	return FALSE;
    }

    if (!gnome_rr_crtc_supports_rotation (crtc, rotation))
    {
	g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_CRTC_ASSIGNMENT,
		     _("CRTC %d does not support rotation=%d"),
		     crtc_id, rotation);
	return FALSE;
    }

    if (info)
    {
	if (!(info->mode == mode	&&
	      info->x == x		&&
	      info->y == y		&&
	      info->rotation == rotation))
	{
	    g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_CRTC_ASSIGNMENT,
			 _("output %s does not have the same parameters as another cloned output:\n"
			   "existing mode = %d, new mode = %d\n"
			   "existing coordinates = (%d, %d), new coordinates = (%d, %d)\n"
			   "existing rotation = %d, new rotation = %d"),
			 output_name,
			 gnome_rr_mode_get_id (info->mode), gnome_rr_mode_get_id (mode),
			 info->x, info->y,
			 x, y,
			 info->rotation, rotation);
	    return FALSE;
	}

	if (!can_clone (info, output))
	{
	    g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_CRTC_ASSIGNMENT,
			 _("cannot clone to output %s"),
			 output_name);
	    return FALSE;
	}

	g_ptr_array_add (info->outputs, output);

	if (primary && !assign->primary)
	{
	    assign->primary = output;
	}

	return TRUE;
    }
    else
    {	
	info = g_new0 (CrtcInfo, 1);
	
	info->mode = mode;
	info->x = x;
	info->y = y;
	info->rotation = rotation;
	info->outputs = g_ptr_array_new ();
	
	g_ptr_array_add (info->outputs, output);
	
	g_hash_table_insert (assign->info, crtc, info);
	    
        if (primary && !assign->primary)
        {
            assign->primary = output;
        }

	return TRUE;
    }
}

static void
crtc_assignment_unassign (CrtcAssignment *assign,
			  GnomeRRCrtc         *crtc,
			  GnomeRROutput       *output)
{
    CrtcInfo *info = g_hash_table_lookup (assign->info, crtc);

    if (info)
    {
	g_ptr_array_remove (info->outputs, output);

        if (assign->primary == output)
        {
            assign->primary = NULL;
        }

	if (info->outputs->len == 0)
	    g_hash_table_remove (assign->info, crtc);
    }
}

static void
crtc_assignment_free (CrtcAssignment *assign)
{
    g_hash_table_destroy (assign->info);

    g_free (assign);
}

static gboolean
mode_is_rotated (CrtcInfo *info)
{
    if ((info->rotation & GNOME_RR_ROTATION_270)		||
	(info->rotation & GNOME_RR_ROTATION_90))
    {
	return TRUE;
    }
    return FALSE;
}

static void
accumulate_error (GString *accumulated_error, GError *error)
{
    g_string_append_printf (accumulated_error, "    %s\n", error->message);
    g_error_free (error);
}

/* Check whether the given set of settings can be used
 * at the same time -- ie. whether there is an assignment
 * of CRTC's to outputs.
 *
 * Brute force - the number of objects involved is small
 * enough that it doesn't matter.
 */
static gboolean
real_assign_crtcs (GnomeRRScreen *screen,
		   GnomeRROutputInfo **outputs,
		   CrtcAssignment *assignment,
		   GError **error)
{
    GnomeRRCrtc **crtcs = gnome_rr_screen_list_crtcs (screen);
    GnomeRROutputInfo *output;
    int i;
    gboolean tried_mode;
    GError *my_error;
    GString *accumulated_error;
    gboolean success;

    output = *outputs;
    if (!output)
	return TRUE;

    /* It is always allowed for an output to be turned off */
    if (!output->active) {
	return real_assign_crtcs (screen, outputs + 1, assignment, error);
    }

    success = FALSE;
    tried_mode = FALSE;
    accumulated_error = g_string_new (NULL);

    for (i = 0; crtcs[i] != NULL; ++i)
    {
	GnomeRRCrtc *crtc = crtcs[i];
	int crtc_id = gnome_rr_crtc_get_id (crtc);
	int pass;

	g_string_append_printf (accumulated_error,
				_("Trying modes for CRTC %d\n"),
				crtc_id);

	/* Make two passes, one where frequencies must match, then
	 * one where they don't have to
	 */
	for (pass = 0; pass < 2; ++pass)
	{
	    GnomeRROutput *gnome_rr_output = gnome_rr_screen_get_output_by_name (screen, output->name);
	    GnomeRRMode **modes = gnome_rr_output_list_modes (gnome_rr_output);
	    int j;

	    for (j = 0; modes[j] != NULL; ++j)
	    {
		GnomeRRMode *mode = modes[j];
		int mode_width;
		int mode_height;
		int mode_freq;

		mode_width = gnome_rr_mode_get_width (mode);
		mode_height = gnome_rr_mode_get_height (mode);
		mode_freq = gnome_rr_mode_get_freq (mode);

		g_string_append_printf (accumulated_error,
					_("CRTC %d: trying mode %dx%d@%dHz with output at %dx%d@%dHz (pass %d)\n"),
					crtc_id,
					mode_width, mode_height, mode_freq,
					output->width, output->height, output->rate,
					pass);

		if (mode_width == output->width	&&
		    mode_height == output->height &&
		    (pass == 1 || mode_freq == output->rate))
		{
		    tried_mode = TRUE;

		    my_error = NULL;
		    if (crtc_assignment_assign (
			    assignment, crtc, modes[j],
			    output->x, output->y,
			    output->rotation,
                            output->primary,
			    gnome_rr_output,
			    &my_error))
		    {
			my_error = NULL;
			if (real_assign_crtcs (screen, outputs + 1, assignment, &my_error)) {
			    success = TRUE;
			    goto out;
			} else
			    accumulate_error (accumulated_error, my_error);

			crtc_assignment_unassign (assignment, crtc, gnome_rr_output);
		    } else
			accumulate_error (accumulated_error, my_error);
		}
	    }
	}
    }

out:

    if (success)
	g_string_free (accumulated_error, TRUE);
    else {
	char *str;

	str = g_string_free (accumulated_error, FALSE);

	if (tried_mode)
	    g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_CRTC_ASSIGNMENT,
			 _("could not assign CRTCs to outputs:\n%s"),
			 str);
	else
	    g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_CRTC_ASSIGNMENT,
			 _("none of the selected modes were compatible with the possible modes:\n%s"),
			 str);

	g_free (str);
    }

    return success;
}

static void
crtc_info_free (CrtcInfo *info)
{
    g_ptr_array_free (info->outputs, TRUE);
    g_free (info);
}

static void
get_required_virtual_size (CrtcAssignment *assign, int *width, int *height)
{
    GList *active_crtcs = g_hash_table_get_keys (assign->info);
    GList *list;
    int d;

    if (!width)
	width = &d;
    if (!height)
	height = &d;
    
    /* Compute size of the screen */
    *width = *height = 1;
    for (list = active_crtcs; list != NULL; list = list->next)
    {
	GnomeRRCrtc *crtc = list->data;
	CrtcInfo *info = g_hash_table_lookup (assign->info, crtc);
	int w, h;

	w = gnome_rr_mode_get_width (info->mode);
	h = gnome_rr_mode_get_height (info->mode);
	
	if (mode_is_rotated (info))
	{
	    int tmp = h;
	    h = w;
	    w = tmp;
	}
	
	*width = MAX (*width, info->x + w);
	*height = MAX (*height, info->y + h);
    }

    g_list_free (active_crtcs);
}

static CrtcAssignment *
crtc_assignment_new (GnomeRRScreen      *screen,
		     GnomeRROutputInfo **outputs,
		     GError            **error)
{
    CrtcAssignment *assignment = g_new0 (CrtcAssignment, 1);

    assignment->outputs = outputs;
    assignment->info = g_hash_table_new_full (
	g_direct_hash, g_direct_equal, NULL, (GFreeFunc)crtc_info_free);

    if (real_assign_crtcs (screen, outputs, assignment, error))
    {
	int width, height;
	int min_width, max_width, min_height, max_height;

	get_required_virtual_size (assignment, &width, &height);

	gnome_rr_screen_get_ranges (
	    screen, &min_width, &max_width, &min_height, &max_height);
    
	if (width < min_width || width > max_width ||
	    height < min_height || height > max_height)
	{
	    g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_BOUNDS_ERROR,
			 /* Translators: the "requested", "minimum", and
			  * "maximum" words here are not keywords; please
			  * translate them as usual. */
			 _("required virtual size does not fit available size: "
			   "requested=(%d, %d), minimum=(%d, %d), maximum=(%d, %d)"),
			 width, height,
			 min_width, min_height,
			 max_width, max_height);
	    goto fail;
	}

	assignment->screen = screen;
	
	return assignment;
    }

fail:
    crtc_assignment_free (assignment);
    
    return NULL;
}

#define ROTATION_MASK 0x7F

static enum wl_output_transform
rotation_to_transform (GnomeRRRotation rotation)
{
    static const enum wl_output_transform y_reflected_map[] = {
        WL_OUTPUT_TRANSFORM_FLIPPED_180,
        WL_OUTPUT_TRANSFORM_FLIPPED_90,
        WL_OUTPUT_TRANSFORM_FLIPPED,
        WL_OUTPUT_TRANSFORM_FLIPPED_270
    };

    enum wl_output_transform ret;

    switch (rotation & ROTATION_MASK) {
        default:
        case GNOME_RR_ROTATION_0:
            ret = WL_OUTPUT_TRANSFORM_NORMAL;
            break;
        case GNOME_RR_ROTATION_90:
            ret = WL_OUTPUT_TRANSFORM_90;
            break;
        case GNOME_RR_ROTATION_180:
            ret = WL_OUTPUT_TRANSFORM_180;
            break;
        case GNOME_RR_ROTATION_270:
            ret = WL_OUTPUT_TRANSFORM_270;
            break;
    }

    if (rotation & GNOME_RR_REFLECT_X) {
        return ret + 4;
    } else if (rotation & GNOME_RR_REFLECT_Y) {
        return y_reflected_map[ret];
    } else {
        return ret;
    }
}

static gboolean
crtc_assignment_apply (CrtcAssignment *assign,
                       gboolean persistent,
                       GError **error)
{
    GVariantBuilder crtc_builder, output_builder, nested_outputs;
    GHashTableIter iter;
    GnomeRRCrtc *crtc;
    CrtcInfo *info;

    g_variant_builder_init (&crtc_builder, G_VARIANT_TYPE ("a(uiiiuaua{sv})"));
    g_variant_builder_init (&output_builder, G_VARIANT_TYPE ("a(ua{sv})"));

    g_hash_table_iter_init (&iter, assign->info);
    while (g_hash_table_iter_next (&iter, (gpointer) &crtc, (gpointer) &info)) {
	g_variant_builder_init (&nested_outputs, G_VARIANT_TYPE ("au"));

	for (unsigned int i = 0; i < info->outputs->len; i++) {
	    GnomeRROutput *output = g_ptr_array_index (info->outputs, i);

	    g_variant_builder_add (&nested_outputs, "u",
				   gnome_rr_output_get_id (output));
	}

	g_variant_builder_add (&crtc_builder, "(uiiiuaua{sv})",
			       gnome_rr_crtc_get_id (crtc),
			       info->mode ?
			       gnome_rr_mode_get_id (info->mode) : (guint32) -1,
			       info->x,
			       info->y,
			       rotation_to_transform (info->rotation),
			       &nested_outputs,
			       NULL);
    }

    for (unsigned int i = 0; assign->outputs[i]; i++) {
	GnomeRROutputInfo *output = assign->outputs[i];
	GnomeRROutput *gnome_rr_output = gnome_rr_screen_get_output_by_name (assign->screen,
									     output->name);

	g_variant_builder_add (&output_builder, "(u@a{sv})",
			       gnome_rr_output_get_id (gnome_rr_output),
			       g_variant_new_parsed ("{ 'primary': <%b>,"
						     "  'presentation': <%b>,"
						     "  'underscanning': <%b> }",
						     output->primary,
						     FALSE,
						     output->underscanning));
    }

    return gnome_rr_screen_apply_configuration (assign->screen,
						persistent,
						g_variant_builder_end (&crtc_builder),
						g_variant_builder_end (&output_builder),
						error);
}
