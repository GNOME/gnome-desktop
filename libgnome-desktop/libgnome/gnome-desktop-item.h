/* -*- Mode: C; c-set-style: linux indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gnome-ditem.h - GNOME Desktop File Representation 

   Copyright (C) 1999, 2000 Red Hat Inc.
   Copyright (C) 2001 Sid Vicious
   All rights reserved.

   This file is part of the Gnome Library.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
   
   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */
/*
  @NOTATION@
 */

#ifndef GNOME_DITEM_H
#define GNOME_DITEM_H

#include <glib.h>
#include <glib-object.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum {
	GNOME_DESKTOP_ITEM_TYPE_NULL = 0 /* This means its NULL, that is, not
					   * set */,
	GNOME_DESKTOP_ITEM_TYPE_OTHER /* This means it's not one of the below
					      strings types, and you must get the
					      Type attribute. */,

	/* These are the standard compliant types: */
	GNOME_DESKTOP_ITEM_TYPE_APPLICATION,
	GNOME_DESKTOP_ITEM_TYPE_LINK,
	GNOME_DESKTOP_ITEM_TYPE_FSDEVICE,
	GNOME_DESKTOP_ITEM_TYPE_MIME_TYPE,
	GNOME_DESKTOP_ITEM_TYPE_DIRECTORY,
	GNOME_DESKTOP_ITEM_TYPE_SERVICE,
	GNOME_DESKTOP_ITEM_TYPE_SERVICE_TYPE
} GnomeDesktopItemType;

typedef enum {
        GNOME_DESKTOP_ITEM_UNCHANGED = 0,
        GNOME_DESKTOP_ITEM_CHANGED = 1,
        GNOME_DESKTOP_ITEM_DISAPPEARED = 2
} GnomeDesktopItemStatus;

#define GNOME_TYPE_DESKTOP_ITEM         (gnome_desktop_item_get_type ())
GType gnome_desktop_item_get_type       (void);

typedef struct _GnomeDesktopItem GnomeDesktopItem;

/* standard */
#define GNOME_DESKTOP_ITEM_ENCODING	"Encoding" /* string */
#define GNOME_DESKTOP_ITEM_VERSION	"Version"  /* numeric */
#define GNOME_DESKTOP_ITEM_NAME		"Name" /* localestring */
#define GNOME_DESKTOP_ITEM_GENERIC_NAME	"GenericName" /* localestring */
#define GNOME_DESKTOP_ITEM_TYPE		"Type" /* string */
#define GNOME_DESKTOP_ITEM_FILE_PATTERN "FilePattern" /* regexp(s) */
#define GNOME_DESKTOP_ITEM_TRY_EXEC	"TryExec" /* string */
#define GNOME_DESKTOP_ITEM_NO_DISPLAY	"NoDisplay" /* boolean */
#define GNOME_DESKTOP_ITEM_COMMENT	"Comment" /* localestring */
#define GNOME_DESKTOP_ITEM_EXEC		"Exec" /* string */
#define GNOME_DESKTOP_ITEM_ACTIONS	"Actions" /* strings */
#define GNOME_DESKTOP_ITEM_ICON		"Icon" /* string */
#define GNOME_DESKTOP_ITEM_MINI_ICON	"MiniIcon" /* string */
#define GNOME_DESKTOP_ITEM_HIDDEN	"Hidden" /* boolean */
#define GNOME_DESKTOP_ITEM_PATH		"Path" /* string */
#define GNOME_DESKTOP_ITEM_TERMINAL	"Terminal" /* boolean */
#define GNOME_DESKTOP_ITEM_TERMINAL_OPTIONS "TerminalOptions" /* string */
#define GNOME_DESKTOP_ITEM_SWALLOW_TITLE "SwallowTitle" /* string */
#define GNOME_DESKTOP_ITEM_SWALLOW_EXEC	"SwallowExec" /* string */
#define GNOME_DESKTOP_ITEM_MIME_TYPE	"MimeType" /* regexp(s) */
#define GNOME_DESKTOP_ITEM_PATTERNS	"Patterns" /* regexp(s) */
#define GNOME_DESKTOP_ITEM_DEFAULT_APP	"DefaultApp" /* string */
#define GNOME_DESKTOP_ITEM_DEV		"Dev" /* string */
#define GNOME_DESKTOP_ITEM_FS_TYPE	"FSType" /* string */
#define GNOME_DESKTOP_ITEM_MOUNT_POINT	"MountPoint" /* string */
#define GNOME_DESKTOP_ITEM_READ_ONLY	"ReadOnly" /* boolean */
#define GNOME_DESKTOP_ITEM_UNMOUNT_ICON "UnmountIcon" /* string */
#define GNOME_DESKTOP_ITEM_SORT_ORDER	"SortOrder" /* strings */
#define GNOME_DESKTOP_ITEM_URL		"URL" /* string */
#define GNOME_DESKTOP_ITEM_DOC_PATH	"X-GNOME-DocPath" /* string */

