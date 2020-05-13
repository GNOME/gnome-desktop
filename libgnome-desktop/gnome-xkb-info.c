/*
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * Written by: Rui Matos <rmatos@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <config.h>

#include <xkbcommon/xkbregistry.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gio/gio.h>

#include <glib/gi18n-lib.h>
#define XKEYBOARD_CONFIG_(String) ((char *) g_dgettext ("xkeyboard-config", String))

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include "gnome-languages.h"
#include "gnome-xkb-info.h"

#ifndef XKB_RULES_FILE
#define XKB_RULES_FILE "evdev"
#endif

typedef struct _Layout Layout;
struct _Layout
{
  gchar *id;
  gchar *xkb_name;
  gchar *short_desc;
  gchar *description;
  gboolean is_variant;
  const Layout *main_layout;
  GSList *iso639Ids;
  GSList *iso3166Ids;
};

typedef struct _XkbOption XkbOption;
struct _XkbOption
{
  gchar *id;
  gchar *description;
};

typedef struct _XkbOptionGroup XkbOptionGroup;
struct _XkbOptionGroup
{
  gchar *id;
  gchar *description;
  gboolean allow_multiple_selection;
  GHashTable *options_table;
};

struct _GnomeXkbInfoPrivate
{
  GHashTable *option_groups_table;
  GHashTable *layouts_by_country;
  GHashTable *layouts_by_language;
  GHashTable *layouts_table;
};

G_DEFINE_TYPE_WITH_CODE (GnomeXkbInfo, gnome_xkb_info, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GnomeXkbInfo));

static void
free_layout (gpointer data)
{
  Layout *layout = data;

  g_return_if_fail (layout != NULL);

  g_free (layout->id);
  g_free (layout->xkb_name);
  g_free (layout->short_desc);
  g_free (layout->description);
  g_slist_free_full (layout->iso639Ids, g_free);
  g_slist_free_full (layout->iso3166Ids, g_free);
  g_slice_free (Layout, layout);
}

static void
free_option (gpointer data)
{
  XkbOption *option = data;

  g_return_if_fail (option != NULL);

  g_free (option->id);
  g_free (option->description);
  g_slice_free (XkbOption, option);
}

static void
free_option_group (gpointer data)
{
  XkbOptionGroup *group = data;

  g_return_if_fail (group != NULL);

  g_free (group->id);
  g_free (group->description);
  g_hash_table_destroy (group->options_table);
  g_slice_free (XkbOptionGroup, group);
}

static void
add_layout_to_table (GHashTable  *table,
                     const gchar *key,
                     Layout      *layout)
{
  GHashTable *set;

  if (!layout->id)
    return;

  set = g_hash_table_lookup (table, key);
  if (!set)
    {
      set = g_hash_table_new (g_str_hash, g_str_equal);
      g_hash_table_replace (table, g_strdup (key), set);
    }
  else
    {
      if (g_hash_table_contains (set, layout->id))
        return;
    }
  g_hash_table_replace (set, layout->id, layout);
}

static void
add_layout_to_locale_tables (Layout     *layout,
                             GHashTable *layouts_by_language,
                             GHashTable *layouts_by_country)
{
  GSList *l, *lang_codes, *country_codes;
  gchar *language, *country;

  lang_codes = layout->iso639Ids;
  country_codes = layout->iso3166Ids;

  if (layout->is_variant)
    {
      if (!lang_codes)
        lang_codes = layout->main_layout->iso639Ids;
      if (!country_codes)
        country_codes = layout->main_layout->iso3166Ids;
    }

  for (l = lang_codes; l; l = l->next)
    {
      language = gnome_get_language_from_code ((gchar *) l->data, NULL);
      if (language)
        {
          add_layout_to_table (layouts_by_language, language, layout);
          g_free (language);
        }
    }

  for (l = country_codes; l; l = l->next)
    {
      country = gnome_get_country_from_code ((gchar *) l->data, NULL);
      if (country)
        {
          add_layout_to_table (layouts_by_country, country, layout);
          g_free (country);
        }
    }
}

enum layout_subsets {
    ONLY_MAIN_LAYOUTS,
    ONLY_VARIANTS,
};

static void
add_layouts (GnomeXkbInfo  *self, struct rxkb_context *ctx,
	     enum layout_subsets which)
{
  GnomeXkbInfoPrivate *priv = self->priv;
  struct rxkb_layout *layout;

  for (layout = rxkb_layout_first (ctx);
       layout;
       layout = rxkb_layout_next (layout))
    {
      struct rxkb_iso639_code *iso639;
      struct rxkb_iso3166_code *iso3166;
      const char *name, *variant;
      Layout *l;

      name = rxkb_layout_get_name (layout);
      variant = rxkb_layout_get_variant (layout);

      if ((which == ONLY_VARIANTS && variant == NULL) ||
          (which == ONLY_MAIN_LAYOUTS && variant != NULL))
          continue;

      l = g_slice_new0 (Layout);
      if (variant)
        {
          /* This relies on the main layouts being added first */
          l->main_layout = g_hash_table_lookup (priv->layouts_table, name);
          if (l->main_layout == NULL)
           {
               /* This is a bug in libxkbregistry */
               g_warning ("Ignoring variant '%s(%s)' without a main layout",
                          name, variant);
               g_free (l);
               continue;
           }

          l->xkb_name = g_strdup (variant);
          l->is_variant = TRUE;
          l->id = g_strjoin ("+", name, variant, NULL);
        }
      else
        {
          l->xkb_name = g_strdup (name);
          l->id = g_strdup (name);
        }
      l->description = g_strdup (rxkb_layout_get_description (layout));
      l->short_desc = g_strdup (rxkb_layout_get_brief (layout));
      for (iso639 = rxkb_layout_get_iso639_first (layout);
           iso639;
           iso639 = rxkb_iso639_code_next (iso639))
        {
          char *id = g_strdup (rxkb_iso639_code_get_code (iso639));
          l->iso3166Ids = g_slist_prepend (l->iso3166Ids, id);
        }
      for (iso3166 = rxkb_layout_get_iso3166_first (layout);
           iso3166;
           iso3166 = rxkb_iso3166_code_next (iso3166))
        {
          char *id = g_strdup (rxkb_iso3166_code_get_code (iso3166));
          l->iso3166Ids = g_slist_prepend (l->iso3166Ids, id);
        }

      g_hash_table_replace (priv->layouts_table, l->id, l);
      add_layout_to_locale_tables (l,
                                   priv->layouts_by_language,
                                   priv->layouts_by_country);

   }
}
static bool
parse_rules_file (GnomeXkbInfo  *self, const char *ruleset,
                  bool include_extras)
{
  GnomeXkbInfoPrivate *priv = self->priv;
  struct rxkb_context *ctx;
  struct rxkb_option_group *group;
  enum rxkb_context_flags flags = RXKB_CONTEXT_NO_FLAGS;

  if (include_extras)
      flags |= RXKB_CONTEXT_LOAD_EXOTIC_RULES;

  ctx = rxkb_context_new (flags);
  if (!rxkb_context_parse (ctx, ruleset)) {
      rxkb_context_unref (ctx);
      return FALSE;
  }

  /* libxkbregistry doesn't guarantee a sorting order of the layouts but we
   * want to reference the main layout from the variants. So populate with
   * the main layouts first, then add the variants */
  add_layouts (self, ctx, ONLY_MAIN_LAYOUTS);
  add_layouts (self, ctx, ONLY_VARIANTS);

  for (group = rxkb_option_group_first (ctx);
       group;
       group = rxkb_option_group_next (group))
    {
        XkbOptionGroup *g;
        struct rxkb_option *option;

        g = g_slice_new (XkbOptionGroup);
        g->id = g_strdup (rxkb_option_group_get_name (group));
        g->description = g_strdup (rxkb_option_group_get_description (group));
        g->options_table = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                  NULL, free_option);
        g->allow_multiple_selection = rxkb_option_group_allows_multiple (group);
        g_hash_table_replace (priv->option_groups_table, g->id, g);

        for (option = rxkb_option_first (group);
             option;
             option = rxkb_option_next (option))
          {
            XkbOption *o;

            o = g_slice_new (XkbOption);
            o->id = g_strdup (rxkb_option_get_name (option));
            o->description = g_strdup(rxkb_option_get_description (option));
            g_hash_table_replace (g->options_table, o->id, o);
          }
    }

  rxkb_context_unref (ctx);

  return TRUE;
}

