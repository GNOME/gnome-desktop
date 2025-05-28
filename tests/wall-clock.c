/* -*- mode: C; c-file-style: "linux"; indent-tabs-mode: t -*-
 *
 * Copyright (C) 2014 Red Hat, Inc.
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

#include "config.h"

#include <gdesktop-enums.h>
#include <glib.h>
#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-wall-clock.h>
#include <locale.h>
#include <string.h>

#define SPACE "  "
#define EN_SPACE " "

static void
test_utf8_character (const char *utf8_char,
                     const char *non_utf8_fallback)
{
	GDateTime  *datetime;
	GnomeWallClock *clock;
	const char *save_locale;
	const char *str;

	/* When testing that UTF8 locales don't use double spaces
	   to separate date and time, make sure the date itself
	   doesn't contain double spaces ("Aug  1") */
	datetime = g_date_time_new_local (2014, 5, 28, 23, 59, 59);

	/* In the C locale, make sure the time string is formatted with regular
         * colons */
	save_locale = setlocale (LC_ALL, NULL);
	clock = gnome_wall_clock_new ();
	str = gnome_wall_clock_string_for_datetime (clock,
	                                            datetime,
	                                            G_DESKTOP_CLOCK_FORMAT_24H,
	                                            TRUE, TRUE, TRUE);
	g_assert (strstr (str, non_utf8_fallback) != NULL);
	g_assert (strstr (str, utf8_char) == NULL);
	g_object_unref (clock);

	/* In a UTF8 locale, we want ratio characters and no colons. */
	setlocale (LC_ALL, "en_US.UTF-8");
	clock = gnome_wall_clock_new ();
	str = gnome_wall_clock_string_for_datetime (clock,
	                                            datetime,
	                                            G_DESKTOP_CLOCK_FORMAT_24H,
	                                            TRUE, TRUE, TRUE);
	g_assert (strstr (str, non_utf8_fallback) == NULL);
	g_assert (strstr (str, utf8_char) != NULL);
	g_object_unref (clock);

	/* ... and same thing with an RTL locale: should be formatted with
         * ratio characters */
	setlocale (LC_ALL, "he_IL.UTF-8");
	clock = gnome_wall_clock_new ();
	str = gnome_wall_clock_string_for_datetime (clock,
	                                            datetime,
	                                            G_DESKTOP_CLOCK_FORMAT_24H,
	                                            TRUE, TRUE, TRUE);
	g_assert (strstr (str, non_utf8_fallback) == NULL);
	g_assert (strstr (str, utf8_char) != NULL);
	g_object_unref (clock);

	g_date_time_unref (datetime);

	/* Restore previous locale */
	setlocale (LC_ALL, save_locale);
}

static void
test_space_vs_en_space (void)
{
	test_utf8_character (EN_SPACE, SPACE);
}

static void
test_clock_format_setting (void)
{
	GnomeWallClock *clock;
	GSettings *settings;
	const char *save_locale;
	const char *str;

	save_locale = setlocale (LC_ALL, NULL);
	setlocale (LC_ALL, "en_US.UTF-8");

	settings = g_settings_new ("org.gnome.desktop.interface");

	/* In 12h format, the string ends with AM or PM */
	g_settings_set_enum (settings, "clock-format", G_DESKTOP_CLOCK_FORMAT_12H);
	clock = gnome_wall_clock_new ();
	str = gnome_wall_clock_get_clock (clock);
	g_assert (g_str_has_suffix (str, "AM") || g_str_has_suffix (str, "PM"));
	g_object_unref (clock);

	/* After setting the 24h format, AM / PM should be gone */
	g_settings_set_enum (settings, "clock-format", G_DESKTOP_CLOCK_FORMAT_24H);
	clock = gnome_wall_clock_new ();
	str = gnome_wall_clock_get_clock (clock);
	g_assert (!g_str_has_suffix (str, "AM") && !g_str_has_suffix (str, "PM"));
	g_object_unref (clock);

	g_object_unref (settings);

	/* Restore previous locale */
	setlocale (LC_ALL, save_locale);
}

static gboolean
on_notify_clock_timeout (gpointer user_data)
{
	g_error ("Timeout waiting for notify::clock");
	g_assert_not_reached ();
	return FALSE;
}

static void
on_clock_changed (GnomeWallClock *clock,
                  GParamSpec     *pspec,
                  gpointer        user_data)
{
	GMainLoop *main_loop = user_data;

	/* Nothing much to do here; the test was just to ensure we get the callback */

	g_main_loop_quit (main_loop);
}

static void
test_notify_clock (void)
{
	GMainLoop *main_loop;
	GnomeWallClock *clock;
	GSettings *settings;

	main_loop = g_main_loop_new (NULL, FALSE);
	settings = g_settings_new ("org.gnome.desktop.interface");

	/* Show seconds so we don't have to wait too long for the callback */
	g_settings_set_boolean (settings, "clock-show-seconds", TRUE);

	clock = gnome_wall_clock_new ();
	g_signal_connect (clock, "notify::clock", G_CALLBACK (on_clock_changed), main_loop);
	g_timeout_add_seconds (5, on_notify_clock_timeout, NULL);

	g_main_loop_run (main_loop);

	g_main_loop_unref (main_loop);
	g_object_unref (clock);
	g_object_unref (settings);
}

static void
test_weekday_setting (void)
{
	GnomeWallClock *clock;
	GSettings *settings;
	const char *save_locale;
	const char *str, *ptr, *s;

	/* Save current locale */
	save_locale = setlocale (LC_ALL, NULL);
	setlocale (LC_ALL, "C");
	settings = g_settings_new ("org.gnome.desktop.interface");

	/* Set 24h format, so that the only alphabetical part will be the weekday */
	g_settings_set_enum (settings, "clock-format", G_DESKTOP_CLOCK_FORMAT_24H);

	g_settings_set_boolean (settings, "clock-show-weekday", FALSE);
	g_settings_set_boolean (settings, "clock-show-date", FALSE);
	clock = gnome_wall_clock_new ();
	str = gnome_wall_clock_get_clock (clock);

	/* Verify that no character is alphabetical */
	for (s = str; *s != '\0'; s++)
		g_assert (!g_ascii_isalpha (*s));

	g_object_unref (clock);

	g_settings_set_boolean (settings, "clock-show-weekday", TRUE);
	clock = gnome_wall_clock_new ();
	str = gnome_wall_clock_get_clock (clock);

	/* Verify that every character before the first space is alphabetical */
	ptr = strchr (str, ' ');
	g_assert (ptr != NULL);

	for (s = str; s != ptr; s++)
		g_assert (g_ascii_isalpha (*s));

	for (s = ptr; *s != '\0'; s++)
		g_assert (!g_ascii_isalpha (*s));

	g_object_unref (clock);

	g_object_unref (settings);

	/* Restore previous locale */
	setlocale (LC_ALL, save_locale);
}

int
main (int   argc,
      char *argv[])
{
	g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);

	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/wall-clock/space-vs-en-space", test_space_vs_en_space);
	g_test_add_func ("/wall-clock/24h-clock-format", test_clock_format_setting);
	g_test_add_func ("/wall-clock/notify-clock", test_notify_clock);
	g_test_add_func ("/wall-clock/weekday-setting", test_weekday_setting);

	return g_test_run ();
}