/* The vfolder proposal */
#define GNOME_DESKTOP_ITEM_CATEGORIES	"Categories" /* string */
#define GNOME_DESKTOP_ITEM_ONLY_SHOW_IN	"OnlyShowIn" /* string */

typedef enum {
	/* Use the TryExec field to determine if this should be loaded */
        GNOME_DESKTOP_ITEM_LOAD_ONLY_IF_EXISTS = 1<<0,
        GNOME_DESKTOP_ITEM_LOAD_NO_TRANSLATIONS = 1<<1
} GnomeDesktopItemLoadFlags;

typedef enum {
	/* Never launch more instances even if the app can only
	 * handle one file and we have passed many */
        GNOME_DESKTOP_ITEM_LAUNCH_ONLY_ONE = 1<<0,
	/* Use current directory instead of home directory */
        GNOME_DESKTOP_ITEM_LAUNCH_USE_CURRENT_DIR = 1<<1,
	/* Append the list of URIs to the command if no Exec
	 * parameter is specified, instead of launching the 
	 * app without parameters. */
	GNOME_DESKTOP_ITEM_LAUNCH_APPEND_URIS = 1<<2,
	/* Same as above but instead append local paths */
	GNOME_DESKTOP_ITEM_LAUNCH_APPEND_PATHS = 1<<3,
	/* Don't automatically reap child process.  */
	GNOME_DESKTOP_ITEM_LAUNCH_DO_NOT_REAP_CHILD = 1<<4
} GnomeDesktopItemLaunchFlags;

typedef enum {
	/* Don't check the kde directories */
        GNOME_DESKTOP_ITEM_ICON_NO_KDE = 1<<0
} GnomeDesktopItemIconFlags;

typedef enum {
	GNOME_DESKTOP_ITEM_ERROR_NO_FILENAME /* No filename set or given on save */,
	GNOME_DESKTOP_ITEM_ERROR_UNKNOWN_ENCODING /* Unknown encoding of the file */,
	GNOME_DESKTOP_ITEM_ERROR_CANNOT_OPEN /* Cannot open file */,
	GNOME_DESKTOP_ITEM_ERROR_NO_EXEC_STRING /* Cannot launch due to no execute string */,
	GNOME_DESKTOP_ITEM_ERROR_BAD_EXEC_STRING /* Cannot launch due to bad execute string */,
	GNOME_DESKTOP_ITEM_ERROR_NO_URL /* No URL on a url entry*/,
	GNOME_DESKTOP_ITEM_ERROR_NOT_LAUNCHABLE /* Not a launchable type of item */,
	GNOME_DESKTOP_ITEM_ERROR_INVALID_TYPE /* Not of type application/x-gnome-app-info */
} GnomeDesktopItemError;

/* Note that functions can also return the G_FILE_ERROR_* errors */

#define GNOME_DESKTOP_ITEM_ERROR gnome_desktop_item_error_quark ()
GQuark gnome_desktop_item_error_quark (void);

/* Returned item from new*() and copy() methods have a refcount of 1 */
GnomeDesktopItem *      gnome_desktop_item_new               (void);
GnomeDesktopItem *      gnome_desktop_item_new_from_file     (const char                 *file,
							      GnomeDesktopItemLoadFlags   flags,
							      GError                    **error);
GnomeDesktopItem *      gnome_desktop_item_new_from_uri      (const char                 *uri,
							      GnomeDesktopItemLoadFlags   flags,
							      GError                    **error);
GnomeDesktopItem *      gnome_desktop_item_new_from_string   (const char                 *uri,
							      const char                 *string,
							      gssize                      length,
							      GnomeDesktopItemLoadFlags   flags,
							      GError                    **error);
GnomeDesktopItem *      gnome_desktop_item_new_from_basename (const char                 *basename,
							      GnomeDesktopItemLoadFlags   flags,
							      GError                    **error);
GnomeDesktopItem *      gnome_desktop_item_copy              (const GnomeDesktopItem     *item);

/* if under is NULL save in original location */
gboolean                gnome_desktop_item_save              (GnomeDesktopItem           *item,
							      const char                 *under,
							      gboolean			  force,
							      GError                    **error);
GnomeDesktopItem *      gnome_desktop_item_ref               (GnomeDesktopItem           *item);
void                    gnome_desktop_item_unref             (GnomeDesktopItem           *item);
int			gnome_desktop_item_launch	     (const GnomeDesktopItem     *item,
							      GList                      *file_list,
							      GnomeDesktopItemLaunchFlags flags,
							      GError                    **error);
int			gnome_desktop_item_launch_with_env   (const GnomeDesktopItem     *item,
							      GList                      *file_list,
							      GnomeDesktopItemLaunchFlags flags,
							      char                      **envp,
							      GError                    **error);

