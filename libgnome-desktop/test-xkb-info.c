#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-xkb-info.h>
int
main (int argc, char **argv)
{
	GnomeXkbInfo *info;
	GList *layouts, *l;
	GList *option_groups, *g;
	GList *options, *o;

	info = gnome_xkb_info_new ();

	g_print ("layouts:\n");
	layouts = gnome_xkb_info_get_all_layouts (info);
	for (l = layouts; l != NULL; l = l->next) {
		const char *id = l->data;
		const char *display_name;
		const char *short_name;
		const char *xkb_layout;
		const char *xkb_variant;

		if (gnome_xkb_info_get_layout_info (info, id,
						    &display_name,
						    &short_name,
						    &xkb_layout,
						    &xkb_variant) == FALSE) {
			g_warning ("Failed to get info for layout '%s'", id);
		} else {
			char *name = g_uri_escape_string (display_name,
							  " (),<>+;:",
							  TRUE);
			g_print ("  %s:\n", id);
			g_print ("    display name: \"%s\"\n", name);
			g_print ("    short name: %s\n", short_name);
			g_print ("    xkb layout: %s\n", xkb_layout);
			g_print ("    xkb variant: %s\n", xkb_variant);

			g_free (name);
		}
	}
	g_list_free (layouts);

	g_print ("\noption groups:\n");
	option_groups = gnome_xkb_info_get_all_option_groups (info);
	for (g = option_groups; g != NULL; g = g->next) {
		const char *group_id = g->data;

		g_print ("  %s:\n", group_id);

		options = gnome_xkb_info_get_options_for_group (info, group_id);
		for (o = options; o != NULL; o = o->next) {
			const char *id = o->data;
			const char *description = gnome_xkb_info_description_for_option (info,
											 group_id,
											 id);
			char *desc = g_uri_escape_string (description,
							  " (),<>+;:",
							  TRUE);

			g_print ("    %s:\n", id);
			g_print ("      description: \"%s\"\n", desc);
			g_free (desc);

		}
		g_list_free (options);
	}
	g_list_free (option_groups);

	g_object_unref (info);

	return 0;
}
