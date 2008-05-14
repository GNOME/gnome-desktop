/* monitor-db.h
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
#ifndef MONITOR_DB_H
#define MONITOR_DB_H

#ifndef GNOME_DESKTOP_USE_UNSTABLE_API
#error    monitor-db.h is unstable API. You must define GNOME_DESKTOP_USE_UNSTABLE_API before including monitor-db.h
#endif

#include <libgnomeui/randrwrap.h>
#include <glib.h>

typedef struct Output Output;
typedef struct Configuration Configuration;

struct Output
{
    char *	name;

    gboolean	on;
    int		width;
    int		height;
    int		rate;
    int		x;
    int		y;
    RWRotation	rotation;

    gboolean	connected;
    char	vendor[4];
    guint	product;
    guint	serial;
    double	aspect;
    int		pref_width;
    int		pref_height;
    char *      display_name;

    gpointer	user_data;
};

struct Configuration
{
    gboolean clone;
    
    Output **outputs;
};

void            configuration_free         (Configuration  *configuration);
Configuration  *configuration_new_current  (RWScreen       *screen);
gboolean        configuration_match        (Configuration  *config1,
					    Configuration  *config2);
gboolean        configuration_save         (Configuration  *configuration,
					    GError        **err);
void		configuration_sanitize     (Configuration  *configuration);
gboolean	configuration_apply_stored (RWScreen       *screen);
gboolean	configuration_applicable   (Configuration  *configuration,
					    RWScreen       *screen);

#endif
