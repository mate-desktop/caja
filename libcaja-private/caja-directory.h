/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-directory.h: Caja directory model.

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

#ifndef CAJA_DIRECTORY_H
#define CAJA_DIRECTORY_H

#include <gtk/gtk.h>
#include <gio/gio.h>

#include "caja-file-attributes.h"

G_BEGIN_DECLS

/* CajaDirectory is a class that manages the model for a directory,
   real or virtual, for Caja, mainly the file-manager component. The directory is
   responsible for managing both real data and cached metadata. On top of
   the file system independence provided by gio, the directory
   object also provides:

       1) A synchronization framework, which notifies via signals as the
          set of known files changes.
       2) An abstract interface for getting attributes and performing
          operations on files.
*/

#define CAJA_TYPE_DIRECTORY caja_directory_get_type()
#define CAJA_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_DIRECTORY, CajaDirectory))
#define CAJA_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_DIRECTORY, CajaDirectoryClass))
#define CAJA_IS_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_DIRECTORY))
#define CAJA_IS_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), CAJA_TYPE_DIRECTORY))
#define CAJA_DIRECTORY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), CAJA_TYPE_DIRECTORY, CajaDirectoryClass))

/* CajaFile is defined both here and in caja-file.h. */
#ifndef CAJA_FILE_DEFINED
#define CAJA_FILE_DEFINED
typedef struct CajaFile CajaFile;
#endif

typedef struct _CajaDirectoryPrivate CajaDirectoryPrivate;

typedef struct
{
    GObject object;
    CajaDirectoryPrivate *details;
} CajaDirectory;

typedef void (*CajaDirectoryCallback) (CajaDirectory *directory,
                                       GList             *files,
                                       gpointer           callback_data);

typedef struct
{
    GObjectClass parent_class;

    /*** Notification signals for clients to connect to. ***/

    /* The files_added signal is emitted as the directory model
     * discovers new files.
     */
    void     (* files_added)         (CajaDirectory          *directory,
                                      GList                      *added_files);

    /* The files_changed signal is emitted as changes occur to
     * existing files that are noticed by the synchronization framework,
     * including when an old file has been deleted. When an old file
     * has been deleted, this is the last chance to forget about these
     * file objects, which are about to be unref'd. Use a call to
     * caja_file_is_gone () to test for this case.
     */
    void     (* files_changed)       (CajaDirectory         *directory,
                                      GList                     *changed_files);

    /* The done_loading signal is emitted when a directory load
     * request completes. This is needed because, at least in the
     * case where the directory is empty, the caller will receive
     * no kind of notification at all when a directory load
     * initiated by `caja_directory_file_monitor_add' completes.
     */
    void     (* done_loading)        (CajaDirectory         *directory);

    void     (* load_error)          (CajaDirectory         *directory,
                                      GError                    *error);

    /*** Virtual functions for subclasses to override. ***/
    gboolean (* contains_file)       (CajaDirectory         *directory,
                                      CajaFile              *file);
    void     (* call_when_ready)     (CajaDirectory         *directory,
                                      CajaFileAttributes     file_attributes,
                                      gboolean                   wait_for_file_list,
                                      CajaDirectoryCallback  callback,
                                      gpointer                   callback_data);
    void     (* cancel_callback)     (CajaDirectory         *directory,
                                      CajaDirectoryCallback  callback,
                                      gpointer                   callback_data);
    void     (* file_monitor_add)    (CajaDirectory          *directory,
                                      gconstpointer              client,
                                      gboolean                   monitor_hidden_files,
                                      CajaFileAttributes     monitor_attributes,
                                      CajaDirectoryCallback  initial_files_callback,
                                      gpointer                   callback_data);
    void     (* file_monitor_remove) (CajaDirectory         *directory,
                                      gconstpointer              client);
    void     (* force_reload)        (CajaDirectory         *directory);
    gboolean (* are_all_files_seen)  (CajaDirectory         *directory);
    gboolean (* is_not_empty)        (CajaDirectory         *directory);
    char *	 (* get_name_for_self_as_new_file) (CajaDirectory *directory);

    /* get_file_list is a function pointer that subclasses may override to
     * customize collecting the list of files in a directory.
     * For example, the CajaDesktopDirectory overrides this so that it can
     * merge together the list of files in the $HOME/Desktop directory with
     * the list of standard icons (Computer, Home, Trash) on the desktop.
     */
    GList *	 (* get_file_list)	 (CajaDirectory *directory);

    /* Should return FALSE if the directory is read-only and doesn't
     * allow setting of metadata.
     * An example of this is the search directory.
     */
    gboolean (* is_editable)         (CajaDirectory *directory);
} CajaDirectoryClass;

