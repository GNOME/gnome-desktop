/* -*- mode: C; c-file-style: "linux"; indent-tabs-mode: t -*-
 * gnome-wall-clock.h - monitors TZ setting files and signals changes
 *
 * Copyright (C) 2010 Red Hat, Inc.
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * Author: Colin Walters <walters@verbum.org>
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include "gnome-gettext-portable.h"

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include "gnome-wall-clock.h"
#include <gdesktop-enums.h>
#include "gnome-datetime-source.h"

struct _GnomeWallClockPrivate {
	guint clock_update_id;

	GTimeZone *timezone;

	char *clock_string;
	
	GFileMonitor *tz_monitor;
	GSettings    *desktop_settings;

	gboolean time_only;
	gboolean force_seconds;
};

enum {
	PROP_0,
	PROP_CLOCK,
	PROP_TIMEZONE,
	PROP_TIME_ONLY,
	PROP_FORCE_SECONDS,
};

G_DEFINE_TYPE_WITH_PRIVATE (GnomeWallClock, gnome_wall_clock, G_TYPE_OBJECT);

static gboolean update_clock (gpointer data);
static void on_schema_change (GSettings *schema,
                              const char *key,
                              gpointer user_data);
static void on_tz_changed (GFileMonitor *monitor,
                           GFile        *file,
                           GFile        *other_file,
                           GFileMonitorEvent *event,
                           gpointer      user_data);


static void
gnome_wall_clock_init (GnomeWallClock *self)
{
	GFile *tz;

	self->priv = gnome_wall_clock_get_instance_private (self);

	self->priv->timezone = g_time_zone_new_local ();

	self->priv->clock_string = NULL;
	
	tz = g_file_new_for_path ("/etc/localtime");
	self->priv->tz_monitor = g_file_monitor_file (tz, 0, NULL, NULL);
	g_object_unref (tz);
	
	g_signal_connect (self->priv->tz_monitor, "changed", G_CALLBACK (on_tz_changed), self);
	
	self->priv->desktop_settings = g_settings_new ("org.gnome.desktop.interface");
	g_signal_connect (self->priv->desktop_settings, "changed", G_CALLBACK (on_schema_change), self);

	update_clock (self);
}

static void
gnome_wall_clock_finalize (GObject *object)
{
	GnomeWallClock *self = GNOME_WALL_CLOCK (object);

	if (self->priv->clock_update_id) {
		g_source_remove (self->priv->clock_update_id);
		self->priv->clock_update_id = 0;
	}

	g_clear_object (&self->priv->tz_monitor);
	g_clear_object (&self->priv->desktop_settings);
	g_time_zone_unref (self->priv->timezone);
	g_free (self->priv->clock_string);

	G_OBJECT_CLASS (gnome_wall_clock_parent_class)->finalize (object);
}

static void
gnome_wall_clock_get_property (GObject    *gobject,
			       guint       prop_id,
			       GValue     *value,
			       GParamSpec *pspec)
{
	GnomeWallClock *self = GNOME_WALL_CLOCK (gobject);

	switch (prop_id)
	{
	case PROP_TIME_ONLY:
		g_value_set_boolean (value, self->priv->time_only);
		break;
	case PROP_TIMEZONE:
		g_value_set_boxed (value, self->priv->timezone);
		break;
	case PROP_CLOCK:
		g_value_set_string (value, self->priv->clock_string);
		break;
	case PROP_FORCE_SECONDS:
		g_value_set_boolean (value, self->priv->force_seconds);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
		break;
	}
}

static void
gnome_wall_clock_set_property (GObject      *gobject,
			       guint         prop_id,
			       const GValue *value,
			       GParamSpec   *pspec)
{
	GnomeWallClock *self = GNOME_WALL_CLOCK (gobject);

	switch (prop_id)
	{
	case PROP_TIME_ONLY:
		self->priv->time_only = g_value_get_boolean (value);
		update_clock (self);
		break;
	case PROP_FORCE_SECONDS:
		self->priv->force_seconds = g_value_get_boolean (value);
		update_clock (self);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
		break;
	}
}

static void
gnome_wall_clock_class_init (GnomeWallClockClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	gobject_class->get_property = gnome_wall_clock_get_property;
	gobject_class->set_property = gnome_wall_clock_set_property;
	gobject_class->finalize = gnome_wall_clock_finalize;

	/**
	 * GnomeWallClock:clock:
	 *
	 * A formatted string representing the current clock display.
	 */
	g_object_class_install_property (gobject_class,
					 PROP_CLOCK,
					 g_param_spec_string ("clock",
							      "",
							      "",
							      NULL,
							      G_PARAM_READABLE));

	/**
	 * GnomeWallClock:timezone:
	 *
	 * The timezone used for this clock
	 */
	g_object_class_install_property (gobject_class,
					 PROP_TIMEZONE,
					 g_param_spec_boxed ("timezone",
							     "",
							     "",
							     G_TYPE_TIME_ZONE,
							     G_PARAM_READABLE));

	/**
	 * GnomeWallClock:time-only:
	 *
	 * If %TRUE, the formatted clock will never include a date or the
	 * day of the week, irrespective of configuration.
	 */
	g_object_class_install_property (gobject_class,
					 PROP_TIME_ONLY,
					 g_param_spec_boolean ("time-only",
							       "",
							       "",
							       FALSE,
							       G_PARAM_READABLE | G_PARAM_WRITABLE));

	/**
	 * GnomeWallClock:force-seconds:
	 *
	 * If %TRUE, the formatted clock will always have seconds precision and the
	 * 'clock' property will always be updated every second, irrespective of
	 * system configuration.
	 */
	g_object_class_install_property (gobject_class,
					 PROP_FORCE_SECONDS,
					 g_param_spec_boolean ("force-seconds",
							       "",
							       "",
							       FALSE,
							       G_PARAM_READABLE | G_PARAM_WRITABLE));
}

