#include "config.h"

#define DI_DEBUG 1

#include <limits.h>
#include <ctype.h>
#include <stdio.h>
#include <glib.h>
#include <sys/types.h>
#include <dirent.h>
#include "gnome-defs.h"
#include "gnome-portability.h"
#include "gnome-ditem.h"
#include "gnome-util.h"
#include "gnome-config.h"
#include "gnome-exec.h"

struct _GnomeDesktopItem {
  guchar refcount;

  GnomeDesktopItemFormat item_format : 4;
  GnomeDesktopItemFlags item_flags : 4;

  GHashTable *name; /* key is language, value is translated string */
  GHashTable *comment; /* key is language, value is translated string */

  int exec_length; /* Does not include NULL terminator in count */
  char **exec;

  char *icon_path;

  GHashTable *other_attributes; /* string key, string value */

  GSList *subitems; /* If GNOME_DESKTOP_ITEM_IS_DIRECTORY */
};

#ifdef DI_DEBUG
void
ditem_dump(const GnomeDesktopItem *ditem, int indent_level)
{
  int i;
  for(i = 0; i < indent_level; i++)
    g_print(" ");

  g_print("%s (%s)\n", ditem->name?g_hash_table_lookup(ditem->name, "C"):"ZOT", ditem->icon_path);
  g_slist_foreach(ditem->subitems, (GFunc)ditem_dump, GINT_TO_POINTER(indent_level + 3));
}
#endif

/**
 * gnome_desktop_item_new:
 *
 * Creates a GnomeDesktopItem object. The reference count on the returned value is set to '1'.
 *
 * Returns: The new GnomeDesktopItem
 */
GnomeDesktopItem *
gnome_desktop_item_new(void)
{
  GnomeDesktopItem *retval;

  retval = g_new0(GnomeDesktopItem, 1);

  retval->refcount++;

  return retval;
}

static void
ditem_copy_key_value(gpointer key, gpointer value, gpointer user_data)
{
  g_hash_table_insert(user_data, g_strdup(key), g_strdup(value));
}

/**
 * gnome_desktop_item_copy:
 * @item: The item to be copied
 *
 * Creates a copy of a GnomeDesktopItem
 *
 * Returns: The new copy 
 */
GnomeDesktopItem *
gnome_desktop_item_copy (const GnomeDesktopItem *item)
{
  GnomeDesktopItem *retval;

  g_return_val_if_fail(item, NULL);

  retval = gnome_desktop_item_new();

  retval->item_format = item->item_format;
  retval->item_flags = item->item_flags;
  retval->icon_path = g_strdup(item->icon_path);
  retval->exec = g_copy_vector((const char **)item->exec);
  for(retval->exec_length = 0; retval->exec[retval->exec_length]; retval->exec_length++) /* just count */;

  if(item->name
     && g_hash_table_size(item->name) > 0)
    {
      retval->name = g_hash_table_new (g_str_hash, g_str_equal);
      g_hash_table_foreach(item->name, ditem_copy_key_value, retval->name);
      
    }

  if(item->comment
     && g_hash_table_size(item->comment) > 0)
    {
      retval->comment = g_hash_table_new (g_str_hash, g_str_equal);
      g_hash_table_foreach(item->comment, ditem_copy_key_value, retval->comment);
      
    }

  if(item->other_attributes
     && g_hash_table_size(item->other_attributes) > 0)
    {
      retval->other_attributes = g_hash_table_new (g_str_hash, g_str_equal);
      g_hash_table_foreach(item->other_attributes, ditem_copy_key_value, retval->other_attributes);
      
    }

  retval->subitems = g_slist_copy(item->subitems);
  g_slist_foreach(retval->subitems, (GFunc)gnome_desktop_item_ref, NULL);

  return retval;
}

