/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-file.h: Caja file model.

   Copyright (C) 1999, 2000, 2001 Eazel, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
   Boston, MA 02110-1301, USA.

   Author: Darin Adler <darin@bentspoon.com>
*/

#ifndef CAJA_FILE_H
#define CAJA_FILE_H

#include <gtk/gtk.h>
#include <gio/gio.h>

#include "caja-file-attributes.h"
#include "caja-icon-info.h"

G_BEGIN_DECLS

/* CajaFile is an object used to represent a single element of a
 * CajaDirectory. It's lightweight and relies on CajaDirectory
 * to do most of the work.
 */

/* CajaFile is defined both here and in caja-directory.h. */
#ifndef CAJA_FILE_DEFINED
#define CAJA_FILE_DEFINED
typedef struct CajaFile CajaFile;
#endif

#define CAJA_TYPE_FILE caja_file_get_type()
#define CAJA_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_FILE, CajaFile))
#define CAJA_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_FILE, CajaFileClass))
#define CAJA_IS_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_FILE))
#define CAJA_IS_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), CAJA_TYPE_FILE))
#define CAJA_FILE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), CAJA_TYPE_FILE, CajaFileClass))

typedef enum
{
    CAJA_FILE_SORT_NONE,
    CAJA_FILE_SORT_BY_DISPLAY_NAME,
    CAJA_FILE_SORT_BY_DIRECTORY,
    CAJA_FILE_SORT_BY_SIZE,
    CAJA_FILE_SORT_BY_TYPE,
    CAJA_FILE_SORT_BY_MTIME,
    CAJA_FILE_SORT_BY_ATIME,
    CAJA_FILE_SORT_BY_EMBLEMS,
    CAJA_FILE_SORT_BY_TRASHED_TIME,
    CAJA_FILE_SORT_BY_SIZE_ON_DISK,
    CAJA_FILE_SORT_BY_EXTENSION
} CajaFileSortType;

typedef enum
{
    CAJA_REQUEST_NOT_STARTED,
    CAJA_REQUEST_IN_PROGRESS,
    CAJA_REQUEST_DONE
} CajaRequestStatus;

typedef enum
{
    CAJA_FILE_ICON_FLAGS_NONE = 0,
    CAJA_FILE_ICON_FLAGS_USE_THUMBNAILS = (1<<0),
    CAJA_FILE_ICON_FLAGS_IGNORE_VISITING = (1<<1),
    CAJA_FILE_ICON_FLAGS_EMBEDDING_TEXT = (1<<2),
    CAJA_FILE_ICON_FLAGS_FOR_DRAG_ACCEPT = (1<<3),
    CAJA_FILE_ICON_FLAGS_FOR_OPEN_FOLDER = (1<<4),
    /* whether the thumbnail size must match the display icon size */
    CAJA_FILE_ICON_FLAGS_FORCE_THUMBNAIL_SIZE = (1<<5),
    /* uses the icon of the mount if present */
    CAJA_FILE_ICON_FLAGS_USE_MOUNT_ICON = (1<<6),
    /* render the mount icon as an emblem over the regular one */
    CAJA_FILE_ICON_FLAGS_USE_MOUNT_ICON_AS_EMBLEM = (1<<7)
} CajaFileIconFlags;

/* Emblems sometimes displayed for CajaFiles. Do not localize. */
#define CAJA_FILE_EMBLEM_NAME_SYMBOLIC_LINK "symbolic-link"
#define CAJA_FILE_EMBLEM_NAME_CANT_READ "noread"
#define CAJA_FILE_EMBLEM_NAME_CANT_WRITE "nowrite"
#define CAJA_FILE_EMBLEM_NAME_TRASH "trash"
#define CAJA_FILE_EMBLEM_NAME_NOTE "note"
#define CAJA_FILE_EMBLEM_NAME_DESKTOP "desktop"
#define CAJA_FILE_EMBLEM_NAME_SHARED "shared"

typedef void (*CajaFileCallback)          (CajaFile  *file,
        gpointer       callback_data);
typedef void (*CajaFileListCallback)      (GList         *file_list,
        gpointer       callback_data);
typedef void (*CajaFileOperationCallback) (CajaFile  *file,
        GFile         *result_location,
        GError        *error,
        gpointer       callback_data);
typedef int (*CajaWidthMeasureCallback)   (const char    *string,
        void	     *context);
