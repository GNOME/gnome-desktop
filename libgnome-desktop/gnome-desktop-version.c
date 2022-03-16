/* gnome-desktop-version.c
 *
 * Copyright (C) 2022 Sergio Costas
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <libgnome-desktop/gnome-desktop-version.h>

/**
 * gnome_get_platform_version:
 *
 * Returns an integer with the major version of GNOME. Useful for
 * dynamic languages like Javascript or Python (static languages like
 * C should use %GNOME_DESKTOP_PLATFORM_VERSION). If this
 * function doesn't exist, it can be presumed that the GNOME platform
 * version is 42 or previous.
 *
 * Return value: an integer with the major version of GNOME.
 *
 * Since: 43.0
 **/
int
gnome_get_platform_version (void)
{
  return GNOME_DESKTOP_PLATFORM_VERSION;
}
