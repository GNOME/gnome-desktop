/* -*- mode: C; c-file-style: "linux"; indent-tabs-mode: t -*-
 *
 * Copyright (C) 2021 Dan Cîrnaț <dan@alt.md>
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
#include <libgnome-desktop/gnome-languages.h>
#include <locale.h>
#include <string.h>

static void
test_using_null_locale (void)
{
	const char *translated_territory;
	const char *translated_language;
	const char *translated_modifier;

	translated_territory = gnome_get_country_from_code("US", NULL);
	g_assert (translated_territory != NULL);

	translated_language = gnome_get_language_from_locale("ro", NULL);
	g_assert (translated_language != NULL);

	translated_modifier = gnome_get_translated_modifier("euro", NULL);
	g_assert (translated_modifier != NULL);

}

int
main (int   argc,
      char *argv[])
{
	g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);

	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/languages/using-null-locale", test_using_null_locale);

	return g_test_run ();
}
