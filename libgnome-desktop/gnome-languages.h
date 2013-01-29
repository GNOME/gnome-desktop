/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2008 Red Hat, Inc.
 * Copyright 2007 William Jon McCann <mccann@jhu.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Written by: Ray Strode
 *             William Jon McCann
 */

#ifndef __GNOME_LANGUAGES_H
#define __GNOME_LANGUAGES_H

#ifndef GNOME_DESKTOP_USE_UNSTABLE_API
#error    This is unstable API. You must define GNOME_DESKTOP_USE_UNSTABLE_API before including gnome-languages.h
#endif

G_BEGIN_DECLS

char *        gnome_get_language_from_name  (const char *name,
                                             const char *locale);
char *        gnome_get_region_from_name    (const char *name,
                                             const char *locale);
char **       gnome_get_all_language_names  (void);
gboolean      gnome_parse_language_name     (const char *name,
                                             char      **language_codep,
                                             char      **territory_codep,
                                             char      **codesetp,
                                             char      **modifierp);
char *        gnome_normalize_language_name (const char *name);
gboolean      gnome_language_has_translations (const char *language_name);
char *        gnome_get_language_from_code  (const char *code,
                                             const char *locale);
char *        gnome_get_country_from_code   (const char *code,
                                             const char *locale);
gboolean      gnome_get_input_source_from_locale (const char  *locale,
                                                  const char **type,
                                                  const char **id);

G_END_DECLS

#endif /* __GNOME_LANGUAGES_H */
