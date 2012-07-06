#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <locale.h>
#include <glib-object.h>
#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>

int main (int argc, char **argv)
{
        char **langs;
        guint i;

        setlocale (LC_ALL, NULL);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

        if (argc > 1) {
                guint i;
                for (i = 1; i < argc; i++) {
                        char *lang, *norm;
                        norm = gnome_normalize_language_name (argv[i]);
                        lang = gnome_get_language_from_name (norm, NULL);
                        g_print ("%s (norm: %s) == %s\n", argv[i], norm, lang);
                        g_free (norm);
                        g_free (lang);
                }
                return 0;
        }

        langs = gnome_get_all_language_names ();
        if (langs == NULL) {
                g_warning ("No languages found");
                return 1;
        }

        for (i = 0; langs[i] != NULL; i++) {
                char *lang;
                lang = gnome_get_language_from_name (langs[i], NULL);
                g_print ("%s == %s\n", langs[i], lang);
                g_free (lang);
        }

        g_strfreev (langs);

        return 0;
}

