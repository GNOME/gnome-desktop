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

#include <gdesktop-enums.h>
#include <glib.h>
#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-wall-clock.h>
#include <locale.h>
#include <string.h>

#define COLON ":"
#define RATIO "∶"

static void
test_colon_vs_ratio (void)
{
	GnomeWallClock *clock;
	const char *save_locale;
	const char *str;

	/* Save current locale */
	save_locale = setlocale (LC_ALL, NULL);

	/* In the C locale, make sure the time string is formatted with regular
         * colons */
	setlocale (LC_ALL, "C");
	clock = gnome_wall_clock_new ();
	str = gnome_wall_clock_get_clock (clock);
	g_assert (strstr (str, COLON) != NULL);
	g_assert (strstr (str, RATIO) == NULL);
	g_object_unref (clock);

	/* In a UTF8 locale, we want ratio characters and no colons */
	setlocale (LC_ALL, "en_US.utf8");
	clock = gnome_wall_clock_new ();
	str = gnome_wall_clock_get_clock (clock);
	g_assert (strstr (str, COLON) == NULL);
	g_assert (strstr (str, RATIO) != NULL);
	g_object_unref (clock);

	/* ... and same thing with an RTL locale: should be formatted with
         * ratio characters */
	setlocale (LC_ALL, "he_IL.utf8");
	clock = gnome_wall_clock_new ();
	str = gnome_wall_clock_get_clock (clock);
	g_assert (strstr (str, COLON) == NULL);
	g_assert (strstr (str, RATIO) != NULL);
	g_object_unref (clock);

	/* Restore previous locale */
	setlocale (LC_ALL, save_locale);
}

static void
test_clock_format_setting (void)
{
	GnomeWallClock *clock;
	GSettings *settings;
	const char *save_locale;
	const char *str;

	/* Save current locale */
	save_locale = setlocale (LC_ALL, NULL);

	setlocale (LC_ALL, "en_US.utf8");
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

int
main (int   argc,
      char *argv[])
{
	g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);

	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/wall-clock/colon-vs-ratio", test_colon_vs_ratio);
	g_test_add_func ("/wall-clock/24h-clock-format", test_clock_format_setting);

	return g_test_run ();
}
