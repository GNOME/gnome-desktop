#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-wall-clock.h>

static void
update_clock (GnomeWallClock *clock)
{
  g_print ("%s\n", gnome_wall_clock_get_clock (clock));
}

static void
on_clock_changed (GnomeWallClock *clock,
		  GParamSpec     *pspec,
		  gpointer        user_data)
{
  update_clock (clock);
}

int
main (int    argc,
      char **argv)
{
  GnomeWallClock *clock;
  GMainLoop *loop;

  loop = g_main_loop_new (NULL, TRUE);

  clock = gnome_wall_clock_new ();
  g_signal_connect (clock, "notify::clock", G_CALLBACK (on_clock_changed), NULL);
  update_clock (clock);

  g_main_loop_run (loop);

  return 0;
}
