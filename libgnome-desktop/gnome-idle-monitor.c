/* -*- mode: C; c-file-style: "linux"; indent-tabs-mode: t -*-
 *
 * Adapted from gnome-session/gnome-session/gs-idle-monitor.c
 *
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <time.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/extensions/sync.h>

#include <glib.h>
#include <gdk/gdkx.h>
#include <gdk/gdk.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include "gnome-idle-monitor.h"

#define GNOME_IDLE_MONITOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GNOME_TYPE_IDLE_MONITOR, GnomeIdleMonitorPrivate))

struct GnomeIdleMonitorPrivate
{
	Display	    *display;

	GHashTable  *watches;
	int	     sync_event_base;
	XSyncCounter counter;

	XSyncAlarm   xalarm_reset;
};

typedef struct
{
	Display			 *display;
	guint			  id;
	GnomeIdleMonitorWatchFunc callback;
	gpointer		  user_data;
	GDestroyNotify		  notify;
	XSyncAlarm		  xalarm;
} GnomeIdleMonitorWatch;

enum
{
	BECAME_ACTIVE,
	TRIGGERED_IDLE,

	LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GnomeIdleMonitor, gnome_idle_monitor, G_TYPE_OBJECT)

static gint64
_xsyncvalue_to_int64 (XSyncValue value)
{
	return ((guint64) XSyncValueHigh32 (value)) << 32
		| (guint64) XSyncValueLow32 (value);
}

static XSyncValue
_int64_to_xsyncvalue (gint64 value)
{
	XSyncValue ret;

	XSyncIntsToValue (&ret, value, ((guint64)value) >> 32);

	return ret;
}

static gboolean
_find_alarm (gpointer		    key,
	     GnomeIdleMonitorWatch *watch,
	     XSyncAlarm		   *alarm)
{
	return watch->xalarm == *alarm;
}

static XSyncAlarm
_xsync_alarm_set (GnomeIdleMonitor	*monitor,
		  XSyncTestType          test_type,
		  guint64                interval)
{
	XSyncAlarmAttributes attr;
	XSyncValue	     delta;
	guint		     flags;

	flags = XSyncCACounter | XSyncCAValueType | XSyncCATestType |
		XSyncCAValue | XSyncCADelta | XSyncCAEvents;

	XSyncIntToValue (&delta, 0);
	attr.trigger.counter = monitor->priv->counter;
	attr.trigger.value_type = XSyncAbsolute;
	attr.delta = delta;
	attr.events = TRUE;

	attr.trigger.wait_value = _int64_to_xsyncvalue (interval);
	attr.trigger.test_type = test_type;
	return XSyncCreateAlarm (monitor->priv->display, flags, &attr);
}

static GnomeIdleMonitorWatch *
find_watch_for_alarm (GnomeIdleMonitor *monitor,
		      XSyncAlarm	alarm)
{
	GnomeIdleMonitorWatch *watch;

	watch = g_hash_table_find (monitor->priv->watches,
				   (GHRFunc)_find_alarm,
				   &alarm);
	return watch;
}

static void
handle_alarm_notify_event (GnomeIdleMonitor	    *monitor,
			   XSyncAlarmNotifyEvent    *alarm_event)
{
	if (alarm_event->state != XSyncAlarmActive) {
		return;
	}

	if (alarm_event->alarm == monitor->priv->xalarm_reset) {
		g_signal_emit (monitor, signals[BECAME_ACTIVE], 0);
	} else {
		GnomeIdleMonitorWatch *watch = find_watch_for_alarm (monitor, alarm_event->alarm);

		if (watch == NULL) {
			return;
		}

		g_signal_emit (monitor, signals[TRIGGERED_IDLE], 0, watch->id);

		if (watch->callback != NULL) {
			watch->callback (monitor,
					 watch->id,
					 watch->user_data);
		}
	}
}

static GdkFilterReturn
xevent_filter (GdkXEvent	*xevent,
	       GdkEvent		*event,
	       GnomeIdleMonitor *monitor)
{
	XEvent		      *ev;
	XSyncAlarmNotifyEvent *alarm_event;

	ev = xevent;
	if (ev->xany.type != monitor->priv->sync_event_base + XSyncAlarmNotify) {
		return GDK_FILTER_CONTINUE;
	}

	alarm_event = xevent;
	handle_alarm_notify_event (monitor, alarm_event);

	return GDK_FILTER_CONTINUE;
}

static XSyncCounter
find_idletime_counter (Display *display)
{
	int		    i;
	int		    ncounters;
	XSyncSystemCounter *counters;
	XSyncCounter        counter = None;

	counters = XSyncListSystemCounters (display, &ncounters);
	for (i = 0; i < ncounters; i++) {
		if (counters[i].name != NULL && strcmp (counters[i].name, "IDLETIME") == 0) {
			counter = counters[i].counter;
			break;
		}
	}
	XSyncFreeSystemCounterList (counters);

	return counter;
}

static guint32
get_next_watch_serial (void)
{
	static guint32 serial = 1;
	g_atomic_int_inc (&serial);
	return serial;
}

static GnomeIdleMonitorWatch *
idle_monitor_watch_new (void)
{
	GnomeIdleMonitorWatch *watch;

	watch = g_slice_new0 (GnomeIdleMonitorWatch);
	watch->id = get_next_watch_serial ();
	watch->xalarm = None;

	return watch;
}

static void
idle_monitor_watch_free (GnomeIdleMonitorWatch *watch)
{
	if (watch == NULL) {
		return;
	}

	if (watch->notify != NULL) {
	    watch->notify (watch->user_data);
	}

	if (watch->xalarm != None) {
		XSyncDestroyAlarm (watch->display, watch->xalarm);
	}
	g_slice_free (GnomeIdleMonitorWatch, watch);
}

static void
init_xsync (GnomeIdleMonitor *monitor)
{
	int		    sync_error_base;
	int		    res;
	int		    major;
	int		    minor;

	res = XSyncQueryExtension (monitor->priv->display,
				   &monitor->priv->sync_event_base,
				   &sync_error_base);
	if (! res) {
		g_warning ("GnomeIdleMonitor: Sync extension not present");
		return;
	}

	res = XSyncInitialize (monitor->priv->display, &major, &minor);
	if (! res) {
		g_warning ("GnomeIdleMonitor: Unable to initialize Sync extension");
		return;
	}

	monitor->priv->counter = find_idletime_counter (monitor->priv->display);
	if (monitor->priv->counter == None) {
		g_warning ("GnomeIdleMonitor: IDLETIME counter not found");
		return;
	}

	monitor->priv->xalarm_reset = _xsync_alarm_set (monitor, XSyncNegativeTransition, 1);

	gdk_window_add_filter (NULL, (GdkFilterFunc)xevent_filter, monitor);
}

static void
gnome_idle_monitor_dispose (GObject *object)
{
	GnomeIdleMonitor *monitor;

	monitor = GNOME_IDLE_MONITOR (object);
	if (monitor->priv->watches != NULL) {
		g_hash_table_destroy (monitor->priv->watches);
		monitor->priv->watches = NULL;
	}

	G_OBJECT_CLASS (gnome_idle_monitor_parent_class)->dispose (object);
}

static GObject *
gnome_idle_monitor_constructor (GType                  type,
                                guint                  n_construct_properties,
                                GObjectConstructParam *construct_properties)
{
	static GnomeIdleMonitor *global_monitor = NULL;

	if (g_once_init_enter (&global_monitor)) {
		GnomeIdleMonitor *monitor;

		monitor = GNOME_IDLE_MONITOR (G_OBJECT_CLASS (gnome_idle_monitor_parent_class)->constructor (type,
													     n_construct_properties,
													     construct_properties));

		g_once_init_leave (&global_monitor, monitor);
	}

	return g_object_ref (global_monitor);
}

static void
gnome_idle_monitor_class_init (GnomeIdleMonitorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gnome_idle_monitor_dispose;
	object_class->constructor = gnome_idle_monitor_constructor;

        signals[BECAME_ACTIVE] =
                g_signal_new ("became-active",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              0,
			      NULL, NULL, NULL,
                              G_TYPE_NONE, 0);
        signals[TRIGGERED_IDLE] =
                g_signal_new ("triggered-idle",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              0,
			      NULL, NULL, NULL,
                              G_TYPE_NONE, 1, G_TYPE_UINT);

	g_type_class_add_private (klass, sizeof (GnomeIdleMonitorPrivate));
}

static void
gnome_idle_monitor_init (GnomeIdleMonitor *monitor)
{
	monitor->priv = GNOME_IDLE_MONITOR_GET_PRIVATE (monitor);

	monitor->priv->watches = g_hash_table_new_full (NULL,
							NULL,
							NULL,
							(GDestroyNotify)idle_monitor_watch_free);
	monitor->priv->counter = None;

	monitor->priv->display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
	init_xsync (monitor);
}

GnomeIdleMonitor *
gnome_idle_monitor_new (void)
{
	return GNOME_IDLE_MONITOR (g_object_new (GNOME_TYPE_IDLE_MONITOR, NULL));
}

/**
 * gnome_idle_monitor_add_watch:
 * @monitor: A #GnomeIdleMonitor
 * @interval: The idletime interval, in milliseconds
 * @callback: (allow-none): The callback to call when the user has
 *     accumulated @interval seconds of idle time.
 * @user_data: (allow-none): The user data to pass to the callback
 * @notify: A #GDestroyNotify
 * @watch_id: (out): The watch ID
 *
 * Adds a watch for a specific idle time. The callback will be called
 * when the user has accumlated @interval seconds of idle time.
 * This function will return an ID in @watch_id that can either be passed
 * to gnome_idle_monitor_remove_watch(), or can be used to tell idle time
 * watches apart if you have more than one.
 *
 * If you need to check for more than one interval of
 * idle time, it may be more convenient to connect to the
 * #GnomeIdleMonitor:::triggered-idle signal, and switch on the
 * watch ID that is passed to the signal.
 *
 * Also note that this function will only care about positive transitions
 * (user's idle time exceeding a certain time). If you want to know about
 * negative transitions, connect to the #GnomeIdleMonitor:::became-active
 * signal.
 */
