/* egg-spawn.c:
 *
 * Copyright (C) 2002 Sun Microsystems Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Mark McLoughlin <mark@skynet.ie>
 */

#include <config.h>
#include <string.h>

#include "egg-spawn.h"

#include <glib.h>
#include <gdk/gdk.h>

extern char **environ;

/**
 * egg_make_spawn_environment_for_screen:
 * @screen: A #GdkScreen
 * @envp: program environment to copy, or NULL to use current environment.
 *
 * Returns a modified copy of the program environment @envp (or the current
 * environment if @envp is NULL) with $DISPLAY set such that a launched 
 * application (which calls gdk_display_open()) inheriting this environment
 * would have @screen as its default screen..
 *
 * Returns: a newly-allocated %NULL-terminated array of strings or
 * %NULL on error. Use g_strfreev() to free it.
 **/
gchar **
egg_make_spawn_environment_for_screen (GdkScreen  *screen,
				       gchar     **envp)
{
  gchar **retval = NULL;
  gchar  *display_name;
  gint    display_index = -1;
  gint    i, env_len;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  if (envp == NULL)
    envp = environ;

  for (env_len = 0; envp [env_len]; env_len++)
    if (!strncmp (envp [env_len], "DISPLAY", strlen ("DISPLAY")))
      display_index = env_len;

  if (display_index == -1)
    display_index = env_len++;

  retval = g_new (char *, env_len + 1);
  retval [env_len] = NULL;

  display_name = gdk_screen_make_display_name (screen);

  for (i = 0; i < env_len; i++)
    if (i == display_index)
      retval [i] = g_strconcat ("DISPLAY=", display_name, NULL);
    else
      retval [i] = g_strdup (envp [i]);

  g_assert (i == env_len);

  g_free (display_name);

  return retval;
}

/**
 * egg_spawn_async_on_screen:
 * @working_directory: child's current working directory, or %NULL to inherit parent's
 * @argv: child's argument vector
 * @envp: child's environment, or %NULL to inherit parent's
 * @flags: flags from #GSpawnFlags
 * @child_setup: function to run in the child just before <function>exec()</function>
 * @user_data: user data for @child_setup
 * @screen: a #GdkScreen
 * @child_pid: return location for child process ID, or %NULL
 * @error: return location for error
 *
 * Like g_spawn_async(), except the child process is spawned in such
 * an environment that on calling gdk_display_open() it would be
 * returned a #GdkDisplay with @screen as the default screen.
 *
 * This is useful for applications which wish to launch an application
 * on a specific screen.
 *
 * Return value: %TRUE on success, %FALSE if error is set
 **/
gboolean
egg_spawn_async_on_screen (const gchar           *working_directory,
			   gchar                **argv,
			   gchar                **envp,
			   GSpawnFlags            flags,
			   GSpawnChildSetupFunc   child_setup,
			   gpointer               user_data,
			   GdkScreen             *screen,
			   gint                  *child_pid,
			   GError               **error)
{
  GdkScreen  *default_screen;
  gchar     **new_envp = NULL;
  gboolean    retval;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);

  default_screen = gdk_display_get_default_screen (
				gdk_screen_get_display (screen));
  if (screen != default_screen)
    new_envp = egg_make_spawn_environment_for_screen (screen, envp);

  retval = g_spawn_async (working_directory, argv,
			  new_envp ? new_envp : envp,
			  flags, child_setup, user_data,
			  child_pid, error);

  g_strfreev (new_envp);

  return retval;
}

/**
 * egg_spawn_async_with_pipes_on_screen:
 * @working_directory: child's current working directory, or %NULL to inherit parent's
 * @argv: child's argument vector
 * @envp: child's environment, or %NULL to inherit parent's
 * @flags: flags from #GSpawnFlags
 * @child_setup: function to run in the child just before <function>exec()</function>
 * @user_data: user data for @child_setup
 * @screen: a #GdkScreen
 * @child_pid: return location for child process ID, or %NULL
 * @standard_input: return location for file descriptor to write to child's stdin, or %NULL
 * @standard_output: return location for file descriptor to read child's stdout, or %NULL
 * @standard_error: return location for file descriptor to read child's stderr, or %NULL
 * @error: return location for error
 *
 * Like g_spawn_async_with_pipes(), except the child process is
 * spawned in such an environment that on calling gdk_display_open()
 * it would be returned a #GdkDisplay with @screen as the default
 * screen.
 *
 * This is useful for applications which wish to launch an application
 * on a specific screen.
 *
 * Return value: %TRUE on success, %FALSE if an error was set
 **/