typedef char * (*CajaTruncateCallback)    (const char    *string,
        int	      width,
        void	     *context);


#define CAJA_FILE_ATTRIBUTES_FOR_ICON (CAJA_FILE_ATTRIBUTE_INFO | CAJA_FILE_ATTRIBUTE_LINK_INFO | CAJA_FILE_ATTRIBUTE_THUMBNAIL)

typedef void CajaFileListHandle;

/* GObject requirements. */
GType                   caja_file_get_type                          (void);

/* Getting at a single file. */
CajaFile *          caja_file_get                               (GFile                          *location);
CajaFile *          caja_file_get_by_uri                        (const char                     *uri);

/* Get a file only if the caja version already exists */
CajaFile *          caja_file_get_existing                      (GFile                          *location);
CajaFile *          caja_file_get_existing_by_uri               (const char                     *uri);

/* Covers for g_object_ref and g_object_unref that provide two conveniences:
 * 1) Using these is type safe.
 * 2) You are allowed to call these with NULL,
 */
CajaFile *          caja_file_ref                               (CajaFile                   *file);
void                    caja_file_unref                             (CajaFile                   *file);

/* Monitor the file. */
void                    caja_file_monitor_add                       (CajaFile                   *file,
        gconstpointer                   client,
        CajaFileAttributes          attributes);
void                    caja_file_monitor_remove                    (CajaFile                   *file,
        gconstpointer                   client);

/* Synchronously refreshes file info from disk.
 * This can call caja_file_changed, so don't call this function from any
 * of the callbacks for that event.
 */
void                     caja_file_refresh_info                     (CajaFile                   *file);

/* Waiting for data that's read asynchronously.
 * This interface currently works only for metadata, but could be expanded
 * to other attributes as well.
 */
void                    caja_file_call_when_ready                   (CajaFile                   *file,
        CajaFileAttributes          attributes,
        CajaFileCallback            callback,
        gpointer                        callback_data);
void                    caja_file_cancel_call_when_ready            (CajaFile                   *file,
        CajaFileCallback            callback,
        gpointer                        callback_data);
gboolean                caja_file_check_if_ready                    (CajaFile                   *file,
        CajaFileAttributes          attributes);
void                    caja_file_invalidate_attributes             (CajaFile                   *file,
        CajaFileAttributes          attributes);
void                    caja_file_invalidate_all_attributes         (CajaFile                   *file);

/* Basic attributes for file objects. */
gboolean                caja_file_contains_text                     (CajaFile                   *file);
gboolean                caja_file_is_binary                         (CajaFile                   *file);
char *                  caja_file_get_display_name                  (CajaFile                   *file);
char *                  caja_file_get_edit_name                     (CajaFile                   *file);
char *                  caja_file_get_name                          (CajaFile                   *file);
GFile *                 caja_file_get_location                      (CajaFile                   *file);
char *			 caja_file_get_description			 (CajaFile			 *file);
char *                  caja_file_get_uri                           (CajaFile                   *file);
char *                  caja_file_get_uri_scheme                    (CajaFile                   *file);
CajaFile *          caja_file_get_parent                        (CajaFile                   *file);
GFile *                 caja_file_get_parent_location               (CajaFile                   *file);
char *                  caja_file_get_parent_uri                    (CajaFile                   *file);
char *                  caja_file_get_parent_uri_for_display        (CajaFile                   *file);
gboolean                caja_file_can_get_size                      (CajaFile                   *file);
goffset                 caja_file_get_size                          (CajaFile                   *file);
goffset                 caja_file_get_size_on_disk                  (CajaFile                   *file);
time_t                  caja_file_get_mtime                         (CajaFile                   *file);
GFileType               caja_file_get_file_type                     (CajaFile                   *file);
char *                  caja_file_get_mime_type                     (CajaFile                   *file);
gboolean                caja_file_is_mime_type                      (CajaFile                   *file,
        const char                     *mime_type);
gboolean                caja_file_is_launchable                     (CajaFile                   *file);
gboolean                caja_file_is_symbolic_link                  (CajaFile                   *file);
gboolean                caja_file_is_mountpoint                     (CajaFile                   *file);
GMount *                caja_file_get_mount                         (CajaFile                   *file);
char *                  caja_file_get_volume_free_space             (CajaFile                   *file);
char *                  caja_file_get_volume_name                   (CajaFile                   *file);
char *                  caja_file_get_symbolic_link_target_path     (CajaFile                   *file);
char *                  caja_file_get_symbolic_link_target_uri      (CajaFile                   *file);
gboolean                caja_file_is_broken_symbolic_link           (CajaFile                   *file);
gboolean                caja_file_is_caja_link                  (CajaFile                   *file);
gboolean                caja_file_is_executable                     (CajaFile                   *file);
gboolean                caja_file_is_directory                      (CajaFile                   *file);
gboolean                caja_file_is_user_special_directory         (CajaFile                   *file,
        GUserDirectory                 special_directory);