/* If you find a bug here, odds are that it exists in ditem_gnome_load() too */
static GnomeDesktopItem *
ditem_kde_load (const char *file, const char *data_file, GnomeDesktopItemLoadFlags flags,
		GnomeDesktopItemFlags item_flags, char ***sub_sort_order)
{
  GnomeDesktopItem *retval = gnome_desktop_item_new();
  char confpath[PATH_MAX];
  char *key, *value;
  void *iter;

  g_snprintf(confpath, sizeof(confpath), "=%s=/KDE Desktop Entry", data_file);

  retval->item_flags = item_flags;
  retval->name = g_hash_table_new(g_str_hash, g_str_equal);
  retval->comment = g_hash_table_new(g_str_hash, g_str_equal);
  retval->other_attributes = g_hash_table_new(g_str_hash, g_str_equal);

  iter = gnome_config_init_iterator(confpath);
  while((iter = gnome_config_iterator_next(iter, &key, &value)))
    {
      if(!*key || !*value)
	{
	  g_free(key);
	  g_free(value);
	  continue;
	}

      if(!strcmp(key, "Name")) {
	g_hash_table_insert(retval->name, g_strdup("C"), value);
      } else if(!strncmp(key, "Name[", 5)) {
	char *mylang, *ctmp;

	mylang = key + 5;
	ctmp = strchr(mylang, ']');
	if(ctmp) *ctmp = '\0';

	g_hash_table_insert(retval->name, g_strdup(mylang), value);
	g_free(key);
      } else if(!strcmp(key, "Comment")) {
	g_hash_table_insert(retval->comment, g_strdup("C"), value);
      } else if(!strncmp(key, "Comment[", strlen("Comment["))) {
	char *mylang, *ctmp;

	mylang = key + strlen("Comment[");
	ctmp = strchr(mylang, ']');
	if(ctmp) *ctmp = '\0';

	g_hash_table_insert(retval->name, g_strdup(mylang), value);
	g_free(key);
      } else if(!strcmp(key, "Icon")) {
	retval->icon_path = value;
	g_free(key);
      } else if(!strcmp(key, "Terminal")) {
	if(tolower(*key) == 'y' || tolower(*key) == 't' || atoi(value))
	  retval->item_flags |= GNOME_DESKTOP_ITEM_RUN_IN_TERMINAL;
	else
	  retval->item_flags &= (~GNOME_DESKTOP_ITEM_RUN_IN_TERMINAL);

	g_free(key);
	g_free(value);
      } else if(!strcmp(key, "Exec")) {
	gnome_config_make_vector(value, &retval->exec_length, &retval->exec);

	retval->exec = g_realloc(retval->exec, (retval->exec_length + 1) * sizeof(char *));
	retval->exec[retval->exec_length] = NULL;

	g_free(key);
	g_free(value);
      } else if(!strcmp(key, "SortOrder")) {
	*sub_sort_order = g_strsplit(value, ",", -1);
	g_free(key);
	g_free(value);
      } else {
	g_hash_table_insert(retval->other_attributes, key, value);
      }
    }

  if ((flags & GNOME_DESKTOP_ITEM_LOAD_ONLY_IF_EXISTS)
      && !(retval->item_flags & GNOME_DESKTOP_ITEM_IS_DIRECTORY))
    {
      char *tryme;
      /* We don't use gnome_desktop_item_exists() here because it is more thorough than the TryExec stuff
	 which GNOME_DESKTOP_ITEM_LOAD_ONLY_IF_EXISTS specifies */

      if(!retval->exec)
	goto errout;

      tryme = g_hash_table_lookup(retval->other_attributes, "TryExec");

      if(tryme && !gnome_is_program_in_path(tryme))
	goto errout;
    }

  return retval;

 errout:
  gnome_desktop_item_unref(retval);
  return NULL;
}

