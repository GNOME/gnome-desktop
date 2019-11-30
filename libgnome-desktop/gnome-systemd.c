/* gnome-systemd.c
 *
 * Copyright 2019 Benjamin Berg <bberg@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "config.h"

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include "gnome-systemd.h"

#ifdef HAVE_SYSTEMD
#include <errno.h>
#include <systemd/sd-login.h>
#endif

#ifdef HAVE_SYSTEMD
typedef struct {
  char       *name;
  char       *description;
  gint32      pid;
} StartSystemdScopeData;

static void
start_systemd_scope_data_free (StartSystemdScopeData *data)
{
  g_clear_pointer (&data->name, g_free);
  g_clear_pointer (&data->description, g_free);
  g_free (data);
}

static void
on_start_transient_unit_cb (GObject      *source,
                            GAsyncResult *res,
                            gpointer      user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  g_autoptr (GVariant) reply = NULL;
  GError *error = NULL;
  StartSystemdScopeData *task_data = g_task_get_task_data (task);

  reply = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source),
                                         res, &error);
  if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_warning ("Could not create transient scope for PID %d: %s",
                 task_data->pid, error->message);
      g_task_return_error (task, error);
      return;
    }

  g_debug ("Created transient scope for PID %d", task_data->pid);

  g_task_return_boolean (task, TRUE);
}

static void
start_systemd_scope (GDBusConnection *connection, GTask *task)
{
  GVariantBuilder builder;
  g_autofree char *unit_name = NULL;
  StartSystemdScopeData *task_data = g_task_get_task_data (task);

  g_assert (task_data != NULL);

  /* This needs to be unique, hopefully the pid will be enough. */
  unit_name = g_strdup_printf ("gnome-launched-%s-%d.scope", task_data->name, task_data->pid);

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("(ssa(sv)a(sa(sv)))"));
  g_variant_builder_add (&builder, "s", unit_name);
  g_variant_builder_add (&builder, "s", "fail");

  /* Note that gnome-session ships a drop-in to control further defaults. */
  g_variant_builder_open (&builder, G_VARIANT_TYPE ("a(sv)"));
  if (task_data->description)
    g_variant_builder_add (&builder,
                           "(sv)",
                           "Description",
                           g_variant_new_string (task_data->description));
  g_variant_builder_add (&builder,
                         "(sv)",
                         "PIDs",
                          g_variant_new_fixed_array (G_VARIANT_TYPE_UINT32, &task_data->pid, 1, 4));
  g_variant_builder_close (&builder);

  g_variant_builder_open (&builder, G_VARIANT_TYPE ("a(sa(sv))"));
  g_variant_builder_close (&builder);

  g_dbus_connection_call (connection,
                          "org.freedesktop.systemd1",
                          "/org/freedesktop/systemd1",
                          "org.freedesktop.systemd1.Manager",
                          "StartTransientUnit",
                          g_variant_builder_end (&builder),
                          G_VARIANT_TYPE ("(o)"),
                          G_DBUS_CALL_FLAGS_NO_AUTO_START,
                          1000,
                          g_task_get_cancellable (task),
                          on_start_transient_unit_cb,
                          task);
}

static void
on_bus_gotten_cb (GObject      *source,
                  GAsyncResult *res,
                  gpointer      user_data)
{
  g_autoptr(GTask) task = G_TASK (user_data);
  g_autoptr(GDBusConnection) connection = NULL;
  GError *error = NULL;

  connection = g_bus_get_finish (res, &error);
  if (error)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Could not get session bus: %s", error->message);

      g_task_return_error (task, error);
      return;
    }

  start_systemd_scope (connection, g_steal_pointer (&task));
}
#endif