gboolean		caja_file_is_archive			(CajaFile			*file);
gboolean                caja_file_is_in_trash                       (CajaFile                   *file);
gboolean                caja_file_is_in_desktop                     (CajaFile                   *file);
gboolean		caja_file_is_home				(CajaFile                   *file);
gboolean                caja_file_is_desktop_directory              (CajaFile                   *file);
GError *                caja_file_get_file_info_error               (CajaFile                   *file);
gboolean                caja_file_get_directory_item_count          (CajaFile                   *file,
        guint                          *count,
        gboolean                       *count_unreadable);
void                    caja_file_recompute_deep_counts             (CajaFile                   *file);
CajaRequestStatus   caja_file_get_deep_counts                   (CajaFile                   *file,
        guint                          *directory_count,
        guint                          *file_count,
        guint                          *unreadable_directory_count,
        goffset                        *total_size,
        goffset                        *total_size_on_disk,
        gboolean                        force);
gboolean                caja_file_should_show_thumbnail             (CajaFile                   *file);
gboolean                caja_file_should_show_directory_item_count  (CajaFile                   *file);
gboolean                caja_file_should_show_type                  (CajaFile                   *file);
GList *                 caja_file_get_keywords                      (CajaFile                   *file);
void                    caja_file_set_keywords                      (CajaFile                   *file,
        GList                          *keywords);
GList *                 caja_file_get_emblem_icons                  (CajaFile                   *file,
        char                          **exclude);
GList *                 caja_file_get_emblem_pixbufs                (CajaFile                   *file,
        int                             size,
        gboolean                        force_size,
        char                          **exclude);
char *                  caja_file_get_top_left_text                 (CajaFile                   *file);
char *                  caja_file_peek_top_left_text                (CajaFile                   *file,
        gboolean                        need_large_text,
        gboolean                       *got_top_left_text);

void                    caja_file_set_attributes                    (CajaFile                   *file,
        GFileInfo                      *attributes,
        CajaFileOperationCallback   callback,
        gpointer                        callback_data);
GFilesystemPreviewType  caja_file_get_filesystem_use_preview        (CajaFile *file);

char *                  caja_file_get_filesystem_id                 (CajaFile                   *file);

CajaFile *          caja_file_get_trash_original_file           (CajaFile                   *file);

/* Permissions. */
gboolean                caja_file_can_get_permissions               (CajaFile                   *file);
gboolean                caja_file_can_set_permissions               (CajaFile                   *file);
guint                   caja_file_get_permissions                   (CajaFile                   *file);
gboolean                caja_file_can_get_owner                     (CajaFile                   *file);
gboolean                caja_file_can_set_owner                     (CajaFile                   *file);
gboolean                caja_file_can_get_group                     (CajaFile                   *file);
gboolean                caja_file_can_set_group                     (CajaFile                   *file);
char *                  caja_file_get_owner_name                    (CajaFile                   *file);
char *                  caja_file_get_group_name                    (CajaFile                   *file);
GList *                 caja_get_user_names                         (void);
GList *                 caja_get_all_group_names                    (void);
GList *                 caja_file_get_settable_group_names          (CajaFile                   *file);
gboolean                caja_file_can_get_selinux_context           (CajaFile                   *file);
char *                  caja_file_get_selinux_context               (CajaFile                   *file);

/* "Capabilities". */
gboolean                caja_file_can_read                          (CajaFile                   *file);
gboolean                caja_file_can_write                         (CajaFile                   *file);
gboolean                caja_file_can_execute                       (CajaFile                   *file);
gboolean                caja_file_can_rename                        (CajaFile                   *file);
gboolean                caja_file_can_delete                        (CajaFile                   *file);
gboolean                caja_file_can_trash                         (CajaFile                   *file);