/* There is entirely too much duplication between this function and ditem_kde_load() */
static GnomeDesktopItem *
ditem_gnome_load (const char *file, const char *data_file, GnomeDesktopItemLoadFlags flags,
		  GnomeDesktopItemFlags item_flags, char ***sub_sort_order)
{
  GnomeDesktopItem *retval = gnome_desktop_item_new();
  char confpath[PATH_MAX];
  char *key, *value;
  void *iter;

  g_snprintf(confpath, sizeof(confpath), "=%s=/Desktop Entry", data_file);

  retval->item_flags = item_flags;
  retval->name = g_hash_table_new(g_str_hash, g_str_equal);
  retval->comment = g_hash_table_new(g_str_hash, g_str_equal);
  retval->other_attributes = g_hash_table_new(g_str_hash, g_str_equal);

  iter = gnome_config_init_iterator(confpath);
  while((iter = gnome_config_iterator_next(iter, &key, &value)))
    {
      if(!*key || !*value)
	{
	  g_free(key);
	  g_free(value);
	  continue;
	}

      if(!strcmp(key, "Name")) {
	g_hash_table_insert(retval->name, g_strdup("C"), value);
      } else if(!strncmp(key, "Name[", 5)) {
	char *mylang, *ctmp;

	mylang = key + 5;
	ctmp = strchr(mylang, ']');
	if(ctmp) *ctmp = '\0';

	g_hash_table_insert(retval->name, g_strdup(mylang), value);
	g_free(key);
      } else if(!strcmp(key, "Comment")) {
	g_hash_table_insert(retval->comment, g_strdup("C"), value);
      } else if(!strncmp(key, "Comment[", strlen("Comment["))) {
	char *mylang, *ctmp;

	mylang = key + strlen("Comment[");
	ctmp = strchr(mylang, ']');
	if(ctmp) *ctmp = '\0';

	g_hash_table_insert(retval->name, g_strdup(mylang), value);
	g_free(key);
      } else if(!strcmp(key, "Icon")) {
	retval->icon_path = value;
	g_free(key);
      } else if(!strcmp(key, "Terminal")) {
	if(tolower(*key) == 'y' || tolower(*key) == 't' || atoi(value))
	  retval->item_flags |= GNOME_DESKTOP_ITEM_RUN_IN_TERMINAL;
	else
	  retval->item_flags &= (~GNOME_DESKTOP_ITEM_RUN_IN_TERMINAL);

	g_free(key);
	g_free(value);
      } else if(!strcmp(key, "Exec")) {
	gnome_config_make_vector(value, &retval->exec_length, &retval->exec);

	retval->exec = g_realloc(retval->exec, (retval->exec_length + 1) * sizeof(char *));
	retval->exec[retval->exec_length] = NULL;

	g_free(key);
	g_free(value);
      } else {
	g_hash_table_insert(retval->other_attributes, key, value);
      }
    }

  if ((flags & GNOME_DESKTOP_ITEM_LOAD_ONLY_IF_EXISTS)
      && !(retval->item_flags & GNOME_DESKTOP_ITEM_IS_DIRECTORY))
    {
      char *tryme;

      if(!retval->exec)
	goto errout;

      tryme = g_hash_table_lookup(retval->other_attributes, "TryExec");

      if(tryme && !gnome_is_program_in_path(tryme))
	goto errout;
    }

  /* Read the .order file */
  if(retval->item_flags & GNOME_DESKTOP_ITEM_IS_DIRECTORY)
    {
      FILE *fh;

      g_snprintf(confpath, sizeof(confpath), "%s/.order", file);

      fh = fopen(file, "r");

      if(fh)
	{
	  char filebuf[8192]; /* Eek, a hardcoded limit! */
	  int nr;

	  nr = fread(filebuf, 1, sizeof(filebuf), fh);
	  if(nr > 0)
	    {
	      filebuf[nr - 1] = '\0';
	      g_strstrip(filebuf);

	      *sub_sort_order = g_strsplit(filebuf, "\n", -1);
	    }

	  fclose(fh);
	}
    }

  return retval;

 errout:
  gnome_desktop_item_unref(retval);
  return NULL;
}

static gboolean
string_in_array(const char *str, char **array)
{
  int i;

  if(!array)
    return FALSE;

  for(i = 0; array[i]; i++)
    {
      if(!strcmp(str, array[i]))
	return TRUE;
    }

  return FALSE;
}

/**
 * gnome_desktop_item_new_from_file:
 * @file: The filename or directory path to load the GnomeDesktopItem from
 * @flags: Flags to influence the loading process
 *
 * This function loads 'file' and turns it into a GnomeDesktopItem.
 * If 'file' is a directory, it loads all the items under that
 * directory as subitems of the directory's GnomeDesktopItem.
 *
 * Returns: The newly loaded item.
 */