/* Basic GObject requirements. */
GType              caja_directory_get_type                 (void);

/* Get a directory given a uri.
 * Creates the appropriate subclass given the uri mappings.
 * Returns a referenced object, not a floating one. Unref when finished.
 * If two windows are viewing the same uri, the directory object is shared.
 */
CajaDirectory *caja_directory_get                      (GFile                     *location);
CajaDirectory *caja_directory_get_by_uri               (const char                *uri);
CajaDirectory *caja_directory_get_for_file             (CajaFile              *file);

/* Covers for g_object_ref and g_object_unref that provide two conveniences:
 * 1) Using these is type safe.
 * 2) You are allowed to call these with NULL,
 */
CajaDirectory *caja_directory_ref                      (CajaDirectory         *directory);
void               caja_directory_unref                    (CajaDirectory         *directory);

/* Access to a URI. */
char *             caja_directory_get_uri                  (CajaDirectory         *directory);
GFile *            caja_directory_get_location             (CajaDirectory         *directory);

/* Is this file still alive and in this directory? */
gboolean           caja_directory_contains_file            (CajaDirectory         *directory,
        CajaFile              *file);

/* Get (and ref) a CajaFile object for this directory. */
CajaFile *     caja_directory_get_corresponding_file   (CajaDirectory         *directory);

/* Waiting for data that's read asynchronously.
 * The file attribute and metadata keys are for files in the directory.
 */
void               caja_directory_call_when_ready          (CajaDirectory         *directory,
        CajaFileAttributes     file_attributes,
        gboolean                   wait_for_all_files,
        CajaDirectoryCallback  callback,
        gpointer                   callback_data);
void               caja_directory_cancel_callback          (CajaDirectory         *directory,
        CajaDirectoryCallback  callback,
        gpointer                   callback_data);


/* Monitor the files in a directory. */
void               caja_directory_file_monitor_add         (CajaDirectory         *directory,
        gconstpointer              client,
        gboolean                   monitor_hidden_files,
        CajaFileAttributes     attributes,
        CajaDirectoryCallback  initial_files_callback,
        gpointer                   callback_data);
void               caja_directory_file_monitor_remove      (CajaDirectory         *directory,
        gconstpointer              client);
void               caja_directory_force_reload             (CajaDirectory         *directory);

/* Get a list of all files currently known in the directory. */
GList *            caja_directory_get_file_list            (CajaDirectory         *directory);

GList *            caja_directory_match_pattern            (CajaDirectory         *directory,
        const char *glob);


/* Return true if the directory has information about all the files.
 * This will be false until the directory has been read at least once.
 */
gboolean           caja_directory_are_all_files_seen       (CajaDirectory         *directory);

/* Return true if the directory is local. */
gboolean           caja_directory_is_local                 (CajaDirectory         *directory);

gboolean           caja_directory_is_in_trash              (CajaDirectory         *directory);

/* Return false if directory contains anything besides a Caja metafile.
 * Only valid if directory is monitored. Used by the Trash monitor.
 */
gboolean           caja_directory_is_not_empty             (CajaDirectory         *directory);

/* Convenience functions for dealing with a list of CajaDirectory objects that each have a ref.
 * These are just convenient names for functions that work on lists of GtkObject *.
 */
GList *            caja_directory_list_ref                 (GList                     *directory_list);
void               caja_directory_list_unref               (GList                     *directory_list);
void               caja_directory_list_free                (GList                     *directory_list);
GList *            caja_directory_list_copy                (GList                     *directory_list);
GList *            caja_directory_list_sort_by_uri         (GList                     *directory_list);

/* Fast way to check if a directory is the desktop directory */
gboolean           caja_directory_is_desktop_directory     (CajaDirectory         *directory);

gboolean           caja_directory_is_editable              (CajaDirectory         *directory);

G_END_DECLS

#endif /* CAJA_DIRECTORY_H */