gboolean                caja_file_can_mount                         (CajaFile                   *file);
gboolean                caja_file_can_unmount                       (CajaFile                   *file);
gboolean                caja_file_can_eject                         (CajaFile                   *file);
gboolean                caja_file_can_start                         (CajaFile                   *file);
gboolean                caja_file_can_start_degraded                (CajaFile                   *file);
gboolean                caja_file_can_stop                          (CajaFile                   *file);
GDriveStartStopType     caja_file_get_start_stop_type               (CajaFile                   *file);
gboolean                caja_file_can_poll_for_media                (CajaFile                   *file);
gboolean                caja_file_is_media_check_automatic          (CajaFile                   *file);

void                    caja_file_mount                             (CajaFile                   *file,
        GMountOperation                *mount_op,
        GCancellable                   *cancellable,
        CajaFileOperationCallback   callback,
        gpointer                        callback_data);
void                    caja_file_unmount                           (CajaFile                   *file,
        GMountOperation                *mount_op,
        GCancellable                   *cancellable,
        CajaFileOperationCallback   callback,
        gpointer                        callback_data);
void                    caja_file_eject                             (CajaFile                   *file,
        GMountOperation                *mount_op,
        GCancellable                   *cancellable,
        CajaFileOperationCallback   callback,
        gpointer                        callback_data);

void                    caja_file_start                             (CajaFile                   *file,
        GMountOperation                *start_op,
        GCancellable                   *cancellable,
        CajaFileOperationCallback   callback,
        gpointer                        callback_data);
void                    caja_file_stop                              (CajaFile                   *file,
        GMountOperation                *mount_op,
        GCancellable                   *cancellable,
        CajaFileOperationCallback   callback,
        gpointer                        callback_data);
void                    caja_file_poll_for_media                    (CajaFile                   *file);

/* Basic operations for file objects. */
void                    caja_file_set_owner                         (CajaFile                   *file,
        const char                     *user_name_or_id,
        CajaFileOperationCallback   callback,
        gpointer                        callback_data);
void                    caja_file_set_group                         (CajaFile                   *file,
        const char                     *group_name_or_id,
        CajaFileOperationCallback   callback,
        gpointer                        callback_data);
void                    caja_file_set_permissions                   (CajaFile                   *file,
        guint32                         permissions,
        CajaFileOperationCallback   callback,
        gpointer                        callback_data);
void                    caja_file_rename                            (CajaFile                   *file,
        const char                     *new_name,
        CajaFileOperationCallback   callback,
        gpointer                        callback_data);
void                    caja_file_cancel                            (CajaFile                   *file,
        CajaFileOperationCallback   callback,
        gpointer                        callback_data);

/* Return true if this file has already been deleted.
 * This object will be unref'd after sending the files_removed signal,
 * but it could hang around longer if someone ref'd it.
 */
gboolean                caja_file_is_gone                           (CajaFile                   *file);

/* Return true if this file is not confirmed to have ever really
 * existed. This is true when the CajaFile object has been created, but no I/O
 * has yet confirmed the existence of a file by that name.
 */
gboolean                caja_file_is_not_yet_confirmed              (CajaFile                   *file);

/* Simple getting and setting top-level metadata. */
char *                  caja_file_get_metadata                      (CajaFile                   *file,
        const char                     *key,
        const char                     *default_metadata);
GList *                 caja_file_get_metadata_list                 (CajaFile                   *file,
        const char                     *key);
void                    caja_file_set_metadata                      (CajaFile                   *file,
        const char                     *key,
        const char                     *default_metadata,
        const char                     *metadata);
void                    caja_file_set_metadata_list                 (CajaFile                   *file,
        const char                     *key,
        GList                          *list);

/* Covers for common data types. */
gboolean                caja_file_get_boolean_metadata              (CajaFile                   *file,
        const char                     *key,
        gboolean                        default_metadata);
void                    caja_file_set_boolean_metadata              (CajaFile                   *file,
        const char                     *key,
        gboolean                        default_metadata,
        gboolean                        metadata);
int                     caja_file_get_integer_metadata              (CajaFile                   *file,
        const char                     *key,
        int                             default_metadata);
void                    caja_file_set_integer_metadata              (CajaFile                   *file,
        const char                     *key,
        int                             default_metadata,
        int                             metadata);

#define UNDEFINED_TIME ((time_t) (-1))

time_t                  caja_file_get_time_metadata                 (CajaFile                  *file,
        const char                    *key);
void                    caja_file_set_time_metadata                 (CajaFile                  *file,
        const char                    *key,
        time_t                         time);