void
gnome_idle_monitor_add_watch (GnomeIdleMonitor	       *monitor,
			      guint			interval,
			      GnomeIdleMonitorWatchFunc callback,
			      gpointer			user_data,
			      GDestroyNotify		notify,
			      guint                    *watch_id)
{
	GnomeIdleMonitorWatch *watch;

	g_return_if_fail (GNOME_IS_IDLE_MONITOR (monitor));

	watch = idle_monitor_watch_new ();
	watch->display = monitor->priv->display;
	watch->callback = callback;
	watch->user_data = user_data;
	watch->notify = notify;
	watch->xalarm = _xsync_alarm_set (monitor, XSyncPositiveTransition, interval);

	g_hash_table_insert (monitor->priv->watches,
			     GUINT_TO_POINTER (watch->id),
			     watch);
	*watch_id = watch->id;
}

/**
 * gnome_idle_monitor_remove_watch:
 * @monitor: A #GnomeIdleMonitor
 * @id: A watch ID
 *
 * Removes an idle time watcher, previously added by
 * gnome_idle_monitor_add_watch().
 */
void
gnome_idle_monitor_remove_watch (GnomeIdleMonitor *monitor,
				 guint		   id)
{
	g_return_if_fail (GNOME_IS_IDLE_MONITOR (monitor));

	g_hash_table_remove (monitor->priv->watches,
			     GUINT_TO_POINTER (id));
}

/**
 * gnome_idle_monitor_get_idletime:
 * @monitor: A #GnomeIdleMonitor
 *
 * Returns: The user's current idle time, in milliseconds.
 */
gint64
gnome_idle_monitor_get_idletime (GnomeIdleMonitor *monitor)
{
	XSyncValue value;

	if (!XSyncQueryCounter (monitor->priv->display, monitor->priv->counter, &value))
		return FALSE;

	return _xsyncvalue_to_int64 (value);
}