GnomeDesktopItem *
gnome_desktop_item_new_from_file (const char *file, GnomeDesktopItemLoadFlags flags)
{
  GnomeDesktopItem *retval;
  char subfn[PATH_MAX], headbuf[256], confpath[PATH_MAX];
  FILE *fh;
  GnomeDesktopItemFlags item_flags;
  GnomeDesktopItemFormat fmt;
  char **sub_sort_order;

  g_return_val_if_fail (file, NULL);

#ifdef DI_DEBUG
  //  g_print("Loading file %s\n", file);
#endif

  if (!g_file_exists (file))
    return NULL;

  /* Step one - figure out what type of file this is */
  flags = 0;
  fmt = GNOME_DESKTOP_ITEM_UNKNOWN;
  if (g_file_test (file, G_FILE_TEST_ISDIR))
    {
      item_flags |= GNOME_DESKTOP_ITEM_IS_DIRECTORY;
      g_snprintf(subfn, sizeof(subfn), "%s/.directory", file);
    }
  else
    {
      strcpy(subfn, file);
    }

  fh = fopen(subfn, "r");

  if(!fh && (item_flags & GNOME_DESKTOP_ITEM_IS_DIRECTORY))
    fmt = GNOME_DESKTOP_ITEM_GNOME; /* Empty dir becomes a GNOME thingie */
  else if(fh)
    {
      fgets(headbuf, sizeof(headbuf), fh);

      if(!strncmp(headbuf, "[Desktop Entry]", strlen("[Desktop Entry]")))
	{
	  fmt = GNOME_DESKTOP_ITEM_GNOME;
	}
      else if(!strncmp(headbuf, "[KDE Desktop Entry]", strlen("[KDE Desktop Entry]")))
	{
	  fmt = GNOME_DESKTOP_ITEM_KDE;
	}

      fclose(fh);
    }

  sub_sort_order = NULL;
  switch (fmt) {
  case GNOME_DESKTOP_ITEM_KDE:
    retval = ditem_kde_load(file, subfn, flags, item_flags, &sub_sort_order);
    break;
  case GNOME_DESKTOP_ITEM_GNOME:
    retval = ditem_gnome_load(file, subfn, flags, item_flags, &sub_sort_order);
    break;
  default:
    g_warning("Unknown desktop file format %d", fmt);
    return NULL;
    break;
  }

  if(!retval)
    goto out;

  retval->item_format = fmt;

  if(!g_hash_table_size(retval->name))
    {
      g_hash_table_destroy(retval->name);
      retval->name = NULL;
    }
  if(!g_hash_table_size(retval->comment))
    {
      g_hash_table_destroy(retval->comment);
      retval->comment = NULL;
    }
  if(!g_hash_table_size(retval->other_attributes))
    {
      g_hash_table_destroy(retval->other_attributes);
      retval->other_attributes = NULL;
    }

  /* Now, read the subdirectory (if appropriate) */
  if(retval->item_flags & GNOME_DESKTOP_ITEM_IS_DIRECTORY)
    {
      int i;
      DIR *dirh;

      if(sub_sort_order)
	{
	  for(i = 0; sub_sort_order[i]; i++)
	    {
	      GnomeDesktopItem *subitem;

	      g_snprintf(confpath, sizeof(confpath), "%s/%s", file, sub_sort_order[i]);

	      subitem = gnome_desktop_item_new_from_file(confpath, flags|GNOME_DESKTOP_ITEM_LOAD_NO_SYNC);

	      if(subitem)
		retval->subitems = g_slist_append(retval->subitems, subitem);
	    }
	}

      dirh = opendir(file);

      if(dirh)
	{
	  struct dirent *dent;

	  while((dent = readdir(dirh)))
	    {
	      GnomeDesktopItem *subitem;

	      if(dent->d_name[0] == '.')
		continue;

	      if(string_in_array(dent->d_name, sub_sort_order))
		continue;

	      g_snprintf(confpath, sizeof(confpath), "%s/%s", file, dent->d_name);

	      subitem = gnome_desktop_item_new_from_file(confpath, flags|GNOME_DESKTOP_ITEM_LOAD_NO_SYNC);

	      if(subitem)
		retval->subitems = g_slist_append(retval->subitems, subitem);
	    }

	  closedir(dirh);
	}
    }

 out:
  if(!(flags & GNOME_DESKTOP_ITEM_LOAD_NO_SYNC))
    gnome_config_sync();

  return retval;
}

/**
 * gnome_desktop_item_ref:
 * @item: A desktop item
 *
 * Increases the reference count of the specified item.
 */
void
gnome_desktop_item_ref (GnomeDesktopItem *item)
{
  g_return_if_fail(item);

  item->refcount++;
}

static gboolean
ditem_free_key_value(gpointer key, gpointer value, gpointer user_data)
{
  g_free(key);
  g_free(value);
  return TRUE;
}

/**
 * gnome_desktop_item_ref:
 * @item: A desktop item
 *
 * Decreases the reference count of the specified item, and destroys the item if there are no more references left.
 */