/**
 * gnome_start_systemd_scope:
 * @name: Name for the application
 * @pid: The PID of the application
 * @description: (nullable): A description to use for the unit, or %NULL
 * @connection: (nullable): An #GDBusConnection to the session bus, or %NULL
 * @cancellable: (nullable): #GCancellable to use
 * @callback: (nullable): Callback to call when the operation is done
 * @user_data: Data to be passed to @callback
 *
 * If the current process is running inside a user systemd instance, then move
 * the launched PID into a transient scope. The given @name will be used to
 * create a unit name. It should be the application ID for desktop files or
 * the executable in all other cases.
 *
 * It is advisable to use this function every time where the started application
 * can be considered reasonably independent of the launching application. Placing
 * it in a scope creates proper separation between the programs rather than being
 * considered a single entity by systemd.
 *
 * It is always safe to call this function. Note that a successful return code
 * does not imply that a unit has been created. It solely means that no error
 * condition was hit sending the request.
 *
 * If @connection is %NULL then g_dbus_get() will be called internally.
 *
 * Note that most callers will not need to handle errors. As such, it is normal
 * to pass a %NULL @callback.
 *
 * Stability: unstable
 */
void
gnome_start_systemd_scope (const char           *name,
                           gint32                pid,
                           const char           *description,
                           GDBusConnection      *connection,
                           GCancellable         *cancellable,
                           GAsyncReadyCallback   callback,
                           gpointer              user_data)
{
  g_autoptr(GTask) task = NULL;

#ifdef HAVE_SYSTEMD
  g_autofree char *own_unit = NULL;
  const char *valid_chars =
    "-._1234567890"
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  StartSystemdScopeData *task_data;
  gint res;

  task = g_task_new (NULL, cancellable, callback, user_data);
  task_data = g_new0 (StartSystemdScopeData, 1);

  task_data->pid = pid;

  /* Create a nice and (mangled) name to embed into the unit */
  if (name == NULL)
    name = "anonymous";

  if (name[0] == '/')
    name++;

  task_data->name = g_str_to_ascii (name, "C");
  g_strdelimit (task_data->name, "/", '-');
  g_strcanon (task_data->name, valid_chars, '_');

  task_data->description = g_strdup (description);
  if (task_data->description == NULL)
    {
      const char *app_name = g_get_application_name();

      if (app_name)
        task_data->description = g_strdup_printf ("Application launched by %s",
                                                  app_name);
    }

  g_task_set_task_data (task, task_data, (GDestroyNotify) start_systemd_scope_data_free);

  /* We cannot do anything if this process is not managed by the
   * systemd user instance. */
  res = sd_pid_get_user_unit (getpid (), &own_unit);
  if (res == -ENODATA)
    {
      g_debug ("Not systemd managed, will not move PID %d into transient scope\n", pid);
      g_task_return_boolean (task, TRUE);

      return;
    }
  if (res < 0)
    {
      g_warning ("Error fetching user unit for own pid: %d\n", -res);
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               g_io_error_from_errno (-res),
                               "Error fetching user unit for own pid: %d", -res);

      return;
    }

  if (connection == NULL)
    g_bus_get (G_BUS_TYPE_SESSION, cancellable, on_bus_gotten_cb, g_steal_pointer (&task));
  else
    start_systemd_scope (connection, g_steal_pointer (&task));
#else
  g_debug ("Not creating transient scope for PID %d. Systemd support not compiled in.", pid);

  task = g_task_new (NULL, cancellable, callback, user_data);
  g_task_return_boolean (task, TRUE);
#endif
}

/**
 * gnome_start_systemd_scope_finish:
 * @res: A #GAsyncResult
 * @error: Return location for errors, or %NULL to ignore
 *
 * Finish an asynchronous operation to create a transient scope that was
 * started with gnome_start_systemd_scope().
 *
 * Note that a successful return code does not imply that a unit has been
 * created. It solely means that no error condition was hit sending the request.
 *
 * Returns: %FALSE on error, %TRUE otherwise
 */
gboolean
gnome_start_systemd_scope_finish (GAsyncResult  *res,
                                  GError       **error)
{
  return g_task_propagate_boolean (G_TASK (res), error);
}