/* Replace 'target' with 'replacement' in the input string. */
static char *
string_replace (const char *input,
                const char *target,
                const char *replacement)
{
	char **pieces = NULL;
	char *output = NULL;

	pieces = g_strsplit (input, target, -1);
	output = g_strjoinv (replacement, pieces);
	g_strfreev (pieces);
	return output;
}

/* This function wraps g_date_time_format, replacing underscores with en-space
 * as it looks visually better in time strings.
 */
static char *
date_time_format (GDateTime *datetime,
                  const char *format)
{
	char *replaced_format;
	char *ret;
	char *no_enspace;
	gboolean is_utf8;

	is_utf8 = g_get_charset (NULL);

	/* Replace en-space with underscore before passing it to
	 * g_date_time_format.  */
	no_enspace = string_replace (format, " ", "_");
	g_debug ("no_enspace: %s", no_enspace);
	replaced_format = g_date_time_format (datetime, no_enspace);
	g_debug ("replaced_format: %s", replaced_format);

	g_free (no_enspace);

	if (is_utf8) {
		/* Replace underscores with en-space */
		ret = string_replace (replaced_format, "_", " ");
	} else {
		/* Replace the underscore with double spaces instead of en-space */
		ret = string_replace (replaced_format, "_", "  ");
	}

	g_free (replaced_format);
	g_debug ("is_utf8: %s", is_utf8 ? "TRUE" : "FALSE");
	g_debug ("ret: %s", ret);
	return ret;
}

/*
 * Translate @str according to the locale defined by LC_TIME; unlike
 * dcgettext(), the translation is still taken from the LC_MESSAGES
 * catalogue and not the LC_TIME one.
 */
static const char *
translate_time_format_string (const char *str)
{
  const char *locale = g_getenv ("LC_TIME");
  const char *res;
  char *sep;
  locale_t loc = (locale_t)0;

  if (locale)
    loc = newlocale (LC_MESSAGES_MASK, locale, (locale_t)0);

  sep = strchr (str, '\004');
  res = g_dpgettext_l (loc, GETTEXT_PACKAGE, str, sep ? sep - str + 1 : 0);

  if (loc != (locale_t)0)
    freelocale (loc);

  return res;
}

#define T_(string) translate_time_format_string (string)

/**
 * gnome_wall_clock_string_for_datetime:
 *
 * Returns: (skip): a newly allocated string representing the date & time
 * passed, with the options applied.
 */
char *
gnome_wall_clock_string_for_datetime (GnomeWallClock      *self,
				      GDateTime           *now,
				      GDesktopClockFormat  clock_format,
				      gboolean             show_weekday,
				      gboolean             show_full_date,
				      gboolean             show_seconds)
{
	const char *format_string;

	g_debug ("clock_format: %s", clock_format == G_DESKTOP_CLOCK_FORMAT_24H ? "24h" : "12h");
	g_debug ("show_weekday: %s", show_weekday ? "TRUE" : "FALSE");
	g_debug ("show_full_date: %s", show_full_date ? "TRUE" : "FALSE");
	g_debug ("show_seconds: %s", show_seconds ? "TRUE" : "FALSE");

	if (clock_format == G_DESKTOP_CLOCK_FORMAT_24H) {
		if (show_full_date) {
			if (show_weekday)
				/* Translators: This is the time format with full date
				   plus day used in 24-hour mode. Please keep the under-
				   score to separate the date from the time. */
				format_string = show_seconds ? T_(N_("%a %b %-e_%R:%S"))
					: T_(N_("%a %b %-e_%R"));
			else
				/* Translators: This is the time format with full date
				   used in 24-hour mode. Please keep the underscore to
				   separate the date from the time. */
				format_string = show_seconds ? T_(N_("%b %-e_%R:%S"))
					: T_(N_("%b %-e_%R"));
		} else if (show_weekday) {
			/* Translators: This is the time format with day used
			   in 24-hour mode. */
			format_string = show_seconds ? T_(N_("%a %R:%S"))
				: T_(N_("%a %R"));
		} else {
			/* Translators: This is the time format without date used
			   in 24-hour mode. */
			format_string = show_seconds ? T_(N_("%R:%S"))
                                : T_(N_("%R"));
		}
	} else {
		if (show_full_date) {
			if (show_weekday)
				/* Translators: This is a time format with full date
				   plus day used for AM/PM. Please keep the under-
				   score to separate the date from the time. */
				format_string = show_seconds ? T_(N_("%a %b %-e_%l:%M:%S %p"))
					: T_(N_("%a %b %-e_%l:%M %p"));
			else
				/* Translators: This is a time format with full date
				   used for AM/PM. Please keep the underscore to
				   separate the date from the time. */
				format_string = show_seconds ? T_(N_("%b %-e_%l:%M:%S %p"))
					: T_(N_("%b %-e_%l:%M %p"));
		} else if (show_weekday) {
			/* Translators: This is a time format with day used
			   for AM/PM. */
			format_string = show_seconds ? T_(N_("%a %l:%M:%S %p"))
				: T_(N_("%a %l:%M %p"));
		} else {
			/* Translators: This is a time format without date used
			   for AM/PM. */
			format_string = show_seconds ? T_(N_("%l:%M:%S %p"))
				: T_(N_("%l:%M %p"));
		}
	}

	g_debug ("format_string: %s", format_string);

	return date_time_format (now, format_string);
}

