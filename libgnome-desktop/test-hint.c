#include <config.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>
#include <gconf/gconf-client.h>

#include "gnome-hint.h"

int main (int argc, char **argv) {
  GtkWidget *gnome_hint;
  GnomeProgram *gnome_hint_program;
  GConfClient* client;

  gchar *hintfile, *backimg, *logoimg;

  gnome_hint_program = gnome_program_init("gnome-hint","0.1",LIBGNOMEUI_MODULE,
                                   argc, argv, NULL);

  if (argc !=4){
 	printf("You must specify the location of a hintfile\na background image and a logo image\n\n");
	return (1);
  }

  hintfile = g_strdup(argv[1]);
  backimg = g_strdup(argv[2]);
  logoimg = g_strdup(argv[3]);

  client = gconf_client_get_default();
/*  gconf_client_add_dir(client,"/apps/test-hint/",
	GCONF_CLIENT_PRELOAD_NONE,NULL); */

  gnome_hint = gnome_hint_new (hintfile, "GNOME Test Hints",
				backimg, logoimg, "/apps/test-hint/startup");

  if (!gnome_hint){
	printf("Bad hintfile\n\n");
	return (2);
  }
 
  gtk_signal_connect(GTK_OBJECT(gnome_hint), "destroy",
		GTK_SIGNAL_FUNC (gtk_main_quit), NULL);

  gtk_widget_show_all (GTK_WIDGET(gnome_hint));
  gtk_main ();
  return 0;
}

