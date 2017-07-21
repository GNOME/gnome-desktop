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
  gboolean sandbox;
  GArray *fd_array;
  /* Input/output file paths outside the sandbox */
  char *infile;
  char *outfile;
  char *outdir; /* outdir is outfile's parent dir, if it needs to be deleted */
  /* I/O file paths inside the sandbox */
  char *s_infile;
  char *s_outfile;
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

/* From https://github.com/flatpak/flatpak/blob/master/common/flatpak-run.c */
G_GNUC_NULL_TERMINATED
static void
add_args (GPtrArray *argv_array, ...)
{
  va_list args;
  const gchar *arg;

  va_start (args, argv_array);
  while ((arg = va_arg (args, const gchar *)))
    g_ptr_array_add (argv_array, g_strdup (arg));
  va_end (args);
}

static char *
get_extension (const char *path)
{
  g_autofree char *basename = NULL;
  char *p;

  basename = g_path_get_basename (path);
  p = strrchr (basename, '.');
  if (p == NULL)
    return NULL;
  return g_strdup (p + 1);
}

#ifdef HAVE_BWRAP
static gboolean
add_bwrap (GPtrArray   *array,
	   ScriptExec  *script)
{
  char *fd_str;
  int fd;

  g_return_val_if_fail (script->outdir != NULL, FALSE);
  g_return_val_if_fail (script->s_infile != NULL, FALSE);

  add_args (array,
	    "bwrap",
	    "--ro-bind", "/usr", "/usr",
	    "--proc", "/proc",
	    "--dev", "/dev",
	    "--symlink", "usr/lib", "/lib",
	    "--symlink", "usr/lib64", "/lib64",
	    "--symlink", "usr/bin", "/bin",
	    "--symlink", "usr/sbin", "/sbin",
	    "--chdir", "/",
	    "--unshare-all",
	    "--die-with-parent",
	    NULL);

  /* Add gnome-desktop's install prefix if needed */
  if (g_strcmp0 (INSTALL_PREFIX, "") != 0 &&
      g_strcmp0 (INSTALL_PREFIX, "/usr") != 0 &&
      g_strcmp0 (INSTALL_PREFIX, "/usr/") != 0)
    {
      add_args (array,
                "--ro-bind", INSTALL_PREFIX, INSTALL_PREFIX,
                NULL);
    }

  g_ptr_array_add (array, g_strdup ("--bind"));
  g_ptr_array_add (array, g_strdup (script->outdir));
  g_ptr_array_add (array, g_strdup ("/tmp"));

  /* Open the infile so we can pass the fd through bwrap,
   * we make sure to also re-use the original file's original
   * extension in case it's useful for the thumbnailer to
   * identify the file type */
  fd = open (script->infile, O_RDONLY | O_CLOEXEC);
  if (fd == -1)
    goto bail;
  fd_str = g_strdup_printf ("%d", fd);
  g_ptr_array_add (array, g_strdup ("--file"));
  g_ptr_array_add (array, fd_str);
  g_ptr_array_add (array, g_strdup (script->s_infile));

  g_array_append_val (script->fd_array, fd);

  return TRUE;

bail:
  g_ptr_array_set_size (array, 0);
  return FALSE;
}
#endif /* HAVE_BWRAP */

static char **
expand_thumbnailing_cmd (const char  *cmd,
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

#ifdef HAVE_BWRAP
  if (script->sandbox)
    {
      if (!add_bwrap (array, script))
        {
          g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
			       "Bubblewrap setup failed");
	  goto bail;
	}
    }
#endif

  got_in = got_out = FALSE;
  for (i = 0; cmd_elems[i] != NULL; i++)
    {
      char *expanded;

      expanded = expand_thumbnailing_elem (cmd_elems[i],
					   size,
					   script->s_infile ? script->s_infile : script->infile,
					   script->s_outfile ? script->s_outfile : script->outfile,
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
child_setup (gpointer user_data)
{
  GArray *fd_array = user_data;
  int i;

  /* If no fd_array was specified, don't care. */
  if (fd_array == NULL)
    return;

  /* Otherwise, mark not - close-on-exec all the fds in the array */
  for (i = 0; i < fd_array->len; i++)
    fcntl (g_array_index (fd_array, int, i), F_SETFD, 0);
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
  if (exec->outdir)
    {
      g_rmdir (exec->outdir);
      g_free (exec->outdir);
    }
  g_free (exec->s_infile);
  g_free (exec->s_outfile);
  if (exec->fd_array)
    g_array_free (exec->fd_array, TRUE);
  g_free (exec);
}

static void
clear_fd (gpointer data)
{
  int *fd_p = data;
  if (fd_p != NULL && *fd_p != -1)
    close (*fd_p);
}

static ScriptExec *
script_exec_new (const char *uri)
{
  ScriptExec *exec;
  g_autoptr(GFile) file = NULL;

  exec = g_new0 (ScriptExec, 1);
#ifdef HAVE_BWRAP
  /* Bubblewrap is not used if the application is already sandboxed in
   * Flatpak as all privileges to create a new namespace are dropped when
   * the initial one is created. */
  if (!g_file_test ("/.flatpak-info", G_FILE_TEST_IS_REGULAR))
    exec->sandbox = TRUE;
#endif

  file = g_file_new_for_uri (uri);

  exec->infile = g_file_get_path (file);
  if (!exec->infile)
    goto bail;

#ifdef HAVE_BWRAP
  if (exec->sandbox)
    {
      char *tmpl;
      g_autofree char *ext = NULL;

      exec->fd_array = g_array_new (FALSE, TRUE, sizeof (int));
      g_array_set_clear_func (exec->fd_array, clear_fd);

      tmpl = g_strdup ("/tmp/gnome-desktop-thumbnailer-XXXXXX");
      exec->outdir = g_mkdtemp (tmpl);
      if (!exec->outdir)
        goto bail;
      exec->outfile = g_build_filename (exec->outdir, "gnome-desktop-thumbnailer.png", NULL);

      ext = get_extension (exec->infile);
      exec->s_infile = g_strdup_printf ("/tmp/gnome-desktop-file-to-thumbnail.%s", ext);
      exec->s_outfile = g_strdup ("/tmp/gnome-desktop-thumbnailer.png");
    }
  else
#endif
    {
      int fd;
      g_autofree char *tmpname = NULL;

      fd = g_file_open_tmp (".gnome_desktop_thumbnail.XXXXXX", &tmpname, NULL);
      if (fd == -1)
        goto bail;
      close (fd);
      exec->outfile = g_steal_pointer (&tmpname);
    }

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
  expanded_script = expand_thumbnailing_cmd (cmd, exec, size, error);
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
		      child_setup, exec->fd_array, NULL, &error_out,
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

