/* -*- mode: C; c-file-style: "linux"; indent-tabs-mode: t -*-
 *
 * Copyright (C) 2021 Robert Marcano <robert@marcanoonline.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <glib.h>
#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-bg-slide-show.h>

void
test_starttime (void)
{
	GTimeZone *tz;
	GDateTime *reference_time, *slide_show_time;
	GnomeBGSlideShow *slide_show;
	gboolean loaded;

	tz = g_time_zone_new_local ();
	/* Load reference time to be compared with the one on the XML file */
	reference_time = g_date_time_new_from_iso8601 ("2021-04-02T13:14:15", tz);
	g_assert (reference_time != NULL);

	g_time_zone_unref (tz);

	/* Parsing XML file */
	slide_show = gnome_bg_slide_show_new ("starttime.xml");
	loaded = gnome_bg_slide_show_load (slide_show, NULL);
	g_assert (loaded);

	/* Test if the slide show start time is the same that the reference time */
	slide_show_time = g_date_time_new_from_unix_local (gnome_bg_slide_show_get_start_time (slide_show));
	g_assert (g_date_time_compare (reference_time, slide_show_time) == 0);

	g_date_time_unref (reference_time);
	g_date_time_unref (slide_show_time);
	g_object_unref (slide_show);
}

static void
test_starttime_subprocess (const char *tz_str)
{
	if (g_test_subprocess ()) {
		g_setenv ("TZ", tz_str, TRUE);
		tzset ();
		test_starttime ();
		return;
	}

	g_test_trap_subprocess (NULL, 0, 0);
	g_test_trap_assert_passed ();
}

static void
test_starttime_no_dst (void)
{
	test_starttime_subprocess ("C");
}

static void
test_starttime_dst (void)
{
	test_starttime_subprocess ("US/Eastern");
}

int
main (int   argc,
      char *argv[])
{
	const char *basedir;

	g_test_init (&argc, &argv, NULL);

	basedir = g_getenv ("G_TEST_SRCDIR");
	if (basedir == NULL)
		basedir = INSTALLED_TEST_DIR;

	/* We need to ensure the process' current working directory
	 * is the same as the test data in order to load the test
	 * slide show easily.
	 */
	chdir (basedir);

	g_test_add_func ("/bg-slide-show/starttime-no-dst", test_starttime_no_dst);
	g_test_add_func ("/bg-slide-show/starttime-dst", test_starttime_dst);

	return g_test_run ();
}
