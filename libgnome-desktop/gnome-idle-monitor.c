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

struct _GnomeIdleMonitorPrivate
{
	Display	    *display;

	GHashTable  *watches;
	int	     sync_event_base;
	XSyncCounter counter;

	XSyncAlarm   xalarm_reset;

	GdkDevice   *device;
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
	PROP_0,
	PROP_DEVICE,
	PROP_LAST,
};

static GParamSpec *obj_props[PROP_LAST];

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

#define GINT64_TO_XSYNCVALUE(value, ret) XSyncIntsToValue (ret, value, ((guint64)value) >> 32)

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

	GINT64_TO_XSYNCVALUE (interval, &attr.trigger.wait_value);
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
		GnomeIdleMonitorWatch *watch;
		XSyncAlarmAttributes attr;

		watch = find_watch_for_alarm (monitor, alarm_event->alarm);
		if (watch == NULL)
			return;

		g_signal_emit (monitor, signals[TRIGGERED_IDLE], 0, watch->id);

		if (watch->callback != NULL) {
			watch->callback (monitor,
					 watch->id,
					 watch->user_data);
		}

		/* Reset the alarm so it can be triggered again */
		attr.events = TRUE;
		XSyncChangeAlarm (watch->display, watch->xalarm, XSyncCAEvents, &attr);
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

static char *
counter_name_for_device (GdkDevice *device)
{
	if (device) {
		gint device_id = gdk_x11_device_get_id (device);
		if (device_id > 0)
			return g_strdup_printf ("DEVICEIDLETIME %d", device_id);
	}

	return g_strdup ("IDLETIME");
}

static XSyncCounter
find_idletime_counter (GnomeIdleMonitor *monitor)
{
	int		    i;
	int		    ncounters;
	XSyncSystemCounter *counters;
	XSyncCounter        counter = None;
	char               *counter_name;

	counter_name = counter_name_for_device (monitor->priv->device);
	counters = XSyncListSystemCounters (monitor->priv->display, &ncounters);
	for (i = 0; i < ncounters; i++) {
		if (counters[i].name != NULL && strcmp (counters[i].name, counter_name) == 0) {
			counter = counters[i].counter;
			break;
		}
	}
	XSyncFreeSystemCounterList (counters);
	g_free (counter_name);

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

	monitor->priv->counter = find_idletime_counter (monitor);
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

	gdk_window_remove_filter (NULL, (GdkFilterFunc)xevent_filter, monitor);

	g_clear_object (&monitor->priv->device);

	G_OBJECT_CLASS (gnome_idle_monitor_parent_class)->dispose (object);
}

static void
gnome_idle_monitor_get_property (GObject    *object,
				 guint       prop_id,
				 GValue     *value,
				 GParamSpec *pspec)
{
	GnomeIdleMonitor *monitor = GNOME_IDLE_MONITOR (object);
	switch (prop_id)
	{
	case PROP_DEVICE:
		g_value_set_object (value, monitor->priv->device);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gnome_idle_monitor_set_property (GObject      *object,
				 guint         prop_id,
				 const GValue *value,
				 GParamSpec   *pspec)
{
	GnomeIdleMonitor *monitor = GNOME_IDLE_MONITOR (object);
	switch (prop_id)
	{
	case PROP_DEVICE:
		monitor->priv->device = g_value_dup_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gnome_idle_monitor_constructed (GObject *object)
{
	GnomeIdleMonitor *monitor = GNOME_IDLE_MONITOR (object);

	monitor->priv->display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
	init_xsync (monitor);
}

static void
gnome_idle_monitor_class_init (GnomeIdleMonitorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gnome_idle_monitor_dispose;
	object_class->constructed = gnome_idle_monitor_constructed;
	object_class->get_property = gnome_idle_monitor_get_property;
	object_class->set_property = gnome_idle_monitor_set_property;

	/**
	 * GnomeIdleMonitor:device:
	 *
	 * The device to listen to idletime on.
	 */
	obj_props[PROP_DEVICE] =
		g_param_spec_object ("device",
				     "Device",
				     "The device to listen to idletime on",
				     GDK_TYPE_DEVICE,
				     G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
	g_object_class_install_property (object_class, PROP_DEVICE, obj_props[PROP_DEVICE]);

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
}

/**
 * gnome_idle_monitor_new:
 *
 * Returns: a new #GnomeIdleMonitor that tracks the server-global
 * idletime for all devices. To track device-specific idletime,
 * use gnome_idle_monitor_new_for_device().
 */
GnomeIdleMonitor *
gnome_idle_monitor_new (void)
{
	return GNOME_IDLE_MONITOR (g_object_new (GNOME_TYPE_IDLE_MONITOR, NULL));
}

/**
 * gnome_idle_monitor_new_for_device:
 * @device: A #GdkDevice to get the idle time for.
 *
 * Returns: a new #GnomeIdleMonitor that tracks the device-specific
 * idletime for @device. To track server-global idletime for all
 * devices, use gnome_idle_monitor_new().
 */
GnomeIdleMonitor *
gnome_idle_monitor_new_for_device (GdkDevice *device)
{
	return GNOME_IDLE_MONITOR (g_object_new (GNOME_TYPE_IDLE_MONITOR,
						 "device", device, NULL));
}

/**
 * gnome_idle_monitor_add_watch:
 * @monitor: A #GnomeIdleMonitor
 * @interval_msec: The idletime interval, in milliseconds
 * @callback: (allow-none): The callback to call when the user has
 *     accumulated @interval seconds of idle time.
 * @user_data: (allow-none): The user data to pass to the callback
 * @notify: A #GDestroyNotify
 *
 * Returns: a watch id
 *
 * Adds a watch for a specific idle time. The callback will be called
 * when the user has accumulated @interval_msec milliseconds of idle time.
 * This function will return an ID that can either be passed to
 * gnome_idle_monitor_remove_watch(), or can be used to tell idle time
 * watches apart if you have more than one.
 *
 * If you need to check for more than one interval of
 * idle time, it may be more convenient to connect to the
 * #GnomeIdleMonitor::triggered-idle signal, and filter on the
 * watch ID received in the signal handler.
 *
 * Also note that this function will only care about positive transitions
 * (user's idle time exceeding a certain time). If you want to know about
 * negative transitions, connect to the #GnomeIdleMonitor::became-active
 * signal.
 */
guint
gnome_idle_monitor_add_watch (GnomeIdleMonitor	       *monitor,
			      guint			interval_msec,
			      GnomeIdleMonitorWatchFunc callback,
			      gpointer			user_data,
			      GDestroyNotify		notify)
{
	GnomeIdleMonitorWatch *watch;

	g_return_val_if_fail (GNOME_IS_IDLE_MONITOR (monitor), 0);

	watch = idle_monitor_watch_new ();
	watch->display = monitor->priv->display;
	watch->callback = callback;
	watch->user_data = user_data;
	watch->notify = notify;
	watch->xalarm = _xsync_alarm_set (monitor, XSyncPositiveTransition, interval_msec);

	g_hash_table_insert (monitor->priv->watches,
			     GUINT_TO_POINTER (watch->id),
			     watch);
	return watch->id;
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
 * Returns: The current idle time, in milliseconds, or -1 for not supported
 */
gint64
gnome_idle_monitor_get_idletime (GnomeIdleMonitor *monitor)
{
	XSyncValue value;

	if (!XSyncQueryCounter (monitor->priv->display, monitor->priv->counter, &value))
		return -1;

	return _xsyncvalue_to_int64 (value);
}