static void
parse_rules (GnomeXkbInfo *self)
{
  GnomeXkbInfoPrivate *priv = self->priv;
  GSettings *settings;
  gboolean show_all_sources;

  /* Make sure the translated strings we get from XKEYBOARD_CONFIG() are
   * in UTF-8 and not in the current locale */
  bind_textdomain_codeset ("xkeyboard-config", "UTF-8");

  /* Maps option group ids to XkbOptionGroup structs. Owns the
     XkbOptionGroup structs. */
  priv->option_groups_table = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                     NULL, free_option_group);
  /* Maps country strings to a GHashTable which is a set of Layout
     struct pointers into the Layout structs stored in
     layouts_table. */
  priv->layouts_by_country = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                    g_free, (GDestroyNotify) g_hash_table_destroy);
  /* Maps language strings to a GHashTable which is a set of Layout
     struct pointers into the Layout structs stored in
     layouts_table. */
  priv->layouts_by_language = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                     g_free, (GDestroyNotify) g_hash_table_destroy);
  /* Maps layout ids to Layout structs. Owns the Layout structs. */
  priv->layouts_table = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, free_layout);

  settings = g_settings_new ("org.gnome.desktop.input-sources");
  show_all_sources = g_settings_get_boolean (settings, "show-all-sources");
  g_object_unref (settings);

  if (!parse_rules_file (self, XKB_RULES_FILE, show_all_sources))
    {
      g_warning ("Failed to load '%s' XKB layouts", XKB_RULES_FILE);
      g_clear_pointer (&priv->option_groups_table, g_hash_table_destroy);
      g_clear_pointer (&priv->layouts_by_country, g_hash_table_destroy);
      g_clear_pointer (&priv->layouts_by_language, g_hash_table_destroy);
      g_clear_pointer (&priv->layouts_table, g_hash_table_destroy);
    }
}

