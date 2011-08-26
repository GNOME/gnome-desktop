/* gnome-tz-monitor.h - monitors TZ setting files and signals changes
 *
 * Copyright (C) 2010 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Author: Bastien Nocera <hadess@hadess.net>
 * Based on system-timezone.c from gnome-panel
*/

#include <gio/gio.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include "gnome-tz-monitor.h"

struct _GnomeTzMonitorPrivate {
	GList *monitors; /* list of GFileMonitor */
};

const char *tz_files[] = {
	"/etc/timezone",
	"/etc/TIMEZONE",
	"/etc/rc.conf",
	"/etc/sysconfig/clock",
	"/etc/conf.d/clock",
	"/etc/localtime"
};

enum {
	CHANGED,
	NUMBER_OF_SIGNALS
};

static guint signals[NUMBER_OF_SIGNALS] = { 0 };

G_DEFINE_TYPE (GnomeTzMonitor, gnome_tz_monitor, G_TYPE_OBJECT)
#define GNOME_TZ_MONITOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o),\
			                   GNOME_TYPE_TZ_MONITOR,\
			                   GnomeTzMonitorPrivate))
static void
gnome_tz_monitor_finalize (GObject *object)
{
	GnomeTzMonitor *monitor;
	GList *l;

	monitor = GNOME_TZ_MONITOR (object);

	for (l = monitor->priv->monitors; l != NULL; l = l->next) {
		g_object_unref (l->data);
	}
	g_list_free (monitor->priv->monitors);
	monitor->priv->monitors = NULL;

	G_OBJECT_CLASS (gnome_tz_monitor_parent_class)->finalize (object);
}

static void
gnome_tz_monitor_class_init (GnomeTzMonitorClass *fade_class)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS (fade_class);

	gobject_class->finalize = gnome_tz_monitor_finalize;

	/**
	 * GnomeTzMonitor::changed:
	 * @monitor: the #GnomeTzMonitor that received the signal
	 *
	 * When a timezone file changes, this signal will be emitted.
	 * It is up to the receiver to check the timezone actually changed.
	 */
	signals[CHANGED] = g_signal_new ("changed",
					  G_OBJECT_CLASS_TYPE (gobject_class),
					  G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					  g_cclosure_marshal_VOID__OBJECT,
					  G_TYPE_NONE, 1, G_TYPE_OBJECT);

	g_type_class_add_private (gobject_class, sizeof (GnomeTzMonitorPrivate));
}

static void
gnome_tz_monitor_monitor_changed (GFileMonitor *handle,
				  GFile *file,
				  GFile *other_file,
				  GFileMonitorEvent event,
				  GnomeTzMonitor *monitor)
{
	if (event != G_FILE_MONITOR_EVENT_CHANGED &&
	    event != G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT &&
	    event != G_FILE_MONITOR_EVENT_DELETED &&
	    event != G_FILE_MONITOR_EVENT_CREATED)
		return;

	g_signal_emit (G_OBJECT (monitor), signals[CHANGED], 0, NULL);
}

static void
gnome_tz_monitor_init (GnomeTzMonitor *monitor)
{
	guint i;

	monitor->priv = GNOME_TZ_MONITOR_GET_PRIVATE (monitor);

	for (i = 0; i < G_N_ELEMENTS (tz_files); i++) {
		GFile        *file;
		GFile        *parent;
		GFileType     parent_type;
		GFileMonitor *m;

		file = g_file_new_for_path (tz_files[i]);

		parent = g_file_get_parent (file);
		parent_type = g_file_query_file_type (parent, G_FILE_QUERY_INFO_NONE, NULL);
		g_object_unref (parent);

		/* We don't try to monitor the file if the parent directory
		 * doesn't exist: this means we're on a system where this file
		 * is not useful to determine the system timezone.
		 * Since gio does not monitor file in non-existing directories
		 * in a clever way (as of gio 2.22, it just polls every other
		 * seconds to see if the directory now exists), this avoids
		 * unnecessary wakeups. */
		m = NULL;
		if (parent_type == G_FILE_TYPE_DIRECTORY) {
			m = g_file_monitor_file (file,
						 G_FILE_MONITOR_NONE,
						 NULL, NULL);
		}
		g_object_unref (file);

		if (m) {
			monitor->priv->monitors = g_list_prepend (monitor->priv->monitors, m);
			g_signal_connect (G_OBJECT (m),
					  "changed",
					  G_CALLBACK (gnome_tz_monitor_monitor_changed),
					  monitor);
		}
	}

	monitor->priv->monitors = g_list_reverse (monitor->priv->monitors);
}

/**
 * gnome_tz_monitor_new:
 *
 * Creates a new object to monitor timezone files,
 * and emit the #changed signal when one of them changes.
 *
 * Return value: the new #GnomeTzMonitor
 **/
GnomeTzMonitor *
gnome_tz_monitor_new (void)
{
	return g_object_new (GNOME_TYPE_TZ_MONITOR, NULL);
}


