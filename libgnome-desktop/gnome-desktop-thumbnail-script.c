/*
 * Copyright (C) 2002, 2017 Red Hat, Inc.
 * Copyright (C) 2010 Carlos Garcia Campos <carlosgc@gnome.org>
 *
 * This file is part of the Gnome Library.
 *
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Authors: Alexander Larsson <alexl@redhat.com>
 *          Carlos Garcia Campos <carlosgc@gnome.org>
 *          Bastien Nocera <hadess@hadess.net>
 */

#include "config.h"

#include <gio/gio.h>
#include <glib/gstdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "gnome-desktop-thumbnail-script.h"

typedef struct {
  char *infile;
  char *outfile;
} ScriptExec;

static char *
expand_thumbnailing_elem (const char *elem,
			  const int   size,
			  const char *inuri,
			  const char *outfile,
			  gboolean   *got_input,
			  gboolean   *got_output)
{
  GString *str;
  const char *p, *last;
  char *localfile;

  str = g_string_new (NULL);

  last = elem;
  while ((p = strchr (last, '%')) != NULL)
    {
      g_string_append_len (str, last, p - last);
      p++;

      switch (*p) {
      case 'u':
	g_string_append (str, inuri);
	*got_input = TRUE;
	p++;
	break;
      case 'i':
	localfile = g_filename_from_uri (inuri, NULL, NULL);
	if (localfile)
	  {
	    g_string_append (str, localfile);
	    *got_input = TRUE;
	    g_free (localfile);
	  }
	p++;
	break;
      case 'o':
	g_string_append (str, outfile);
	*got_output = TRUE;
	p++;
	break;
      case 's':
	g_string_append_printf (str, "%d", size);
	p++;
	break;
      case '%':
	g_string_append_c (str, '%');
	p++;
	break;
      case 0:
      default:
	break;
      }
      last = p;
    }
  g_string_append (str, last);

  return g_string_free (str, FALSE);
}

static char **
expand_thumbnailing_script (const char  *cmd,
			    ScriptExec  *script,
			    int          size,
			    GError     **error)
{
  GPtrArray *array;
  g_auto(GStrv) cmd_elems = NULL;
  guint i;
  gboolean got_in, got_out;
  g_autofree char *sandboxed_path = NULL;

  if (!g_shell_parse_argv (cmd, NULL, &cmd_elems, error))
    return NULL;

  array = g_ptr_array_new_with_free_func (g_free);

  got_in = got_out = FALSE;
  for (i = 0; cmd_elems[i] != NULL; i++)
    {
      char *expanded;

      expanded = expand_thumbnailing_elem (cmd_elems[i],
					   size,
					   script->infile,
					   script->outfile,
					   &got_in,
					   &got_out);

      g_ptr_array_add (array, expanded);
    }

  if (!got_in)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
			   "Input file could not be set");
      goto bail;
    }
  else if (!got_out)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
			   "Output file could not be set");
      goto bail;
    }

  g_ptr_array_add (array, NULL);

  return (char **) g_ptr_array_free (array, FALSE);

bail:
  g_ptr_array_free (array, TRUE);
  return NULL;
}

static void
script_exec_free (ScriptExec *exec)
{
  g_free (exec->infile);
  if (exec->outfile)
    {
      g_unlink (exec->outfile);
      g_free (exec->outfile);
    }
  g_free (exec);
}

static ScriptExec *
script_exec_new (const char *uri)
{
  ScriptExec *exec;
  g_autoptr(GFile) file = NULL;
  int fd;
  g_autofree char *tmpname = NULL;

  exec = g_new0 (ScriptExec, 1);
  file = g_file_new_for_uri (uri);

  exec->infile = g_file_get_path (file);
  if (!exec->infile)
    goto bail;

  fd = g_file_open_tmp (".gnome_desktop_thumbnail.XXXXXX", &tmpname, NULL);
  if (fd == -1)
    goto bail;
  close (fd);
  exec->outfile = g_steal_pointer (&tmpname);

  return exec;

bail:
  script_exec_free (exec);
  return NULL;
}

GBytes *
gnome_desktop_thumbnail_script_exec (const char  *cmd,
				     int          size,
				     const char  *uri,
				     GError     **error)
{
  g_autofree char *error_out = NULL;
  g_auto(GStrv) expanded_script = NULL;
  int exit_status;
  gboolean ret;
  GBytes *image = NULL;
  ScriptExec *exec;

  exec = script_exec_new (uri);
  expanded_script = expand_thumbnailing_script (cmd, exec, size, error);
  if (expanded_script == NULL)
    goto out;

#if 0
  guint i;

  g_print ("About to launch script: ");
  for (i = 0; expanded_script[i]; i++)
    g_print ("%s ", expanded_script[i]);
  g_print ("\n");
#endif

  ret = g_spawn_sync (NULL, expanded_script, NULL, G_SPAWN_SEARCH_PATH,
		      NULL, NULL, NULL, &error_out,
		      &exit_status, error);
  if (ret && g_spawn_check_exit_status (exit_status, error))
    {
      char *contents;
      gsize length;

      if (g_file_get_contents (exec->outfile, &contents, &length, error))
        image = g_bytes_new_take (contents, length);
    }
  else
    {
      g_debug ("Failed to launch script: %s", !ret ? (*error)->message : error_out);
    }

out:
  script_exec_free (exec);
  return image;
}