int                     gnome_desktop_item_launch_on_screen  (const GnomeDesktopItem       *item,
							      GList                        *file_list,
							      GnomeDesktopItemLaunchFlags   flags,
							      GdkScreen                    *screen,
							      int                           workspace,
							      GError                      **error);

/* A list of files or urls dropped onto an icon */
int                     gnome_desktop_item_drop_uri_list     (const GnomeDesktopItem     *item,
							      const char                 *uri_list,
							      GnomeDesktopItemLaunchFlags flags,
							      GError                    **error);

int                     gnome_desktop_item_drop_uri_list_with_env    (const GnomeDesktopItem     *item,
								      const char                 *uri_list,
								      GnomeDesktopItemLaunchFlags flags,
								      char                      **envp,
								      GError                    **error);

gboolean                gnome_desktop_item_exists            (const GnomeDesktopItem     *item);

GnomeDesktopItemType	gnome_desktop_item_get_entry_type    (const GnomeDesktopItem	 *item);
/* You could also just use the set_string on the TYPE argument */
void			gnome_desktop_item_set_entry_type    (GnomeDesktopItem		 *item,
							      GnomeDesktopItemType	  type);

/* Get current location on disk */
const char *            gnome_desktop_item_get_location      (const GnomeDesktopItem     *item);
void                    gnome_desktop_item_set_location      (GnomeDesktopItem           *item,
							      const char                 *location);
void                    gnome_desktop_item_set_location_file (GnomeDesktopItem           *item,
							      const char                 *file);
GnomeDesktopItemStatus  gnome_desktop_item_get_file_status   (const GnomeDesktopItem     *item);

/*
 * Get the icon, this is not as simple as getting the Icon attr as it actually tries to find
 * it and returns %NULL if it can't
 */
char *                  gnome_desktop_item_get_icon          (const GnomeDesktopItem     *item,
							      GtkIconTheme               *icon_theme);

char *                  gnome_desktop_item_find_icon         (GtkIconTheme               *icon_theme,
							      const char                 *icon,
							      /* size is only a suggestion */
							      int                         desired_size,
							      int                         flags);


/*
 * Reading/Writing different sections, NULL is the standard section
 */
gboolean                gnome_desktop_item_attr_exists       (const GnomeDesktopItem     *item,
							      const char                 *attr);

/*
 * String type
 */
const char *            gnome_desktop_item_get_string        (const GnomeDesktopItem     *item,
							      const char		 *attr);

void                    gnome_desktop_item_set_string        (GnomeDesktopItem           *item,
							      const char		 *attr,
							      const char                 *value);

const char *            gnome_desktop_item_get_attr_locale   (const GnomeDesktopItem     *item,
							      const char		 *attr);

/*
 * LocaleString type
 */
const char *            gnome_desktop_item_get_localestring  (const GnomeDesktopItem     *item,
							      const char		 *attr);
const char *            gnome_desktop_item_get_localestring_lang (const GnomeDesktopItem *item,
								  const char		 *attr,
								  const char             *language);
/* use g_list_free only */
GList *                 gnome_desktop_item_get_languages     (const GnomeDesktopItem     *item,
							      const char		 *attr);

void                    gnome_desktop_item_set_localestring  (GnomeDesktopItem           *item,
							      const char		 *attr,
							      const char                 *value);
void                    gnome_desktop_item_set_localestring_lang  (GnomeDesktopItem      *item,
								   const char		 *attr,
								   const char		 *language,
								   const char            *value);
void                    gnome_desktop_item_clear_localestring(GnomeDesktopItem           *item,
							      const char		 *attr);

/*
 * Strings, Regexps types
 */

/* use gnome_desktop_item_free_string_list */
char **                 gnome_desktop_item_get_strings       (const GnomeDesktopItem     *item,
							      const char		 *attr);

void			gnome_desktop_item_set_strings       (GnomeDesktopItem           *item,
							      const char                 *attr,
							      char                      **strings);

/*
 * Boolean type
 */
gboolean                gnome_desktop_item_get_boolean       (const GnomeDesktopItem     *item,
							      const char		 *attr);

void                    gnome_desktop_item_set_boolean       (GnomeDesktopItem           *item,
							      const char		 *attr,
							      gboolean                    value);

/*
 * Xserver time of user action that caused the application launch to start.
 */
void                    gnome_desktop_item_set_launch_time   (GnomeDesktopItem           *item,
							      guint32                     timestamp);

/*
 * Clearing attributes
 */
#define                 gnome_desktop_item_clear_attr(item,attr) \
				gnome_desktop_item_set_string(item,attr,NULL)
void			gnome_desktop_item_clear_section     (GnomeDesktopItem           *item,
							      const char                 *section);

G_END_DECLS

#endif /* GNOME_DITEM_H */