void
gnome_desktop_item_unref (GnomeDesktopItem *item)
{
  g_return_if_fail(item);

  item->refcount--;

  if(item->refcount != 0)
    return;

  g_strfreev(item->exec);
  g_free(item->icon_path);

  if(item->name)
    {
      g_hash_table_freeze(item->name);
      g_hash_table_foreach_remove(item->name, ditem_free_key_value, NULL);
      g_hash_table_destroy(item->name);
    }

  if(item->comment)
    {
      g_hash_table_freeze(item->comment);
      g_hash_table_foreach_remove(item->comment, ditem_free_key_value, NULL);
      g_hash_table_destroy(item->comment);
    }

  if(item->other_attributes)
    {
      g_hash_table_freeze(item->other_attributes);
      g_hash_table_foreach_remove(item->other_attributes, ditem_free_key_value, NULL);
      g_hash_table_destroy(item->other_attributes);
    }

  g_slist_foreach(item->subitems, (GFunc)gnome_desktop_item_unref, NULL);
  g_slist_free(item->subitems);

  g_free(item);
}

/**
 * gnome_desktop_item_launch:
 * @item: A desktop item
 * @argc: An optional count of arguments to be added to the arguments defined.
 * @argv: An optional argument array, of length 'argc', to be appended
 *        to the command arguments specified in 'item'. Can be NULL.
 *
 * This function runs the program listed in the specified 'item', optionally appending additional arguments
 * to its command line.
 *
 * Returns: The value returned by gnome_execute_async() upon execution of the specified item.  */
int
gnome_desktop_item_launch (const GnomeDesktopItem *item, int argc, const char **argv)
{
  char **real_argv;
  int real_argc;
  int i, j;

  g_return_val_if_fail(item, -1);
  g_return_val_if_fail(item->exec, -1);

  if(!argv)
    argc = 0;

  real_argc = argc + item->exec_length;
  real_argv = g_alloca((real_argc + 1) * sizeof(char *));

  for(i = 0; i < item->exec_length; i++)
    real_argv[i] = item->exec[i];

  for(j = 0; j < argc; j++, i++)
    real_argv[i] = (char *)argv[j];

  return gnome_execute_async(NULL, real_argc, real_argv);
}

/**
 * gnome_desktop_item_exists:
 * @item: A desktop item
 *
 * Returns: Whether the program specified by 'item' is available to run on the system.
 */
gboolean
gnome_desktop_item_exists (const GnomeDesktopItem *item)
{
  g_return_val_if_fail(item, FALSE);

  if(item->other_attributes)
    {
      char *tryme;

      tryme = g_hash_table_lookup(item->other_attributes, "TryExec");
      if(tryme)
	{
	  tryme = gnome_is_program_in_path(tryme);
	  if(tryme)
	    {
	      g_free(tryme);
	      return TRUE;
	    }
	  else
	    return FALSE;
	}
    }

  if(item->exec_length && item->exec)
    {
      if(item->exec[0][0] == PATH_SEP)
	return g_file_exists(item->exec[0]);
      else
	{
	  char *tryme = gnome_is_program_in_path(item->exec[0]);
	  if(tryme)
	    {
	      g_free(tryme);
	      return TRUE;
	    }
	  else
	    return FALSE;
	}
    }

  return TRUE;
}

/**
 * gnome_desktop_item_get_flags:
 * @item: A desktop item
 *
 * Returns: The flags associated with the specified 'item'
 */
GnomeDesktopItemFlags
gnome_desktop_item_get_flags (const GnomeDesktopItem *item)
{
  g_return_val_if_fail(item, 0);

  return item->item_flags;
}

/**
 * gnome_desktop_item_get_command:
 * @item: A desktop item
 * @argc: An optional pointer through which the number of arguments included in the command will be returned. May be NULL.
 *
 * Returns: The command associated with the specified 'item'. The returned memory remains owned by the GnomeDesktopItem
 *          and should not be freed.
 */
const char **
gnome_desktop_item_get_command (const GnomeDesktopItem *item, int *argc)
{
  g_return_val_if_fail(item, NULL);

  if(argc)
    *argc = item->exec_length;

  return (const char **)item->exec;
}

/**
 * gnome_desktop_item_get_icon_path:
 * @item: A desktop item
 *
 * Returns: The icon filename associated with the specified 'item'. The returned memory remains owned by the GnomeDesktopItem
 *          and should not be freed.
 */
