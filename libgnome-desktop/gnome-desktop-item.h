/* -*- Mode: C; c-set-style: linux indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gnome-ditem.h - GNOME Desktop File Representation 

   Copyright (C) 1999, 2000 Red Hat Inc.
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
/* FIXME #include <gnome-desktop/GNOME_Desktop.h>
#include <bonobo/bonobo-arg.h>
*/

/* FIXME: */
   typedef enum
   {
      GNOME_Desktop_ENTRY_TYPE_UNKNOWN,
      GNOME_Desktop_ENTRY_TYPE_APPLICATION,
      GNOME_Desktop_ENTRY_TYPE_URL,
      GNOME_Desktop_ENTRY_TYPE_LINK,
      GNOME_Desktop_ENTRY_TYPE_FSDEVICE,
      GNOME_Desktop_ENTRY_TYPE_MIMETYPE,
      GNOME_Desktop_ENTRY_TYPE_DIRECTORY,
      GNOME_Desktop_ENTRY_TYPE_SERVICE,
      GNOME_Desktop_ENTRY_TYPE_SERVICETYPE,
      GNOME_Desktop_ENTRY_TYPE_PANEL_APPLET,
      GNOME_Desktop_ENTRY_TYPE_SEPARATOR
   }
   GNOME_Desktop_EntryType;

G_BEGIN_DECLS

typedef enum {
	/* Start a terminal to run this */
        GNOME_DESKTOP_ITEM_RUN_IN_TERMINAL = 1<<0,
	/* Directory type */
        GNOME_DESKTOP_ITEM_IS_DIRECTORY = 1<<1
} GnomeDesktopItemFlags;

typedef enum {
        GNOME_DESKTOP_ITEM_UNCHANGED = 0,
        GNOME_DESKTOP_ITEM_CHANGED = 1,
        GNOME_DESKTOP_ITEM_DISAPPEARED = 2
} GnomeDesktopItemStatus;

typedef struct _GnomeDesktopItem GnomeDesktopItem;

typedef enum {
	/* Use the TryExec field to determine if this shoul dbe loaded */
        GNOME_DESKTOP_ITEM_LOAD_ONLY_IF_EXISTS = 1<<0,
} GnomeDesktopItemLoadFlags;

/* Returned item from new*() and copy() methods have a refcount of 1 */
GnomeDesktopItem *      gnome_desktop_item_new               (void);
GnomeDesktopItem *      gnome_desktop_item_new_from_file     (const char                 *file,
							      GnomeDesktopItemLoadFlags   flags);
GnomeDesktopItem *      gnome_desktop_item_copy              (GnomeDesktopItem           *item);

/* if under is NULL save in original location */
gboolean                gnome_desktop_item_save              (GnomeDesktopItem           *item,
							      const char                 *under);
GnomeDesktopItem *      gnome_desktop_item_ref               (GnomeDesktopItem           *item);
void                    gnome_desktop_item_unref             (GnomeDesktopItem           *item);
int                     gnome_desktop_item_launch            (GnomeDesktopItem           *item,
							      int                         argc,
							      const char                **argv);

/* A list of files or urls dropped onto an icon, the proper (Url or File
   exec is run you can pass directly the output of 
   gnome_uri_list_extract_filenames */
int                     gnome_desktop_item_drop_uri_list     (GnomeDesktopItem           *item,
							      GList                      *uri_list);

gboolean                gnome_desktop_item_exists            (GnomeDesktopItem           *item);
GnomeDesktopItemFlags   gnome_desktop_item_get_flags         (GnomeDesktopItem           *item);
char *                  gnome_desktop_item_get_location      (GnomeDesktopItem           *item);
GNOME_Desktop_EntryType gnome_desktop_item_get_type          (GnomeDesktopItem           *item);
gchar *                 gnome_desktop_item_get_command       (GnomeDesktopItem           *item);
gchar *                 gnome_desktop_item_get_tryexec       (GnomeDesktopItem           *item);
gchar *                 gnome_desktop_item_get_icon_path     (GnomeDesktopItem           *item);

/* Note: you want to search each language in the user's search path */
gchar *                 gnome_desktop_item_get_name          (GnomeDesktopItem           *item,
							      const char                 *language);
gchar *                 gnome_desktop_item_get_comment       (GnomeDesktopItem           *item,
							      const char                 *language);

gchar *                 gnome_desktop_item_get_local_name    (GnomeDesktopItem           *item);
gchar *                 gnome_desktop_item_get_local_comment (GnomeDesktopItem           *item);

/* FIXME: */
/*BonoboArg *             gnome_desktop_item_get_attribute     (GnomeDesktopItem           *item,
							      const char                 *attr_name);*/
GSList *                gnome_desktop_item_get_order         (GnomeDesktopItem           *item);
GnomeDesktopItemStatus  gnome_desktop_item_get_file_status   (GnomeDesktopItem           *item);


/* Free the return value but not the contained strings */
GSList *                gnome_desktop_item_get_languages     (GnomeDesktopItem           *item);
GSList *                gnome_desktop_item_get_attributes    (GnomeDesktopItem           *item);

/* the _clear_name clears the name for all languages */
void                    gnome_desktop_item_clear_name        (GnomeDesktopItem           *item);
void                    gnome_desktop_item_set_name          (GnomeDesktopItem           *item,
							      const char                 *language,
							      const char                 *name);
void                    gnome_desktop_item_set_type          (GnomeDesktopItem           *item,
							      GNOME_Desktop_EntryType     type);
void                    gnome_desktop_item_set_command       (GnomeDesktopItem           *item,
							      const char                 *command);
void                    gnome_desktop_item_set_icon_path     (GnomeDesktopItem           *item,
							      const char                 *icon_path);
/* the _clear_comment clears the comment for all languages */
void                    gnome_desktop_item_clear_comment     (GnomeDesktopItem           *item);
void                    gnome_desktop_item_set_comment       (GnomeDesktopItem           *item,
							      const char                 *language,
							      const char                 *comment);
/* FIXME:
void                    gnome_desktop_item_set_attribute     (GnomeDesktopItem           *item,
							      const char                 *attr_name,
							      const BonoboArg            *attr_value);
							      */
void                    gnome_desktop_item_set_order         (GnomeDesktopItem           *item,
							      GSList                     *order);
void                    gnome_desktop_item_set_flags         (GnomeDesktopItem           *item,
							      GnomeDesktopItemFlags       flags);
void                    gnome_desktop_item_set_location      (GnomeDesktopItem           *item,
							      const char                 *location);


G_END_DECLS

#endif /* GNOME_DITEM_H */