static gboolean
ensure_rules_are_parsed (GnomeXkbInfo *self)
{
  GnomeXkbInfoPrivate *priv = self->priv;

  if (!priv->layouts_table)
    parse_rules (self);

  return !!priv->layouts_table;
}

static void
gnome_xkb_info_init (GnomeXkbInfo *self)
{
  self->priv = gnome_xkb_info_get_instance_private (self);
}

static void
gnome_xkb_info_finalize (GObject *self)
{
  GnomeXkbInfoPrivate *priv = GNOME_XKB_INFO (self)->priv;

  if (priv->option_groups_table)
    g_hash_table_destroy (priv->option_groups_table);
  if (priv->layouts_by_country)
    g_hash_table_destroy (priv->layouts_by_country);
  if (priv->layouts_by_language)
    g_hash_table_destroy (priv->layouts_by_language);
  if (priv->layouts_table)
    g_hash_table_destroy (priv->layouts_table);

  G_OBJECT_CLASS (gnome_xkb_info_parent_class)->finalize (self);
}

static void
gnome_xkb_info_class_init (GnomeXkbInfoClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = gnome_xkb_info_finalize;
}

/**
 * gnome_xkb_info_new:
 *
 * Returns: (transfer full): a new #GnomeXkbInfo instance.
 */
GnomeXkbInfo *
gnome_xkb_info_new (void)
{
  return g_object_new (GNOME_TYPE_XKB_INFO, NULL);
}

/**
 * gnome_xkb_info_get_all_layouts:
 * @self: a #GnomeXkbInfo
 *
 * Returns a list of all layout identifiers we know about.
 *
 * Return value: (transfer container) (element-type utf8): the list
 * of layout names. The caller takes ownership of the #GList but not
 * of the strings themselves, those are internally allocated and must
 * not be modified.
 *
 * Since: 3.6
 */
GList *
gnome_xkb_info_get_all_layouts (GnomeXkbInfo *self)
{
  GnomeXkbInfoPrivate *priv;

  g_return_val_if_fail (GNOME_IS_XKB_INFO (self), NULL);

  priv = self->priv;

  if (!ensure_rules_are_parsed (self))
    return NULL;

  return g_hash_table_get_keys (priv->layouts_table);
}

/**
 * gnome_xkb_info_get_all_option_groups:
 * @self: a #GnomeXkbInfo
 *
 * Returns a list of all option group identifiers we know about.
 *
 * Return value: (transfer container) (element-type utf8): the list
 * of option group ids. The caller takes ownership of the #GList but
 * not of the strings themselves, those are internally allocated and
 * must not be modified.
 *
 * Since: 3.6
 */
GList *
gnome_xkb_info_get_all_option_groups (GnomeXkbInfo *self)
{
  GnomeXkbInfoPrivate *priv;

  g_return_val_if_fail (GNOME_IS_XKB_INFO (self), NULL);

  priv = self->priv;

  if (!ensure_rules_are_parsed (self))
    return NULL;

  return g_hash_table_get_keys (priv->option_groups_table);
}

/**
 * gnome_xkb_info_description_for_group:
 * @self: a #GnomeXkbInfo
 * @group_id: identifier for group
 *
 * Return value: the translated description for the group @group_id.
 *
 * Since: 3.8
 */
