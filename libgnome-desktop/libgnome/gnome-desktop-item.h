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
#include <gobject/gtype.h>

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

#define GNOME_DESKTOP_ITEM_ENCODING	"Encoding" /* string */
#define GNOME_DESKTOP_ITEM_VERSION	"Version"  /* numeric */
#define GNOME_DESKTOP_ITEM_NAME		"Name" /* localestring */
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

typedef enum {
	/* Use the TryExec field to determine if this should be loaded */
        GNOME_DESKTOP_ITEM_LOAD_ONLY_IF_EXISTS = 1<<0,
        GNOME_DESKTOP_ITEM_LOAD_NO_TRANSLATIONS = 1<<1
} GnomeDesktopItemLoadFlags;

typedef enum {
	GNOME_DESKTOP_ITEM_ERROR_NO_FILENAME /* No filename set or given on save */,
	GNOME_DESKTOP_ITEM_ERROR_UNKNOWN_ENCODING /* Unknown encoding of the file */,
	GNOME_DESKTOP_ITEM_ERROR_CANNOT_OPEN /* Cannot open file */
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
GnomeDesktopItem *      gnome_desktop_item_copy              (const GnomeDesktopItem     *item);

/* if under is NULL save in original location */
gboolean                gnome_desktop_item_save              (GnomeDesktopItem           *item,
							      const char                 *under,
							      gboolean			  force,
							      GError                    **error);
GnomeDesktopItem *      gnome_desktop_item_ref               (GnomeDesktopItem           *item);
void                    gnome_desktop_item_unref             (GnomeDesktopItem           *item);
int                     gnome_desktop_item_launch            (const GnomeDesktopItem     *item,
							      int                         argc,
							      char                      **argv,
							      GError                    **error);

/* A list of files or urls dropped onto an icon This is the output
 * of gnome_vfs_uri_list_parse */
int                     gnome_desktop_item_drop_uri_list     (const GnomeDesktopItem     *item,
							      GList                      *uri_list,
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
GnomeDesktopItemStatus  gnome_desktop_item_get_file_status   (const GnomeDesktopItem     *item);

/*
 * Get the icon, this is not as simple as getting the Icon attr as it actually tries to find
 * it and returns %NULL if it can't
 */
char *                  gnome_desktop_item_get_icon          (const GnomeDesktopItem     *item);


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
 * Clearing attributes
 */
#define                 gnome_desktop_item_clear_attr(item,attr) \
				gnome_desktop_item_set_string(item,attr,NULL)
void			gnome_desktop_item_clear_section     (GnomeDesktopItem           *item,
							      const char                 *section);

G_END_DECLS

#endif /* GNOME_DITEM_H */