/* Attributes for file objects as user-displayable strings. */
char *                  caja_file_get_string_attribute              (CajaFile                   *file,
        const char                     *attribute_name);
char *                  caja_file_get_string_attribute_q            (CajaFile                   *file,
        GQuark                          attribute_q);
char *                  caja_file_get_string_attribute_with_default (CajaFile                   *file,
        const char                     *attribute_name);
char *                  caja_file_get_string_attribute_with_default_q (CajaFile                  *file,
        GQuark                          attribute_q);
char *			caja_file_fit_modified_date_as_string	(CajaFile 			*file,
        int				 width,
        CajaWidthMeasureCallback    measure_callback,
        CajaTruncateCallback	 truncate_callback,
        void				*measure_truncate_context);

/* Matching with another URI. */
gboolean                caja_file_matches_uri                       (CajaFile                   *file,
        const char                     *uri);

/* Is the file local? */
gboolean                caja_file_is_local                          (CajaFile                   *file);

/* Comparing two file objects for sorting */
CajaFileSortType    caja_file_get_default_sort_type             (CajaFile                   *file,
        gboolean                       *reversed);
const gchar *           caja_file_get_default_sort_attribute        (CajaFile                   *file,
        gboolean                       *reversed);

int                     caja_file_compare_for_sort                  (CajaFile                   *file_1,
        CajaFile                   *file_2,
        CajaFileSortType            sort_type,
        gboolean			 directories_first,
        gboolean		  	 reversed);
int                     caja_file_compare_for_sort_by_attribute     (CajaFile                   *file_1,
        CajaFile                   *file_2,
        const char                     *attribute,
        gboolean                        directories_first,
        gboolean                        reversed);
int                     caja_file_compare_for_sort_by_attribute_q   (CajaFile                   *file_1,
        CajaFile                   *file_2,
        GQuark                          attribute,
        gboolean                        directories_first,
        gboolean                        reversed);
gboolean                caja_file_is_date_sort_attribute_q          (GQuark                          attribute);

int                     caja_file_compare_display_name              (CajaFile                   *file_1,
        const char                     *pattern);
int                     caja_file_compare_location                  (CajaFile                    *file_1,
        CajaFile                    *file_2);

/* filtering functions for use by various directory views */
gboolean                caja_file_is_hidden_file                    (CajaFile                   *file);
gboolean                caja_file_should_show                       (CajaFile                   *file,
        gboolean                        show_hidden,
        gboolean                        show_foreign,
        gboolean                        show_backup);
GList                  *caja_file_list_filter_hidden                (GList                          *files,
        gboolean                        show_hidden);


/* Get the URI that's used when activating the file.
 * Getting this can require reading the contents of the file.
 */
gboolean                caja_file_is_launcher                       (CajaFile                   *file);
gboolean                caja_file_is_foreign_link                   (CajaFile                   *file);
gboolean                caja_file_is_trusted_link                   (CajaFile                   *file);
gboolean                caja_file_has_activation_uri                (CajaFile                   *file);
char *                  caja_file_get_activation_uri                (CajaFile                   *file);
GFile *                 caja_file_get_activation_location           (CajaFile                   *file);

char *                  caja_file_get_drop_target_uri               (CajaFile                   *file);

/* Get custom icon (if specified by metadata or link contents) */
char *                  caja_file_get_custom_icon                   (CajaFile                   *file);


GIcon           *caja_file_get_gicon        (CajaFile         *file,
                                             CajaFileIconFlags flags);
CajaIconInfo    *caja_file_get_icon         (CajaFile         *file,
                                             int               size,
                                             int               scale,
                                             CajaFileIconFlags flags);
cairo_surface_t *caja_file_get_icon_surface (CajaFile         *file,
                                             int               size,
                                             gboolean          force_size,
                                             int               scale,
                                             CajaFileIconFlags flags);

gboolean                caja_file_has_open_window                   (CajaFile                   *file);
void                    caja_file_set_has_open_window               (CajaFile                   *file,
        gboolean                        has_open_window);

/* Thumbnailing handling */
gboolean                caja_file_is_thumbnailing                   (CajaFile                   *file);

/* Convenience functions for dealing with a list of CajaFile objects that each have a ref.
 * These are just convenient names for functions that work on lists of GtkObject *.
 */