const gchar *
gnome_xkb_info_description_for_group (GnomeXkbInfo *self,
                                      const gchar  *group_id)
{
  GnomeXkbInfoPrivate *priv;
  const XkbOptionGroup *group;

  g_return_val_if_fail (GNOME_IS_XKB_INFO (self), NULL);

  priv = self->priv;

  if (!ensure_rules_are_parsed (self))
    return NULL;

  group = g_hash_table_lookup (priv->option_groups_table, group_id);
  if (!group)
    return NULL;

  return XKEYBOARD_CONFIG_(group->description);
}

/**
 * gnome_xkb_info_get_options_for_group:
 * @self: a #GnomeXkbInfo
 * @group_id: group's identifier about which to retrieve the options
 *
 * Returns a list of all option identifiers we know about for group
 * @group_id.
 *
 * Return value: (transfer container) (element-type utf8): the list
 * of option ids. The caller takes ownership of the #GList but not of
 * the strings themselves, those are internally allocated and must not
 * be modified.
 *
 * Since: 3.6
 */
GList *
gnome_xkb_info_get_options_for_group (GnomeXkbInfo *self,
                                      const gchar  *group_id)
{
  GnomeXkbInfoPrivate *priv;
  const XkbOptionGroup *group;

  g_return_val_if_fail (GNOME_IS_XKB_INFO (self), NULL);

  priv = self->priv;

  if (!ensure_rules_are_parsed (self))
    return NULL;

  group = g_hash_table_lookup (priv->option_groups_table, group_id);
  if (!group)
    return NULL;

  return g_hash_table_get_keys (group->options_table);
}

/**
 * gnome_xkb_info_description_for_option:
 * @self: a #GnomeXkbInfo
 * @group_id: identifier for group containing the option
 * @id: option identifier
 *
 * Return value: the translated description for the option @id.
 *
 * Since: 3.6
 */
const gchar *
gnome_xkb_info_description_for_option (GnomeXkbInfo *self,
                                       const gchar  *group_id,
                                       const gchar  *id)
{
  GnomeXkbInfoPrivate *priv;
  const XkbOptionGroup *group;
  const XkbOption *option;

  g_return_val_if_fail (GNOME_IS_XKB_INFO (self), NULL);

  priv = self->priv;

  if (!ensure_rules_are_parsed (self))
    return NULL;

  group = g_hash_table_lookup (priv->option_groups_table, group_id);
  if (!group)
    return NULL;

  option = g_hash_table_lookup (group->options_table, id);
  if (!option)
    return NULL;

  return XKEYBOARD_CONFIG_(option->description);
}

/**
 * gnome_xkb_info_get_layout_info:
 * @self: a #GnomeXkbInfo
 * @id: layout's identifier about which to retrieve the info
 * @display_name: (out) (allow-none) (transfer none): location to store
 * the layout's display name, or %NULL
 * @short_name: (out) (allow-none) (transfer none): location to store
 * the layout's short name, or %NULL
 * @xkb_layout: (out) (allow-none) (transfer none): location to store
 * the layout's XKB name, or %NULL
 * @xkb_variant: (out) (allow-none) (transfer none): location to store
 * the layout's XKB variant, or %NULL
 *
 * Retrieves information about a layout. Both @display_name and
 * @short_name are suitable to show in UIs and might be localized if
 * translations are available.
 *
 * Some layouts don't provide a short name (2 or 3 letters) or don't
 * specify a XKB variant, in those cases @short_name or @xkb_variant
 * are empty strings, i.e. "".
 *
 * If the given layout doesn't exist the return value is %FALSE and
 * all the (out) parameters are set to %NULL.
 *
 * Return value: %TRUE if the layout exists or %FALSE otherwise.
 *
 * Since: 3.6
 */
gboolean
gnome_xkb_info_get_layout_info (GnomeXkbInfo *self,
                                const gchar  *id,
                                const gchar **display_name,
                                const gchar **short_name,
                                const gchar **xkb_layout,
                                const gchar **xkb_variant)
{
  GnomeXkbInfoPrivate *priv;
  const Layout *layout;

  if (display_name)
    *display_name = NULL;
  if (short_name)
    *short_name = NULL;
  if (xkb_layout)
    *xkb_layout = NULL;
  if (xkb_variant)
    *xkb_variant = NULL;

  g_return_val_if_fail (GNOME_IS_XKB_INFO (self), FALSE);

  priv = self->priv;

  if (!ensure_rules_are_parsed (self))
    return FALSE;

  if (!g_hash_table_lookup_extended (priv->layouts_table, id, NULL, (gpointer *)&layout))
    return FALSE;

  if (display_name)
    *display_name = XKEYBOARD_CONFIG_(layout->description);

  if (!layout->is_variant)
    {
      if (short_name)
        *short_name = XKEYBOARD_CONFIG_(layout->short_desc ? layout->short_desc : "");
      if (xkb_layout)
        *xkb_layout = layout->xkb_name;
      if (xkb_variant)
        *xkb_variant = "";
    }
  else
    {
      if (short_name)
        *short_name = XKEYBOARD_CONFIG_(layout->short_desc ? layout->short_desc :
                        layout->main_layout->short_desc ? layout->main_layout->short_desc : "");
      if (xkb_layout)
        *xkb_layout = layout->main_layout->xkb_name;
      if (xkb_variant)
        *xkb_variant = layout->xkb_name;
    }

  return TRUE;
}

