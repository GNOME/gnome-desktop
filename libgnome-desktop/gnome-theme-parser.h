/* GnomeThemeParser - a parser of icon-theme files
 * gnome-theme-parser.h Copyright (C) 2002 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef GNOME_THEME_PARSER_H
#define GNOME_THEME_PARSER_H

#include <glib.h>

typedef struct _GnomeThemeFile GnomeThemeFile;

typedef void (* GnomeThemeFileSectionFunc) (GnomeThemeFile *df,
					    const char       *name,
					    gpointer          data);
typedef void (* GnomeThemeFileLineFunc) (GnomeThemeFile *df,
					 const char       *key, /* If NULL, value is comment line */
					 const char       *locale,
					 const char       *value, /* This is raw unescaped data */
					 gpointer          data);

typedef enum 
{
  GNOME_THEME_FILE_PARSE_ERROR_INVALID_SYNTAX,
  GNOME_THEME_FILE_PARSE_ERROR_INVALID_ESCAPES,
  GNOME_THEME_FILE_PARSE_ERROR_INVALID_CHARS
} GnomeThemeFileParseError;

#define GNOME_THEME_FILE_PARSE_ERROR gnome_theme_file_parse_error_quark()
GQuark gnome_theme_file_parse_error_quark (void);

GnomeThemeFile *gnome_theme_file_new_from_string (char                    *data,
						  GError                 **error);
char *          gnome_theme_file_to_string       (GnomeThemeFile          *df);
void            gnome_theme_file_free            (GnomeThemeFile          *df);


void                   gnome_theme_file_foreach_section (GnomeThemeFile            *df,
							 GnomeThemeFileSectionFunc  func,
							 gpointer                   user_data);
void                   gnome_theme_file_foreach_key     (GnomeThemeFile            *df,
							 const char                *section,
							 gboolean                   include_localized,
							 GnomeThemeFileLineFunc     func,
							 gpointer                   user_data);


/* Gets the raw text of the key, unescaped */
gboolean gnome_theme_file_get_raw            (GnomeThemeFile   *df,
					      const char       *section,
					      const char       *keyname,
					      const char       *locale,
					      char            **val);
gboolean gnome_theme_file_get_integer        (GnomeThemeFile   *df,
					      const char       *section,
					      const char       *keyname,
					      int              *val);
gboolean gnome_theme_file_get_string         (GnomeThemeFile   *df,
					      const char       *section,
					      const char       *keyname,
					      char            **val);
gboolean gnome_theme_file_get_locale_string  (GnomeThemeFile   *df,
					      const char       *section,
					      const char       *keyname,
					      char            **val);

#endif /* GNOME_THEME_PARSER_H */
