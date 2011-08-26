/* gnome-tz-monitor.h - fade window background between two surfaces

   Copyright 2008, Red Hat, Inc.

   This file is part of the Gnome Library.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Ray Strode <rstrode@redhat.com>
*/

#ifndef __GNOME_TZ_MONITOR_H__
#define __GNOME_TZ_MONITOR_H__

#ifndef GNOME_DESKTOP_USE_UNSTABLE_API
#error    GnomeTzMonitor is unstable API. You must define GNOME_DESKTOP_USE_UNSTABLE_API before including gnome-tz-monitor.h
#endif

#include <gdk/gdk.h>

G_BEGIN_DECLS

#define GNOME_TYPE_TZ_MONITOR            (gnome_tz_monitor_get_type ())
#define GNOME_TZ_MONITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_TZ_MONITOR, GnomeTzMonitor))
#define GNOME_TZ_MONITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GNOME_TYPE_TZ_MONITOR, GnomeTzMonitorClass))
#define GNOME_IS_TZ_MONITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_TYPE_TZ_MONITOR))
#define GNOME_IS_TZ_MONITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GNOME_TYPE_TZ_MONITOR))
#define GNOME_TZ_MONITOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GNOME_TYPE_TZ_MONITOR, GnomeTzMonitorClass))

typedef struct _GnomeTzMonitorPrivate GnomeTzMonitorPrivate;
typedef struct _GnomeTzMonitor GnomeTzMonitor;
typedef struct _GnomeTzMonitorClass GnomeTzMonitorClass;

struct _GnomeTzMonitor
{
	GObject parent_object;

	GnomeTzMonitorPrivate *priv;
};

struct _GnomeTzMonitorClass
{
	GObjectClass parent_class;

	void (* changed) (GnomeTzMonitor *monitor);
};

GType             gnome_tz_monitor_get_type      (void);
GnomeTzMonitor *gnome_tz_monitor_new             (void);

G_END_DECLS

#endif