static void
collect_layout_ids (gpointer key,
                    gpointer value,
                    gpointer data)
{
  Layout *layout = value;
  GList **list = data;

  *list = g_list_prepend (*list, layout->id);
}

/**
 * gnome_xkb_info_get_layouts_for_language:
 * @self: a #GnomeXkbInfo
 * @language_code: an ISO 639 code string
 *
 * Returns a list of all layout identifiers we know about for
 * @language_code.
 *
 * Return value: (transfer container) (element-type utf8): the list
 * of layout ids. The caller takes ownership of the #GList but not of
 * the strings themselves, those are internally allocated and must not
 * be modified.
 *
 * Since: 3.8
 */
GList *
gnome_xkb_info_get_layouts_for_language (GnomeXkbInfo *self,
                                         const gchar  *language_code)
{
  GnomeXkbInfoPrivate *priv;
  GHashTable *layouts_for_language;
  gchar *language;
  GList *list;

  g_return_val_if_fail (GNOME_IS_XKB_INFO (self), NULL);

  priv = self->priv;

  if (!ensure_rules_are_parsed (self))
    return NULL;

  language = gnome_get_language_from_code (language_code, NULL);
  if (!language)
    return NULL;

  layouts_for_language = g_hash_table_lookup (priv->layouts_by_language, language);
  g_free (language);

  if (!layouts_for_language)
    return NULL;

  list = NULL;
  g_hash_table_foreach (layouts_for_language, collect_layout_ids, &list);

  return list;
}

/**
 * gnome_xkb_info_get_layouts_for_country:
 * @self: a #GnomeXkbInfo
 * @country_code: an ISO 3166 code string
 *
 * Returns a list of all layout identifiers we know about for
 * @country_code.
 *
 * Return value: (transfer container) (element-type utf8): the list
 * of layout ids. The caller takes ownership of the #GList but not of
 * the strings themselves, those are internally allocated and must not
 * be modified.
 *
 * Since: 3.8
 */
GList *
gnome_xkb_info_get_layouts_for_country (GnomeXkbInfo *self,
                                        const gchar  *country_code)
{
  GnomeXkbInfoPrivate *priv;
  GHashTable *layouts_for_country;
  gchar *country;
  GList *list;

  g_return_val_if_fail (GNOME_IS_XKB_INFO (self), NULL);

  priv = self->priv;

  if (!ensure_rules_are_parsed (self))
    return NULL;

  country = gnome_get_country_from_code (country_code, NULL);
  if (!country)
    return NULL;

  layouts_for_country = g_hash_table_lookup (priv->layouts_by_country, country);
  g_free (country);

  if (!layouts_for_country)
    return NULL;

  list = NULL;
  g_hash_table_foreach (layouts_for_country, collect_layout_ids, &list);

  return list;
}

static void
collect_languages (gpointer value,
                   gpointer data)
{
  gchar *language = value;
  GList **list = data;

  *list = g_list_append (*list, language);
}

/**
 * gnome_xkb_info_get_languages_for_layout:
 * @self: a #GnomeXkbInfo
 * @layout_id: a layout identifier
 *
 * Returns a list of all languages supported by a layout, given by
 * @layout_id.
 *
 * Return value: (transfer container) (element-type utf8): the list of
 * ISO 639 code strings. The caller takes ownership of the #GList but
 * not of the strings themselves, those are internally allocated and
 * must not be modified.
 *
 * Since: 3.18
 */
GList *
gnome_xkb_info_get_languages_for_layout (GnomeXkbInfo *self,
                                         const gchar  *layout_id)
{
  GnomeXkbInfoPrivate *priv;
  Layout *layout;
  GList *list;

  g_return_val_if_fail (GNOME_IS_XKB_INFO (self), NULL);

  priv = self->priv;

  if (!ensure_rules_are_parsed (self))
    return NULL;

  layout = g_hash_table_lookup (priv->layouts_table, layout_id);

  if (!layout)
    return NULL;

  list = NULL;
  g_slist_foreach (layout->iso639Ids, collect_languages, &list);

  return list;
}
