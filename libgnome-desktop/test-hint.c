/*
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
 * Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>
#include <gconf/gconf-client.h>

#undef GNOME_DISABLE_DEPRECATED
#include <libgnomeui/gnome-hint.h>

int main (int argc, char **argv) {
  GtkWidget *gnome_hint;

  gchar *hintfile, *backimg, *logoimg;

  gnome_program_init("gnome-hint","0.1",LIBGNOMEUI_MODULE,
                                   argc, argv, NULL);

  if (argc !=4){
 	printf("You must specify the location of a hintfile\na background image and a logo image\n\n");
	return (1);
  }

  hintfile = g_strdup(argv[1]);
  backimg = g_strdup(argv[2]);
  logoimg = g_strdup(argv[3]);

  gnome_hint = gnome_hint_new (hintfile, "GNOME Test Hints",
				backimg, logoimg, "/apps/test-hint/startup");

  if (!gnome_hint){
	printf("Bad hintfile\n\n");
	return (2);
  }
 


  g_signal_connect_swapped(GTK_OBJECT(gnome_hint), "destroy",
                           G_CALLBACK (gtk_main_quit), NULL);
  gtk_widget_show_all (GTK_WIDGET(gnome_hint));
  gtk_main ();
  return 0;
}

