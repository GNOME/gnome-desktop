#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <locale.h>
#include <glib-object.h>
#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>

int main (int argc, char **argv)
{
        char **locales;
        int i;

        setlocale (LC_ALL, "");
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

        if (argc > 1) {
                for (i = 1; i < argc; i++) {
                        char *lang, *country, *norm;
                        norm = gnome_normalize_locale (argv[i]);
                        lang = gnome_get_language_from_locale (norm, NULL);
                        country = gnome_get_country_from_locale (norm, NULL);
                        g_print ("%s (norm: %s) == %s -- %s\n", argv[i], norm, lang, country);
                        g_free (norm);
                        g_free (lang);
                        g_free (country);
                }
                return 0;
        }

        locales = gnome_get_all_locales ();
        if (locales == NULL) {
                g_warning ("No locales found");
                return 1;
        }

        for (i = 0; locales[i] != NULL; i++) {
                char *lang, *country;
                lang = gnome_get_language_from_locale (locales[i], NULL);
                country = gnome_get_country_from_locale (locales[i], NULL);
                g_print ("%s == %s -- %s\n", locales[i], lang, country);
                g_free (lang);
                g_free (country);
        }

        g_strfreev (locales);

        return 0;
}

