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

GnomeThemeFile *gnome_theme_file_new             (void);
GnomeThemeFile *gnome_theme_file_new_from_string (char                    *data,
						  GError                 **error);
char *          gnome_theme_file_to_string       (GnomeThemeFile          *df);
void            gnome_theme_file_free            (GnomeThemeFile          *df);
void            gnome_theme_file_launch          (GnomeThemeFile          *df,
						  char                   **argv,
						  int                      argc,
						  GError                 **error);



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