gboolean
egg_spawn_async_with_pipes_on_screen (const gchar          *working_directory,
				      gchar               **argv,
				      gchar               **envp,
				      GSpawnFlags           flags,
				      GSpawnChildSetupFunc  child_setup,
				      gpointer              user_data,
				      GdkScreen            *screen,
				      gint                 *child_pid,
				      gint                 *standard_input,
				      gint                 *standard_output,
				      gint                 *standard_error,
				      GError              **error)
{
  GdkScreen  *default_screen;
  gchar     **new_envp = NULL;
  gboolean    retval;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);

  default_screen = gdk_display_get_default_screen (
				gdk_screen_get_display (screen));
  if (screen != default_screen)
    new_envp = egg_make_spawn_environment_for_screen (screen, envp);

  retval = g_spawn_async_with_pipes (working_directory, argv,
				     new_envp ? new_envp : envp,
				     flags, child_setup, user_data,
				     child_pid, standard_input,
				     standard_output, standard_error,
				     error);

  g_strfreev (new_envp);

  return retval;
}

/**
 * egg_spawn_sync_on_screen:
 * @working_directory: child's current working directory, or %NULL to inherit parent's
 * @argv: child's argument vector
 * @envp: child's environment, or %NULL to inherit parent's
 * @flags: flags from #GSpawnFlags
 * @child_setup: function to run in the child just before <function>exec()</function>
 * @user_data: user data for @child_setup
 * @screen: a #GdkScreen
 * @standard_output: return location for child output
 * @standard_error: return location for child error messages
 * @exit_status: child exit status, as returned by <function>waitpid()</function>
 * @error: return location for error
 *
 * Like g_spawn_sync(), except the child process is spawned in such
 * an environment that on calling gdk_display_open() it would be
 * returned a #GdkDisplay with @screen as the default screen.
 *
 * This is useful for applications which wish to launch an application
 * on a specific screen.
 *
 * Return value: %TRUE on success, %FALSE if an error was set.
 **/
gboolean
egg_spawn_sync_on_screen (const gchar          *working_directory,
			  gchar               **argv,
			  gchar               **envp,
			  GSpawnFlags           flags,
			  GSpawnChildSetupFunc  child_setup,
			  gpointer              user_data,
			  GdkScreen            *screen,
			  gchar               **standard_output,
			  gchar               **standard_error,
			  gint                 *exit_status,
			  GError              **error)
{
  GdkScreen  *default_screen;
  gchar     **new_envp = NULL;
  gboolean    retval;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);

  default_screen = gdk_display_get_default_screen (
				gdk_screen_get_display (screen));
  if (screen != default_screen)
    new_envp = egg_make_spawn_environment_for_screen (screen, envp);

  retval = g_spawn_sync (working_directory, argv,
			 new_envp ? new_envp : envp,
			 flags, child_setup, user_data,
			 standard_output, standard_error,
			 exit_status, error);

  g_strfreev (new_envp);

  return retval;
}

/**
 * egg_spawn_command_line_sync_on_screen:
 * @command_line: a command line
 * @screen: a #GdkScreen
 * @standard_output: return location for child output
 * @standard_error: return location for child errors
 * @exit_status: return location for child exit status
 * @error: return location for errors
 *
 * Like g_spawn_command_line_sync(), except the child process is
 * spawned in such an environment that on calling gdk_display_open()
 * it would be returned a #GdkDisplay with @screen as the default
 * screen.
 *
 * This is useful for applications which wish to launch an application
 * on a specific screen.
 *
 * Return value: %TRUE on success, %FALSE if an error was set
 **/
gboolean
egg_spawn_command_line_sync_on_screen  (const gchar  *command_line,
					GdkScreen    *screen,
					gchar       **standard_output,
					gchar       **standard_error,
					gint         *exit_status,
					GError      **error)
{
  gchar    **argv = NULL;
  gboolean   retval;

  g_return_val_if_fail (command_line != NULL, FALSE);

  if (!g_shell_parse_argv (command_line,
			   NULL, &argv,
			   error))
    return FALSE;

  retval = egg_spawn_sync_on_screen (NULL,
				     argv,
				     NULL,
				     G_SPAWN_SEARCH_PATH,
				     NULL,
				     NULL,
				     screen,
				     standard_output,
				     standard_error,
				     exit_status,
				     error);

  g_strfreev (argv);

  return retval;
}

/**
 * egg_spawn_command_line_async_on_screen:
 * @command_line: a command line
 * @screen: a #GdkScreen
 * @error: return location for errors
 *
 * Like g_spawn_command_line_async(), except the child process is
 * spawned in such an environment that on calling gdk_display_open()
 * it would be returned a #GdkDisplay with @screen as the default
 * screen.
 *
 * This is useful for applications which wish to launch an application
 * on a specific screen.
 *
 * Return value: %TRUE on success, %FALSE if error is set.
 **/
gboolean
egg_spawn_command_line_async_on_screen (const gchar  *command_line,
					GdkScreen    *screen,
					GError      **error)
{
  gchar    **argv = NULL;
  gboolean   retval;

  g_return_val_if_fail (command_line != NULL, FALSE);

  if (!g_shell_parse_argv (command_line,
			   NULL, &argv,
			   error))
    return FALSE;

  retval = egg_spawn_async_on_screen (NULL,
				      argv,
				      NULL,
				      G_SPAWN_SEARCH_PATH,
				      NULL,
				      NULL,
				      screen,
				      NULL,
				      error);
  g_strfreev (argv);

  return retval;
}