#undef T_

gboolean
has_seconds (GnomeWallClock* self)
{
	if (self->priv->force_seconds) {
		return TRUE;
	}

	return g_settings_get_boolean (self->priv->desktop_settings, "clock-show-seconds");
}

static gboolean
update_clock (gpointer data)
{
	GnomeWallClock   *self = data;
	GDesktopClockFormat clock_format;
	gboolean show_full_date;
	gboolean show_weekday;
	gboolean show_seconds;
	GSource *source;
	GDateTime *now;
	GDateTime *expiry;

	clock_format = g_settings_get_enum (self->priv->desktop_settings, "clock-format");
	show_weekday = !self->priv->time_only && g_settings_get_boolean (self->priv->desktop_settings, "clock-show-weekday");
	show_full_date = !self->priv->time_only && g_settings_get_boolean (self->priv->desktop_settings, "clock-show-date");
	show_seconds = has_seconds (self);

	now = g_date_time_new_now (self->priv->timezone);
	if (show_seconds)
		expiry = g_date_time_add_seconds (now, 1);
	else
		expiry = g_date_time_add_seconds (now, 60 - g_date_time_get_second (now));

	if (self->priv->clock_update_id)
		g_source_remove (self->priv->clock_update_id);

	source = _gnome_datetime_source_new (now, expiry, TRUE);
	g_source_set_priority (source, G_PRIORITY_HIGH);
	g_source_set_callback (source, update_clock, self, NULL);
	self->priv->clock_update_id = g_source_attach (source, NULL);
	g_source_unref (source);

	g_free (self->priv->clock_string);
	self->priv->clock_string = gnome_wall_clock_string_for_datetime (self,
									 now,
									 clock_format,
									 show_weekday,
									 show_full_date,
									 show_seconds);

	g_date_time_unref (now);
	g_date_time_unref (expiry);

	g_object_notify ((GObject*)self, "clock");

	return FALSE;
}

static gboolean
update_timezone (gpointer data)
{
	GnomeWallClock *self = data;

	if (self->priv->timezone != NULL)
		g_time_zone_unref (self->priv->timezone);
	self->priv->timezone = g_time_zone_new_local ();
	g_object_notify ((GObject*)self, "timezone");

	return update_clock (data);
}

static void
on_schema_change (GSettings *schema,
                  const char *key,
                  gpointer user_data)
{
	if (g_strcmp0 (key, "clock-format") != 0 &&
	    g_strcmp0 (key, "clock-show-seconds") != 0 &&
	    g_strcmp0 (key, "clock-show-weekday") != 0 &&
	    g_strcmp0 (key, "clock-show-date") != 0) {
		return;
	}

	g_debug ("Updating clock because schema changed");
	update_clock (user_data);
}

static void
on_tz_changed (GFileMonitor      *monitor,
               GFile             *file,
               GFile             *other_file,
               GFileMonitorEvent *event,
               gpointer           user_data)
{
	g_debug ("Updating clock because timezone changed");
	update_timezone (user_data);
}

/**
 * gnome_wall_clock_new:
 *
 * Creates a new #GnomeWallClock
 *
 * Return value: the new clock
 **/
GnomeWallClock *
gnome_wall_clock_new (void)
{
	return (GnomeWallClock *) g_object_new (GNOME_TYPE_WALL_CLOCK, NULL);
}

/**
 * gnome_wall_clock_get_clock:
 * @clock: a #GnomeWallClock
 *
 * Returns the string representing the current time of this clock
 * according to the user settings.
 *
 * Return value: the time of the clock as a string.
 *      This string points to internally allocated storage and
 *      must not be freed, modified or stored.
 */
const char *
gnome_wall_clock_get_clock (GnomeWallClock *clock)
{
	return clock->priv->clock_string;
}

/**
 * gnome_wall_clock_get_timezone:
 * @clock: a #GnomeWallClock
 *
 * Returns the current local time zone used by this clock.
 *
 * Return value: (transfer none): the #GTimeZone of the clock.
 */
GTimeZone *
gnome_wall_clock_get_timezone (GnomeWallClock *clock)
{
	return clock->priv->timezone;
}
