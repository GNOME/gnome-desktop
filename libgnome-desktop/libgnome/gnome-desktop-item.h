#ifndef __GNOME_DITEM_H__
#define __GNOME_DITEM_H__ 1

BEGIN_GNOME_DECLS

typedef enum {
  GNOME_DESKTOP_ITEM_RUN_IN_TERMINAL = 1<<0,
  GNOME_DESKTOP_ITEM_IS_DIRECTORY = 1<<1
} GnomeDesktopItemFlags;

typedef enum {
  GNOME_DESKTOP_ITEM_UNKNOWN=0,
  GNOME_DESKTOP_ITEM_GNOME=1,
  GNOME_DESKTOP_ITEM_KDE=2
  /* More types can be added here as needed */
} GnomeDesktopItemFormat;

typedef enum {
  GNOME_DESKTOP_ITEM_UNCHANGED=0,
  GNOME_DESKTOP_ITEM_CHANGED=1,
  GNOME_DESKTOP_ITEM_DISAPPEARED=2
} GnomeDesktopItemStatus;

typedef struct _GnomeDesktopItem GnomeDesktopItem;

typedef enum {
  GNOME_DESKTOP_ITEM_LOAD_ONLY_IF_EXISTS = 1<<0,
  GNOME_DESKTOP_ITEM_LOAD_NO_SYNC = 1<<1,
  GNOME_DESKTOP_ITEM_LOAD_NO_DIRS = 1<<2,
  GNOME_DESKTOP_ITEM_LOAD_NO_SUBDIRS = 1<<3,
  GNOME_DESKTOP_ITEM_LOAD_NO_SUBITEMS = 1<<4
} GnomeDesktopItemLoadFlags;

/* Returned item from new*() and copy() methods have a refcount of 1 */
GnomeDesktopItem *gnome_desktop_item_new (void);
GnomeDesktopItem *gnome_desktop_item_new_from_file (const char *file, GnomeDesktopItemLoadFlags flags);
GnomeDesktopItem *gnome_desktop_item_copy (const GnomeDesktopItem *item); /* semi-deep copy */
void gnome_desktop_item_save (GnomeDesktopItem *item, const char *under, GnomeDesktopItemLoadFlags save_flags);

void gnome_desktop_item_ref (GnomeDesktopItem *item);
void gnome_desktop_item_unref (GnomeDesktopItem *item);

int gnome_desktop_item_launch (const GnomeDesktopItem *item, int argc, const char **argv);

gboolean gnome_desktop_item_exists (const GnomeDesktopItem *item);
GnomeDesktopItemFlags gnome_desktop_item_get_flags (const GnomeDesktopItem *item);
const char *gnome_desktop_item_get_location (const GnomeDesktopItem *item);
const char **gnome_desktop_item_get_command (const GnomeDesktopItem *item, int *argc); /* argc can be NULL */
const char *gnome_desktop_item_get_icon_path (const GnomeDesktopItem *item);
const char *gnome_desktop_item_get_name (const GnomeDesktopItem *item, const char *language);
const char *gnome_desktop_item_get_comment (const GnomeDesktopItem *item, const char *language);
const char *gnome_desktop_item_get_attribute (const GnomeDesktopItem *item, const char *attr_name);
const GSList *gnome_desktop_item_get_subitems (const GnomeDesktopItem *item);
GnomeDesktopItemFormat gnome_desktop_item_get_format (const GnomeDesktopItem *item);
GnomeDesktopItemStatus gnome_desktop_item_get_file_status (GnomeDesktopItem *item, const char *under);

/* Free the return value but not the contained strings */
GSList *gnome_desktop_item_get_languages(const GnomeDesktopItem *item);
GSList *gnome_desktop_item_get_attributes(const GnomeDesktopItem *item);

void gnome_desktop_item_set_format (GnomeDesktopItem *item, GnomeDesktopItemFormat fmt);
void gnome_desktop_item_set_name (GnomeDesktopItem *item, const char *language, const char *name);
void gnome_desktop_item_set_command (GnomeDesktopItem *item, const char **command);
void gnome_desktop_item_set_icon_path (GnomeDesktopItem *item, const char *icon_path);
void gnome_desktop_item_set_comment (GnomeDesktopItem *item, const char *language, const char *comment);
void gnome_desktop_item_set_attribute (GnomeDesktopItem *item, const char *attr_name, const char *attr_value);
void gnome_desktop_item_set_subitems (GnomeDesktopItem *item, GSList *subitems); /* subitems is adopted by this function */
void gnome_desktop_item_set_flags (GnomeDesktopItem *item, GnomeDesktopItemFlags flags);
void gnome_desktop_item_set_location (GnomeDesktopItem *item, const char *location);

END_GNOME_DECLS

#endif /* __GNOME_DITEM_H__ */