GList *                 caja_file_list_ref                          (GList                          *file_list);
void                    caja_file_list_unref                        (GList                          *file_list);
void                    caja_file_list_free                         (GList                          *file_list);
GList *                 caja_file_list_copy                         (GList                          *file_list);
GList *			caja_file_list_sort_by_display_name		(GList				*file_list);
void                    caja_file_list_call_when_ready              (GList                          *file_list,
        CajaFileAttributes          attributes,
        CajaFileListHandle        **handle,
        CajaFileListCallback        callback,
        gpointer                        callback_data);
void                    caja_file_list_cancel_call_when_ready       (CajaFileListHandle         *handle);

/* Debugging */
void                    caja_file_dump                              (CajaFile                   *file);

typedef struct _CajaFilePrivate CajaFilePrivate;

struct CajaFile
{
    GObject parent_slot;
    CajaFilePrivate *details;
};

/* This is actually a "protected" type, but it must be here so we can
 * compile the get_date function pointer declaration below.
 */
typedef enum
{
    CAJA_DATE_TYPE_MODIFIED,
    CAJA_DATE_TYPE_CHANGED,
    CAJA_DATE_TYPE_ACCESSED,
    CAJA_DATE_TYPE_PERMISSIONS_CHANGED,
    CAJA_DATE_TYPE_TRASHED
} CajaDateType;

typedef struct
{
    GObjectClass parent_slot;

    /* Subclasses can set this to something other than G_FILE_TYPE_UNKNOWN and
       it will be used as the default file type. This is useful when creating
       a "virtual" CajaFile subclass that you can't actually get real
       information about. For exaple CajaDesktopDirectoryFile. */
    GFileType default_file_type;

    /* Called when the file notices any change. */
    void                  (* changed)                (CajaFile *file);

    /* Called periodically while directory deep count is being computed. */
    void                  (* updated_deep_count_in_progress) (CajaFile *file);

    /* Virtual functions (mainly used for trash directory). */
    void                  (* monitor_add)            (CajaFile           *file,
            gconstpointer           client,
            CajaFileAttributes  attributes);
    void                  (* monitor_remove)         (CajaFile           *file,
            gconstpointer           client);
    void                  (* call_when_ready)        (CajaFile           *file,
            CajaFileAttributes  attributes,
            CajaFileCallback    callback,
            gpointer                callback_data);
    void                  (* cancel_call_when_ready) (CajaFile           *file,
            CajaFileCallback    callback,
            gpointer                callback_data);
    gboolean              (* check_if_ready)         (CajaFile           *file,
            CajaFileAttributes  attributes);
    gboolean              (* get_item_count)         (CajaFile           *file,
            guint                  *count,
            gboolean               *count_unreadable);
    CajaRequestStatus (* get_deep_counts)        (CajaFile           *file,
            guint                  *directory_count,
            guint                  *file_count,
            guint                  *unreadable_directory_count,
            goffset                *total_size,
            goffset                *total_size_on_disk);
    gboolean              (* get_date)               (CajaFile           *file,
            CajaDateType        type,
            time_t                 *date);
    char *                (* get_where_string)       (CajaFile           *file);

    void                  (* set_metadata)           (CajaFile           *file,
            const char             *key,
            const char             *value);
    void                  (* set_metadata_as_list)   (CajaFile           *file,
            const char             *key,
            char                  **value);

    void                  (* mount)                  (CajaFile                   *file,
            GMountOperation                *mount_op,
            GCancellable                   *cancellable,
            CajaFileOperationCallback   callback,
            gpointer                        callback_data);
    void                 (* unmount)                 (CajaFile                   *file,
            GMountOperation                *mount_op,
            GCancellable                   *cancellable,
            CajaFileOperationCallback   callback,
            gpointer                        callback_data);
    void                 (* eject)                   (CajaFile                   *file,
            GMountOperation                *mount_op,
            GCancellable                   *cancellable,
            CajaFileOperationCallback   callback,
            gpointer                        callback_data);

    void                  (* start)                  (CajaFile                   *file,
            GMountOperation                *start_op,
            GCancellable                   *cancellable,
            CajaFileOperationCallback   callback,
            gpointer                        callback_data);
    void                 (* stop)                    (CajaFile                   *file,
            GMountOperation                *mount_op,
            GCancellable                   *cancellable,
            CajaFileOperationCallback   callback,
            gpointer                        callback_data);

    void                 (* poll_for_media)          (CajaFile                   *file);
} CajaFileClass;

G_END_DECLS

#endif /* CAJA_FILE_H */
