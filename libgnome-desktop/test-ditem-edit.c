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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>

#include <libgnomeui/libgnomeui.h>

#undef GNOME_DISABLE_DEPRECATED
#include <libgnomeui/gnome-ditem-edit.h>

static void
changed_callback (GnomeDItemEdit *dee,
		  gpointer        data)
{
	fprintf (stderr, "changed_callback invoked\n");
}

static void
icon_changed_callback (GnomeDItemEdit *dee,
		       gpointer        data)
{
	fprintf (stderr, "icon_changed_callback invoked\n");
}

static void
name_changed_callback (GnomeDItemEdit *dee,
		       gpointer        data)
{
	fprintf (stderr, "name_changed_callback invoked\n");
}

int
main(int argc, char * argv[])
{
        GtkWidget *app;
        GtkWidget *dee;

	if (argc != 2) {
		fprintf (stderr, "usage: test-ditem-edit <.desktop-file>\n");
		return 1;
	}

	gnome_program_init ("gnome-ditem-edit", "0.1",
			    LIBGNOMEUI_MODULE,
			    argc, argv,
			    GNOME_PARAM_APP_DATADIR, DATADIR,
			    NULL);

        app = gtk_window_new (GTK_WINDOW_TOPLEVEL);

        dee = gnome_ditem_edit_new ();
	gtk_container_add (GTK_CONTAINER (app), dee);

        gnome_ditem_edit_load_uri (
		GNOME_DITEM_EDIT (dee), argv [1], NULL);

        g_signal_connect_swapped (app, "delete_event",
				  G_CALLBACK (gtk_widget_destroy), app);

        g_signal_connect (app, "destroy",
			  G_CALLBACK (gtk_main_quit),
			  NULL);

        g_signal_connect (dee, "changed",
                          G_CALLBACK (changed_callback), NULL);

        g_signal_connect (dee, "icon_changed",
                         G_CALLBACK (icon_changed_callback), NULL);

        g_signal_connect (dee, "name_changed",
                         G_CALLBACK (name_changed_callback), NULL);

        gtk_widget_show (dee);
        gtk_widget_show (app);

        gtk_main();

        return 0;
}