const char *
gnome_desktop_item_get_icon_path (const GnomeDesktopItem *item)
{
  g_return_val_if_fail(item, NULL);

  return item->icon_path;
}

/**
 * gnome_desktop_item_get_name:
 * @item: A desktop item
 * @language: The language translation for which the name should be returned. If NULL is passed, it defaults to "C".
 *
 * Returns: The human-readable name for the specified 'item', in the
 *          specified 'language'. The returned memory remains owned by
 *          the GnomeDesktopItem and should not be freed.
 * */
const char *
gnome_desktop_item_get_name (const GnomeDesktopItem *item, const char *language)
{
  g_return_val_if_fail(item, NULL);

  if(item->name)
    return g_hash_table_lookup(item->name, language?language:"C");
  else
    return NULL;
}

/**
 * gnome_desktop_item_get_comment:
 * @item: A desktop item
 * @language: The language translation for which the comment should be returned. If NULL is passed, it defaults to "C".
 *
 * Returns: The human-readable comment for the specified 'item', in the
 *          specified 'language'. The returned memory remains owned by
 *          the GnomeDesktopItem and should not be freed.
 *
 */
const char *
gnome_desktop_item_get_comment (const GnomeDesktopItem *item, const char *language)
{
  g_return_val_if_fail(item, NULL);

  if(item->comment)
    return g_hash_table_lookup(item->comment, language?language:"C");
  else
    return NULL;
}

/**
 * gnome_desktop_item_get_attribute:
 * @item: A desktop item
 * @attr_name: The language translation for which the comment should be returned.
 *
 * Returns: The value of the attribute 'attr_name' on the 'item'. The
 *          returned memory remains owned by the GnomeDesktopItem and
 *          should * not be freed.
 *
 */
const char *
gnome_desktop_item_get_attribute (const GnomeDesktopItem *item, const char *attr_name)
{
  g_return_val_if_fail(item, NULL);
  g_return_val_if_fail(attr_name, NULL);

  if(item->other_attributes)
    return g_hash_table_lookup(item->other_attributes, attr_name);
  else
    return NULL;
}

/**
 * gnome_desktop_item_get_subitems:
 * @item: A desktop item
 *
 * Returns: A GSList of the items contained under 'item'. The returned value should not be freed.
 *
 */
const GSList *
gnome_desktop_item_get_subitems (const GnomeDesktopItem *item)
{
  g_return_val_if_fail(item, NULL);

  return item->subitems;
}

static void
ditem_add_key(gpointer key, gpointer value, gpointer user_data)
{
  GSList **list = user_data;

  *list = g_slist_prepend(*list, key);
}

/**
 * gnome_desktop_item_get_languages:
 * @item: A desktop item
 *
 * Returns: A GSList of the language name strings for the languages
 * 'item'. The returned value should be freed, but its contents should
 * not be.
 *
 */
GSList *
gnome_desktop_item_get_languages(const GnomeDesktopItem *item)
{
  GSList *retval = NULL;

  g_return_val_if_fail(item, NULL);

  if(item->name)
    {
      g_hash_table_foreach(item->name, ditem_add_key, &retval);
    }

  return retval;
}

/**
 * gnome_desktop_item_get_attributes:
 * @item: A desktop item
 *
 * Returns: A GSList of the attribute name strings for the attributes of
 * 'item'. The returned value should be freed, but its contents should
 * not be.
 *
 */
GSList *
gnome_desktop_item_get_attributes(const GnomeDesktopItem *item)
{
  GSList *retval = NULL;

  g_return_val_if_fail(item, NULL);

  if(item->other_attributes)
    {
      g_hash_table_foreach(item->other_attributes, ditem_add_key, &retval);
    }

  return retval;
}

/**
 * gnome_desktop_item_get_format:
 * @item: A desktop item
 *
 * Returns: The format that the specified 'item' is stored on disk with
 */
GnomeDesktopItemFormat
gnome_desktop_item_get_format (const GnomeDesktopItem *item)
{
  g_return_val_if_fail(item, GNOME_DESKTOP_ITEM_UNKNOWN);

  return item->item_format;
}

/******* Set... ******/

/**
 * gnome_desktop_item_set_name:
 * @item: A desktop item
 * @language: The language for which to set the item name
 * @name: The item's name in the specified 'language'
 *
 */
