/* -*- Mode: C; c-set-style: gnu indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>

#include "gnome-desktop-item.h"
#include <libgnomevfs/gnome-vfs-utils.h>
/*
#include <libgnomeui/gnome-ditem-edit.h>
*/

#include <locale.h>
#include <stdlib.h>

static void
test_ditem (const char *file)
{
	GnomeDesktopItem *ditem;
	GnomeDesktopItemType type;
	const gchar *text;
	char *uri;
	GSList *list, *c;
	char path[256];

	ditem = gnome_desktop_item_new_from_file (file,
						  GNOME_DESKTOP_ITEM_LOAD_ONLY_IF_EXISTS,
						  NULL);
	if (ditem == NULL) {
		g_print ("File %s is not an existing ditem\n", file);
		return;
	}

	text = gnome_desktop_item_get_location (ditem);
	g_print ("LOCATION: |%s|\n", text);

	type = gnome_desktop_item_get_entry_type (ditem);
	g_print ("TYPE: |%d|\n", type);

	text = gnome_desktop_item_get_string
		(ditem, GNOME_DESKTOP_ITEM_TYPE);
	g_print ("TYPE(string): |%s|\n", text);

	text = gnome_desktop_item_get_string
		(ditem, GNOME_DESKTOP_ITEM_EXEC);
	g_print ("EXEC: |%s|\n", text);

	text = gnome_desktop_item_get_string
		(ditem, GNOME_DESKTOP_ITEM_ICON);
	g_print ("ICON: |%s|\n", text);

	text = gnome_desktop_item_get_localestring
		(ditem, GNOME_DESKTOP_ITEM_NAME);
	g_print ("NAME: |%s|\n", text);

	text = gnome_desktop_item_get_localestring_lang
		(ditem, GNOME_DESKTOP_ITEM_NAME,
		 "cs_CZ");
	g_print ("NAME(lang=cs_CZ): |%s|\n", text);

	text = gnome_desktop_item_get_localestring_lang
		(ditem, GNOME_DESKTOP_ITEM_NAME,
		 "de");
	g_print ("NAME(lang=de): |%s|\n", text);


	text = gnome_desktop_item_get_localestring_lang
		(ditem, GNOME_DESKTOP_ITEM_NAME,
		 NULL);
	g_print ("NAME(lang=null): |%s|\n", text);

	text = gnome_desktop_item_get_localestring
		(ditem, GNOME_DESKTOP_ITEM_COMMENT);
	g_print ("COMMENT: |%s|\n", text);

	g_print ("Setting Name[de]=Neu gestzt! (I have no idea what that means)\n");
	gnome_desktop_item_set_localestring
		(ditem,
		 GNOME_DESKTOP_ITEM_NAME,
		 "Neu gesetzt!");

	getcwd (path, 255 - strlen ("/foo.desktop"));
	strcat (path, "/foo.desktop");

	g_print ("Saving to foo.desktop\n");
	uri = gnome_vfs_get_uri_from_local_path (path);
	g_print ("URI: %s\n", uri);
	gnome_desktop_item_save (ditem, uri, FALSE, NULL);
	g_free (uri);
}


int
main (int argc, char **argv)
{
	char *file;

	if (argc != 2) {
		fprintf (stderr, "One argument, with name of .desktop file allowed\n");
		exit (1);
	}

	file = g_strdup (argv[1]);

	gnome_program_init ("test-ditem", "0.01",
			    LIBGNOMEUI_MODULE,
			    argc, argv, NULL);

	test_ditem (file);

	/*
	test_ditem_edit (file);
	*/

	return 0;
}
