#ifndef __GNOME_HINT_H__
#define __GNOME_HINT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <gnome.h>

#define GNOME_TYPE_HINT			(gnome_hint_get_type ())
#define GNOME_HINT(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_HINT, GnomeHint))
#define GNOME_HINT_CLASS(klass)		(G_TYPE_CHECK_CAST ((klass), GNOME_TYPE_HINT))
#define GNOME_IS_HINT(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_TYPE_HINT))
#define GNOME_IS_HINT_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_HINT))
#define GNOME_HINT_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GNOME_TYPE_HINT, GnomeHintClass))

typedef struct _GnomeHint		GnomeHint;
typedef struct _GnomeHintClass		GnomeHintClass;
typedef struct _GnomeHintPrivate	GnomeHintPrivate;

struct _GnomeHint
{
	GnomeDialog parent_instance;

	GnomeHintPrivate *_priv;
};

struct _GnomeHintClass
{
	GnomeDialogClass parent_class;
};

GType gnome_hint_get_type (void);
GtkWidget * gnome_hint_new (const gchar *hintfile,
			    const gchar *title,
			    const gchar *background_image,
			    const gchar *logo_image);

#ifdef __cplusplus
}
#endif

#endif