void
gnome_desktop_item_set_name (GnomeDesktopItem *item, const char *language, const char *name)
{
  gpointer old_val = NULL, old_key = NULL;

  g_return_if_fail(item);

  if(!item->name)
    item->name = g_hash_table_new(g_str_hash, g_str_equal);
  else
    {
      g_hash_table_lookup_extended(item->name, language, &old_key, &old_val);
    }

  g_hash_table_insert(item->name, g_strdup(language), g_strdup(name));

  g_free(old_key);
  g_free(old_val);
}

/**
 * gnome_desktop_item_set_command:
 * @item: A desktop item
 * @command: A NULL-terminated array of strings that specifies the command line associated with this item.
 *
 */
void
gnome_desktop_item_set_command (GnomeDesktopItem *item, const char **command)
{
  g_return_if_fail(item);

  g_strfreev(item->exec);
  if(command)
    {
      item->exec = g_copy_vector(command);
      for(item->exec_length = 0; item->exec[item->exec_length]; item->exec_length++) /* just count */ ;
    }
  else
    {
      item->exec = NULL;
      item->exec_length = 0;
    }
}

/**
 * gnome_desktop_item_set_icon_path:
 * @item: A desktop item
 * @icon_path: A string specifying the path to the icon file for this item.
 *
 */
void
gnome_desktop_item_set_icon_path (GnomeDesktopItem *item, const char *icon_path)
{
  g_return_if_fail(item);

  g_free(item->icon_path);
  if(icon_path)
    item->icon_path = g_strdup(icon_path);
  else
    item->icon_path = NULL;
}

/**
 * gnome_desktop_item_set_comment:
 * @item: A desktop item
 * @language: The language for which to set the item comment
 * @comment: The item's comment in the specified 'language'
 *
 */
void
gnome_desktop_item_set_comment (GnomeDesktopItem *item, const char *language, const char *comment)
{
  gpointer old_val = NULL, old_key = NULL;
  g_return_if_fail(item);

  if(!item->comment)
    item->comment = g_hash_table_new(g_str_hash, g_str_equal);
  else
    {
      g_hash_table_lookup_extended(item->comment, language, &old_key, &old_val);
    }

  g_hash_table_insert(item->comment, g_strdup(language), g_strdup(comment));

  g_free(old_key);
  g_free(old_val);
}

/**
 * gnome_desktop_item_set_attribute:
 * @item: A desktop item
 * @attr_name: The name of the attribute
 * @attr_value: The value of the attribute
 *
 */
void
gnome_desktop_item_set_attribute (GnomeDesktopItem *item, const char *attr_name, const char *attr_value)
{
  gpointer old_val = NULL, old_key = NULL;
  g_return_if_fail(item);

  if(!item->other_attributes)
    item->other_attributes = g_hash_table_new(g_str_hash, g_str_equal);
  else
    {
      g_hash_table_lookup_extended(item->other_attributes, attr_name, &old_key, &old_val);
    }

  g_hash_table_insert(item->other_attributes, g_strdup(attr_name), g_strdup(attr_value));

  g_free(old_key);
  g_free(old_val);
}

/**
 * gnome_desktop_item_set_subitems:
 * @item: A desktop item for a directory
 * @subitems: A GSList of the items to be listed as children of this item. Ownership the 'subitems' list is transferred
 *            from the application to the 'item'.
 */
void
gnome_desktop_item_set_subitems (GnomeDesktopItem *item, GSList *subitems)
{
  g_return_if_fail(item);
  g_return_if_fail(item->item_flags & GNOME_DESKTOP_ITEM_IS_DIRECTORY);

  g_slist_foreach(subitems, (GFunc)gnome_desktop_item_ref, NULL);
  g_slist_foreach(item->subitems, (GFunc)gnome_desktop_item_unref, NULL);
  g_slist_free(item->subitems);
  item->subitems = subitems;
}

/**
 * gnome_desktop_item_set_flags:
 * @item: A desktop item
 * @flags: The flags to be set
 */
void
gnome_desktop_item_set_flags (GnomeDesktopItem *item, GnomeDesktopItemFlags flags)
{
  g_return_if_fail(item);

  item->item_flags = flags;
}

/**
 * gnome_desktop_item_set_format:
 * @item: A desktop item
 * @fmt: The format to be set
 */
void
gnome_desktop_item_set_format (GnomeDesktopItem *item, GnomeDesktopItemFormat fmt)
{
  g_return_if_fail(item);

  item->item_format = fmt;
}
