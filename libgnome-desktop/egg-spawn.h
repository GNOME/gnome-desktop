/* egg-spawn.h:
 *
 * Copyright (C) 2002  Sun Microsystems Inc.
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

#ifndef __EGG_SPAWN_H__
#define __EGG_SPAWN_H__

#include <gdk/gdk.h>
#include <glib/gspawn.h>

G_BEGIN_DECLS

gchar **egg_make_spawn_environment_for_screen (GdkScreen  *screen,
					       gchar     **envp);

gboolean egg_spawn_async_on_screen (const gchar           *working_directory,
				    gchar                **argv,
				    gchar                **envp,
				    GSpawnFlags            flags,
				    GSpawnChildSetupFunc   child_setup,
				    gpointer               user_data,
				    GdkScreen             *screen,
				    gint                  *child_pid,
				    GError               **error);

gboolean egg_spawn_async_with_pipes_on_screen (const gchar          *working_directory,
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
					       GError              **error);

gboolean egg_spawn_sync_on_screen (const gchar          *working_directory,
				   gchar               **argv,
				   gchar               **envp,
				   GSpawnFlags           flags,
				   GSpawnChildSetupFunc  child_setup,
				   gpointer              user_data,
				   GdkScreen            *screen,
				   gchar               **standard_output,
				   gchar               **standard_error,
				   gint                 *exit_status,
				   GError              **error);

gboolean egg_spawn_command_line_sync_on_screen  (const gchar          *command_line,
						 GdkScreen            *screen,
						 gchar               **standard_output,
						 gchar               **standard_error,
						 gint                 *exit_status,
						 GError              **error);
gboolean egg_spawn_command_line_async_on_screen (const gchar          *command_line,
						 GdkScreen            *screen,
						 GError              **error);

G_END_DECLS

#endif /* __EGG_SPAWN_H__ */
