#include <gtk/gtk.h>
#define GNOME_DESKTOP_USE_UNSTABLE_API
#include "libgnome-desktop/gnome-idle-monitor.h"

#define IDLE_TIME 1000 * 5 /* 5 seconds */

static void
active_watch_func (GnomeIdleMonitor *monitor,
		   guint             id,
		   gpointer          user_data)
{
	g_message ("Active watch func called (watch id %d)", id);
}

static void
ensure_active_watch (GnomeIdleMonitor *monitor)
{
	guint watch_id;

	watch_id = gnome_idle_monitor_add_user_active_watch (monitor,
							     active_watch_func,
							     NULL,
							     NULL);
	g_message ("Added active watch ID %d", watch_id);
}

static void
idle_watch_func (GnomeIdleMonitor      *monitor,
		 guint                  id,
		 gpointer               user_data)
{
	g_message ("Idle watch func called (watch id %d)", id);
	ensure_active_watch (monitor);
}

int main (int argc, char **argv)
{
	GnomeIdleMonitor *monitor;
	guint watch_id;

	gtk_init (&argc, &argv);

	monitor = gnome_idle_monitor_new ();
	watch_id = gnome_idle_monitor_add_idle_watch (monitor,
						      IDLE_TIME,
						      idle_watch_func,
						      NULL,
						      NULL);
	g_message ("Added idle watch ID %d",
		   watch_id);

	ensure_active_watch (monitor);

	gtk_main ();

	return 0;
}
