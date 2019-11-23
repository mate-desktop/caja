/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-directory.c: Caja directory model.

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

#include <config.h>
#include <gtk/gtk.h>
#include <string.h>

#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-macros.h>

#include "caja-directory-private.h"
#include "caja-directory-notify.h"
#include "caja-file-attributes.h"
#include "caja-file-private.h"
#include "caja-file-utilities.h"
#include "caja-search-directory.h"
#include "caja-global-preferences.h"
#include "caja-lib-self-check-functions.h"
#include "caja-metadata.h"
#include "caja-desktop-directory.h"
#include "caja-vfs-directory.h"

enum
{
    FILES_ADDED,
    FILES_CHANGED,
    DONE_LOADING,
    LOAD_ERROR,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static GHashTable *directories;

static void               caja_directory_finalize         (GObject                *object);
static CajaDirectory *caja_directory_new              (GFile                  *location);
static char *             real_get_name_for_self_as_new_file  (CajaDirectory      *directory);
static GList *            real_get_file_list                  (CajaDirectory      *directory);
static gboolean		  real_is_editable                    (CajaDirectory      *directory);
static void               set_directory_location              (CajaDirectory      *directory,
        GFile                  *location);

G_DEFINE_TYPE_WITH_PRIVATE (CajaDirectory, caja_directory, G_TYPE_OBJECT)

static void
caja_directory_class_init (CajaDirectoryClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = caja_directory_finalize;

    signals[FILES_ADDED] =
        g_signal_new ("files_added",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (CajaDirectoryClass, files_added),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__POINTER,
                      G_TYPE_NONE, 1, G_TYPE_POINTER);
    signals[FILES_CHANGED] =
        g_signal_new ("files_changed",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (CajaDirectoryClass, files_changed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__POINTER,
                      G_TYPE_NONE, 1, G_TYPE_POINTER);
    signals[DONE_LOADING] =
        g_signal_new ("done_loading",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (CajaDirectoryClass, done_loading),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
    signals[LOAD_ERROR] =
        g_signal_new ("load_error",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (CajaDirectoryClass, load_error),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__POINTER,
                      G_TYPE_NONE, 1, G_TYPE_POINTER);

    klass->get_name_for_self_as_new_file = real_get_name_for_self_as_new_file;
    klass->get_file_list = real_get_file_list;
    klass->is_editable = real_is_editable;
}

static void
caja_directory_init (CajaDirectory *directory)
{
    directory->details = caja_directory_get_instance_private (directory);
    directory->details->file_hash = g_hash_table_new (g_str_hash, g_str_equal);
    directory->details->high_priority_queue = caja_file_queue_new ();
    directory->details->low_priority_queue = caja_file_queue_new ();
    directory->details->extension_queue = caja_file_queue_new ();
    directory->details->free_space = (guint64)-1;
}

CajaDirectory *
caja_directory_ref (CajaDirectory *directory)
{
    if (directory == NULL)
    {
        return directory;
    }

    g_return_val_if_fail (CAJA_IS_DIRECTORY (directory), NULL);

    g_object_ref (directory);
    return directory;
}

void
caja_directory_unref (CajaDirectory *directory)
{
    if (directory == NULL)
    {
        return;
    }

    g_return_if_fail (CAJA_IS_DIRECTORY (directory));

    g_object_unref (directory);
}

static void
caja_directory_finalize (GObject *object)
{
    CajaDirectory *directory;

    directory = CAJA_DIRECTORY (object);

    g_hash_table_remove (directories, directory->details->location);

    caja_directory_cancel (directory);
    g_assert (directory->details->count_in_progress == NULL);
    g_assert (directory->details->top_left_read_state == NULL);

    if (directory->details->monitor_list != NULL)
    {
        g_warning ("destroying a CajaDirectory while it's being monitored");
        g_list_free_full (directory->details->monitor_list, g_free);
    }

    if (directory->details->monitor != NULL)
    {
        caja_monitor_cancel (directory->details->monitor);
    }

    if (directory->details->dequeue_pending_idle_id != 0)
    {
        g_source_remove (directory->details->dequeue_pending_idle_id);
    }

    if (directory->details->call_ready_idle_id != 0)
    {
        g_source_remove (directory->details->call_ready_idle_id);
    }

    if (directory->details->location)
    {
        g_object_unref (directory->details->location);
    }

    g_assert (directory->details->file_list == NULL);
    g_hash_table_destroy (directory->details->file_hash);

    caja_file_queue_destroy (directory->details->high_priority_queue);
    caja_file_queue_destroy (directory->details->low_priority_queue);
    caja_file_queue_destroy (directory->details->extension_queue);
    g_assert (directory->details->directory_load_in_progress == NULL);
    g_assert (directory->details->count_in_progress == NULL);
    g_assert (directory->details->dequeue_pending_idle_id == 0);
    g_list_free_full (directory->details->pending_file_info, g_object_unref);

    G_OBJECT_CLASS (caja_directory_parent_class)->finalize (object);
}

void
emit_change_signals_for_all_files (CajaDirectory *directory)
{
    GList *files;

    files = g_list_copy (directory->details->file_list);
    if (directory->details->as_file != NULL)
    {
        files = g_list_prepend (files, directory->details->as_file);
    }

    caja_file_list_ref (files);
    caja_directory_emit_change_signals (directory, files);

    caja_file_list_free (files);
}

static void
collect_all_directories (gpointer key, gpointer value, gpointer callback_data)
{
    CajaDirectory *directory;
    GList **dirs;

    directory = CAJA_DIRECTORY (value);
    dirs = callback_data;

    *dirs = g_list_prepend (*dirs, caja_directory_ref (directory));
}

void
emit_change_signals_for_all_files_in_all_directories (void)
{
    GList *dirs, *l;
    CajaDirectory *directory = NULL;

    dirs = NULL;
    g_hash_table_foreach (directories,
                          collect_all_directories,
                          &dirs);

    for (l = dirs; l != NULL; l = l->next)
    {
        directory = CAJA_DIRECTORY (l->data);
        emit_change_signals_for_all_files (directory);
        caja_directory_unref (directory);
    }

    g_list_free (dirs);
}

/**
 * caja_directory_get_by_uri:
 * @uri: URI of directory to get.
 *
 * Get a directory given a uri.
 * Creates the appropriate subclass given the uri mappings.
 * Returns a referenced object, not a floating one. Unref when finished.
 * If two windows are viewing the same uri, the directory object is shared.
 */
CajaDirectory *
caja_directory_get_internal (GFile *location, gboolean create)
{
    CajaDirectory *directory;

    /* Create the hash table first time through. */
    if (directories == NULL) {
        directories = g_hash_table_new (g_file_hash, (GCompareFunc) g_file_equal);
        caja_global_preferences_init ();
    }

    /* If the object is already in the hash table, look it up. */

    directory = g_hash_table_lookup (directories,
                                     location);
    if (directory != NULL)
    {
        caja_directory_ref (directory);
    }
    else if (create)
    {
        /* Create a new directory object instead. */
        directory = caja_directory_new (location);
        if (directory == NULL)
        {
            return NULL;
        }

        /* Put it in the hash table. */
        g_hash_table_insert (directories,
                             directory->details->location,
                             directory);
    }

    return directory;
}

CajaDirectory *
caja_directory_get (GFile *location)
{
    if (location == NULL)
    {
        return NULL;
    }

    return caja_directory_get_internal (location, TRUE);
}

CajaDirectory *
caja_directory_get_existing (GFile *location)
{
    if (location == NULL)
    {
        return NULL;
    }

    return caja_directory_get_internal (location, FALSE);
}


CajaDirectory *
caja_directory_get_by_uri (const char *uri)
{
    CajaDirectory *directory;
    GFile *location;

    if (uri == NULL)
    {
        return NULL;
    }

    location = g_file_new_for_uri (uri);

    directory = caja_directory_get_internal (location, TRUE);
    g_object_unref (location);
    return directory;
}

CajaDirectory *
caja_directory_get_for_file (CajaFile *file)
{
    char *uri;
    CajaDirectory *directory;

    g_return_val_if_fail (CAJA_IS_FILE (file), NULL);

    uri = caja_file_get_uri (file);
    directory = caja_directory_get_by_uri (uri);
    g_free (uri);
    return directory;
}

/* Returns a reffed CajaFile object for this directory.
 */
CajaFile *
caja_directory_get_corresponding_file (CajaDirectory *directory)
{
    CajaFile *file;

    file = caja_directory_get_existing_corresponding_file (directory);
    if (file == NULL)
    {
        char *uri;

        uri = caja_directory_get_uri (directory);
        file = caja_file_get_by_uri (uri);
        g_free (uri);
    }

    return file;
}

/* Returns a reffed CajaFile object for this directory, but only if the
 * CajaFile object has already been created.
 */
CajaFile *
caja_directory_get_existing_corresponding_file (CajaDirectory *directory)
{
    CajaFile *file;
    char *uri;

    file = directory->details->as_file;
    if (file != NULL)
    {
        caja_file_ref (file);
        return file;
    }

    uri = caja_directory_get_uri (directory);
    file = caja_file_get_existing_by_uri (uri);
    g_free (uri);
    return file;
}

/* caja_directory_get_name_for_self_as_new_file:
 *
 * Get a name to display for the file representing this
 * directory. This is called only when there's no VFS
 * directory for this CajaDirectory.
 */
char *
caja_directory_get_name_for_self_as_new_file (CajaDirectory *directory)
{
    g_return_val_if_fail (CAJA_IS_DIRECTORY (directory), NULL);

    if (CAJA_DIRECTORY_GET_CLASS(directory)->get_name_for_self_as_new_file == NULL)
            return NULL;
    else
            return CAJA_DIRECTORY_GET_CLASS(directory)->get_name_for_self_as_new_file (directory);
}

static char *
real_get_name_for_self_as_new_file (CajaDirectory *directory)
{
    char *directory_uri;
    char *name, *colon;

    directory_uri = caja_directory_get_uri (directory);

    colon = strchr (directory_uri, ':');
    if (colon == NULL || colon == directory_uri)
    {
        name = g_strdup (directory_uri);
    }
    else
    {
        name = g_strndup (directory_uri, colon - directory_uri);
    }
    g_free (directory_uri);

    return name;
}

char *
caja_directory_get_uri (CajaDirectory *directory)
{
    g_return_val_if_fail (CAJA_IS_DIRECTORY (directory), NULL);

    return g_file_get_uri (directory->details->location);
}

GFile *
caja_directory_get_location (CajaDirectory  *directory)
{
    g_return_val_if_fail (CAJA_IS_DIRECTORY (directory), NULL);

    return g_object_ref (directory->details->location);
}

static CajaDirectory *
caja_directory_new (GFile *location)
{
    CajaDirectory *directory;
    char *uri;

    uri = g_file_get_uri (location);

    if (eel_uri_is_desktop (uri))
    {
        directory = CAJA_DIRECTORY (g_object_new (CAJA_TYPE_DESKTOP_DIRECTORY, NULL));
    }
    else if (eel_uri_is_search (uri))
    {
        directory = CAJA_DIRECTORY (g_object_new (CAJA_TYPE_SEARCH_DIRECTORY, NULL));
    }
    else if (g_str_has_suffix (uri, CAJA_SAVED_SEARCH_EXTENSION))
    {
        directory = CAJA_DIRECTORY (caja_search_directory_new_from_saved_search (uri));
    }
    else
    {
        directory = CAJA_DIRECTORY (g_object_new (CAJA_TYPE_VFS_DIRECTORY, NULL));
    }

    set_directory_location (directory, location);

    g_free (uri);

    return directory;
}

gboolean
caja_directory_is_local (CajaDirectory *directory)
{
    g_return_val_if_fail (CAJA_IS_DIRECTORY (directory), FALSE);

    if (directory->details->location == NULL)
    {
        return TRUE;
    }

    return caja_directory_is_in_trash (directory) ||
           g_file_is_native (directory->details->location);
}

gboolean
caja_directory_is_in_trash (CajaDirectory *directory)
{
    g_assert (CAJA_IS_DIRECTORY (directory));

    if (directory->details->location == NULL)
    {
        return FALSE;
    }

    return g_file_has_uri_scheme (directory->details->location, "trash");
}

gboolean
caja_directory_are_all_files_seen (CajaDirectory *directory)
{
    g_return_val_if_fail (CAJA_IS_DIRECTORY (directory), FALSE);

    if (CAJA_DIRECTORY_GET_CLASS(directory)->are_all_files_seen == NULL)
    {
        return FALSE;
    } else {
        return CAJA_DIRECTORY_GET_CLASS(directory)->are_all_files_seen (directory);
    }
}

static void
add_to_hash_table (CajaDirectory *directory, CajaFile *file, GList *node)
{
    const char *name;

    name = file->details->name;

    g_assert (node != NULL);
    g_assert (g_hash_table_lookup (directory->details->file_hash,
                                   name) == NULL);
    g_hash_table_insert (directory->details->file_hash, (char *) name, node);
}

static GList *
extract_from_hash_table (CajaDirectory *directory, CajaFile *file)
{
    const char *name;
    GList *node;

    name = file->details->name;
    if (name == NULL)
    {
        return NULL;
    }

    /* Find the list node in the hash table. */
    node = g_hash_table_lookup (directory->details->file_hash, name);
    g_hash_table_remove (directory->details->file_hash, name);

    return node;
}

void
caja_directory_add_file (CajaDirectory *directory, CajaFile *file)
{
    GList *node;
    gboolean add_to_work_queue;

    g_assert (CAJA_IS_DIRECTORY (directory));
    g_assert (CAJA_IS_FILE (file));
    g_assert (file->details->name != NULL);

    /* Add to list. */
    node = g_list_prepend (directory->details->file_list, file);
    directory->details->file_list = node;

    /* Add to hash table. */
    add_to_hash_table (directory, file, node);

    directory->details->confirmed_file_count++;

    add_to_work_queue = FALSE;
    if (caja_directory_is_file_list_monitored (directory))
    {
        /* Ref if we are monitoring, since monitoring owns the file list. */
        caja_file_ref (file);
        add_to_work_queue = TRUE;
    }
    else if (caja_directory_has_active_request_for_file (directory, file))
    {
        /* We're waiting for the file in a call_when_ready. Make sure
           we add the file to the work queue so that said waiter won't
           wait forever for e.g. all files in the directory to be done */
        add_to_work_queue = TRUE;
    }

    if (add_to_work_queue)
    {
        caja_directory_add_file_to_work_queue (directory, file);
    }
}

void
caja_directory_remove_file (CajaDirectory *directory, CajaFile *file)
{
    GList *node;

    g_assert (CAJA_IS_DIRECTORY (directory));
    g_assert (CAJA_IS_FILE (file));
    g_assert (file->details->name != NULL);

    /* Find the list node in the hash table. */
    node = extract_from_hash_table (directory, file);
    g_assert (node != NULL);
    g_assert (node->data == file);

    /* Remove the item from the list. */
    directory->details->file_list = g_list_remove_link
                                    (directory->details->file_list, node);
    g_list_free_1 (node);

    caja_directory_remove_file_from_work_queue (directory, file);

    if (!file->details->unconfirmed)
    {
        directory->details->confirmed_file_count--;
    }

    /* Unref if we are monitoring. */
    if (caja_directory_is_file_list_monitored (directory))
    {
        caja_file_unref (file);
    }
}

GList *
caja_directory_begin_file_name_change (CajaDirectory *directory,
                                       CajaFile *file)
{
    /* Find the list node in the hash table. */
    return extract_from_hash_table (directory, file);
}

void
caja_directory_end_file_name_change (CajaDirectory *directory,
                                     CajaFile *file,
                                     GList *node)
{
    /* Add the list node to the hash table. */
    if (node != NULL)
    {
        add_to_hash_table (directory, file, node);
    }
}

CajaFile *
caja_directory_find_file_by_name (CajaDirectory *directory,
                                  const char *name)
{
    GList *node;

    g_return_val_if_fail (CAJA_IS_DIRECTORY (directory), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    node = g_hash_table_lookup (directory->details->file_hash,
                                name);
    return node == NULL ? NULL : CAJA_FILE (node->data);
}

void
caja_directory_emit_files_added (CajaDirectory *directory,
                                 GList *added_files)
{
    if (added_files != NULL)
    {
        g_signal_emit (directory,
                       signals[FILES_ADDED], 0,
                       added_files);
    }
}

void
caja_directory_emit_files_changed (CajaDirectory *directory,
                                   GList *changed_files)
{
    if (changed_files != NULL)
    {
        g_signal_emit (directory,
                       signals[FILES_CHANGED], 0,
                       changed_files);
    }
}

void
caja_directory_emit_change_signals (CajaDirectory *directory,
                                    GList *changed_files)
{
    GList *p;

    for (p = changed_files; p != NULL; p = p->next)
    {
        caja_file_emit_changed (p->data);
    }
    caja_directory_emit_files_changed (directory, changed_files);
}

void
caja_directory_emit_done_loading (CajaDirectory *directory)
{
    g_signal_emit (directory,
                   signals[DONE_LOADING], 0);
}

void
caja_directory_emit_load_error (CajaDirectory *directory,
                                GError *error)
{
    g_signal_emit (directory,
                   signals[LOAD_ERROR], 0,
                   error);
}

/* Return a directory object for this one's parent. */
static CajaDirectory *
get_parent_directory (GFile *location)
{
    GFile *parent;

    parent = g_file_get_parent (location);
    if (parent)
    {
        CajaDirectory *directory;

        directory = caja_directory_get_internal (parent, TRUE);
        g_object_unref (parent);
        return directory;
    }
    return NULL;
}

/* If a directory object exists for this one's parent, then
 * return it, otherwise return NULL.
 */
static CajaDirectory *
get_parent_directory_if_exists (GFile *location)
{
    GFile *parent;

    parent = g_file_get_parent (location);
    if (parent)
    {
        CajaDirectory *directory;

        directory = caja_directory_get_internal (parent, FALSE);
        g_object_unref (parent);
        return directory;
    }
    return NULL;
}

static void
hash_table_list_prepend (GHashTable *table, gconstpointer key, gpointer data)
{
    GList *list;

    list = g_hash_table_lookup (table, key);
    list = g_list_prepend (list, data);
    g_hash_table_insert (table, (gpointer) key, list);
}

static void
call_files_added_free_list (gpointer key, gpointer value, gpointer user_data)
{
    g_assert (CAJA_IS_DIRECTORY (key));
    g_assert (value != NULL);
    g_assert (user_data == NULL);

    g_signal_emit (key,
                   signals[FILES_ADDED], 0,
                   value);
    g_list_free (value);
}

static void
call_files_changed_common (CajaDirectory *directory, GList *file_list)
{
    GList *node;
    CajaFile *file = NULL;

    for (node = file_list; node != NULL; node = node->next)
    {
        file = node->data;
        if (file->details->directory == directory)
        {
            caja_directory_add_file_to_work_queue (directory,
                                                   file);
        }
    }
    caja_directory_async_state_changed (directory);
    caja_directory_emit_change_signals (directory, file_list);
}

static void
call_files_changed_free_list (gpointer key, gpointer value, gpointer user_data)
{
    g_assert (value != NULL);
    g_assert (user_data == NULL);

    call_files_changed_common (CAJA_DIRECTORY (key), value);
    g_list_free (value);
}

static void
call_files_changed_unref_free_list (gpointer key, gpointer value, gpointer user_data)
{
    g_assert (value != NULL);
    g_assert (user_data == NULL);

    call_files_changed_common (CAJA_DIRECTORY (key), value);
    caja_file_list_free (value);
}

static void
call_get_file_info_free_list (gpointer key, gpointer value, gpointer user_data)
{
    CajaDirectory *directory;
    GList *files;

    g_assert (CAJA_IS_DIRECTORY (key));
    g_assert (value != NULL);
    g_assert (user_data == NULL);

    directory = key;
    files = value;

    caja_directory_get_info_for_new_files (directory, files);
    g_list_free_full (files, g_object_unref);
}

static void
invalidate_count_and_unref (gpointer key, gpointer value, gpointer user_data)
{
    g_assert (CAJA_IS_DIRECTORY (key));
    g_assert (value == key);
    g_assert (user_data == NULL);

    caja_directory_invalidate_count_and_mime_list (key);
    caja_directory_unref (key);
}

static void
collect_parent_directories (GHashTable *hash_table, CajaDirectory *directory)
{
    g_assert (hash_table != NULL);
    g_assert (CAJA_IS_DIRECTORY (directory));

    if (g_hash_table_lookup (hash_table, directory) == NULL)
    {
        caja_directory_ref (directory);
        g_hash_table_insert  (hash_table, directory, directory);
    }
}

void
caja_directory_notify_files_added (GList *files)
{
    GHashTable *added_lists;
    GList *p;
    GHashTable *parent_directories;
    CajaFile *file;
    GFile *parent;
    CajaDirectory *directory = NULL;
    GFile *location = NULL;

    /* Make a list of added files in each directory. */
    added_lists = g_hash_table_new (NULL, NULL);

    /* Make a list of parent directories that will need their counts updated. */
    parent_directories = g_hash_table_new (NULL, NULL);

    for (p = files; p != NULL; p = p->next)
    {
        location = p->data;

        /* See if the directory is already known. */
        directory = get_parent_directory_if_exists (location);
        if (directory == NULL)
        {
            /* In case the directory is not being
             * monitored, but the corresponding file is,
             * we must invalidate it's item count.
             */


            file = NULL;
            parent = g_file_get_parent (location);
            if (parent)
            {
                file = caja_file_get_existing (parent);
                g_object_unref (parent);
            }

            if (file != NULL)
            {
                caja_file_invalidate_count_and_mime_list (file);
                caja_file_unref (file);
            }

            continue;
        }

        collect_parent_directories (parent_directories, directory);

        /* If no one is monitoring files in the directory, nothing to do. */
        if (!caja_directory_is_file_list_monitored (directory))
        {
            caja_directory_unref (directory);
            continue;
        }

        file = caja_file_get_existing (location);
        /* We check is_added here, because the file could have been added
         * to the directory by a caja_file_get() but not gotten
         * files_added emitted
         */
        if (file && file->details->is_added)
        {
            /* A file already exists, it was probably renamed.
             * If it was renamed this could be ignored, but
             * queue a change just in case */
            caja_file_changed (file);
            caja_file_unref (file);
        }
        else
        {
            hash_table_list_prepend (added_lists,
                                     directory,
                                     g_object_ref (location));
        }
        caja_directory_unref (directory);
    }

    /* Now get file info for the new files. This creates CajaFile
     * objects for the new files, and sends out a files_added signal.
     */
    g_hash_table_foreach (added_lists, call_get_file_info_free_list, NULL);
    g_hash_table_destroy (added_lists);

    /* Invalidate count for each parent directory. */
    g_hash_table_foreach (parent_directories, invalidate_count_and_unref, NULL);
    g_hash_table_destroy (parent_directories);
}

void
caja_directory_notify_files_changed (GList *files)
{
    GHashTable *changed_lists;
    GList *node;
    GFile *location = NULL;
    CajaFile *file = NULL;

    /* Make a list of changed files in each directory. */
    changed_lists = g_hash_table_new (NULL, NULL);

    /* Go through all the notifications. */
    for (node = files; node != NULL; node = node->next)
    {
        location = node->data;

        /* Find the file. */
        file = caja_file_get_existing (location);
        if (file != NULL)
        {
            /* Tell it to re-get info now, and later emit
             * a changed signal.
             */
            file->details->file_info_is_up_to_date = FALSE;
            file->details->top_left_text_is_up_to_date = FALSE;
            file->details->link_info_is_up_to_date = FALSE;
            caja_file_invalidate_extension_info_internal (file);

            hash_table_list_prepend (changed_lists,
                                     file->details->directory,
                                     file);
        }
    }
    /* Now send out the changed signals. */
    g_hash_table_foreach (changed_lists, call_files_changed_unref_free_list, NULL);
    g_hash_table_destroy (changed_lists);
}

void
caja_directory_notify_files_removed (GList *files)
{
    GHashTable *changed_lists;
    GList *p;
    GHashTable *parent_directories;
    CajaDirectory *directory = NULL;
    CajaFile *file = NULL;
    GFile *location = NULL;

    /* Make a list of changed files in each directory. */
    changed_lists = g_hash_table_new (NULL, NULL);

    /* Make a list of parent directories that will need their counts updated. */
    parent_directories = g_hash_table_new (NULL, NULL);

    /* Go through all the notifications. */
    for (p = files; p != NULL; p = p->next)
    {
        location = p->data;

        /* Update file count for parent directory if anyone might care. */
        directory = get_parent_directory_if_exists (location);
        if (directory != NULL)
        {
            collect_parent_directories (parent_directories, directory);
            caja_directory_unref (directory);
        }

        /* Find the file. */
        file = caja_file_get_existing (location);
        if (file != NULL && !caja_file_rename_in_progress (file))
        {
            /* Mark it gone and prepare to send the changed signal. */
            caja_file_mark_gone (file);
            hash_table_list_prepend (changed_lists,
                                     file->details->directory,
                                     caja_file_ref (file));
        }
        caja_file_unref (file);
    }
    /* Now send out the changed signals. */
    g_hash_table_foreach (changed_lists, call_files_changed_unref_free_list, NULL);
    g_hash_table_destroy (changed_lists);

    /* Invalidate count for each parent directory. */
    g_hash_table_foreach (parent_directories, invalidate_count_and_unref, NULL);
    g_hash_table_destroy (parent_directories);
}

static void
set_directory_location (CajaDirectory *directory,
                        GFile *location)
{
    if (directory->details->location)
    {
        g_object_unref (directory->details->location);
    }
    directory->details->location = g_object_ref (location);

}

static void
change_directory_location (CajaDirectory *directory,
                           GFile *new_location)
{
    /* I believe it's impossible for a self-owned file/directory
     * to be moved. But if that did somehow happen, this function
     * wouldn't do enough to handle it.
     */
    g_assert (directory->details->as_file == NULL);

    g_hash_table_remove (directories,
                         directory->details->location);

    set_directory_location (directory, new_location);

    g_hash_table_insert (directories,
                         directory->details->location,
                         directory);
}

typedef struct
{
    GFile *container;
    GList *directories;
} CollectData;

static void
collect_directories_by_container (gpointer key, gpointer value, gpointer callback_data)
{
    CajaDirectory *directory;
    CollectData *collect_data;
    GFile *location;

    location = (GFile *) key;
    directory = CAJA_DIRECTORY (value);
    collect_data = (CollectData *) callback_data;

    if (g_file_has_prefix (location, collect_data->container) ||
            g_file_equal (collect_data->container, location))
    {
        caja_directory_ref (directory);
        collect_data->directories =
            g_list_prepend (collect_data->directories,
                            directory);
    }
}

static GList *
caja_directory_moved_internal (GFile *old_location,
                               GFile *new_location)
{
    CollectData collection;
    GList *node, *affected_files;
    char *relative_path;
    CajaDirectory *directory = NULL;
    GFile *new_directory_location = NULL;

    collection.container = old_location;
    collection.directories = NULL;

    g_hash_table_foreach (directories,
                          collect_directories_by_container,
                          &collection);

    affected_files = NULL;

    for (node = collection.directories; node != NULL; node = node->next)
    {
        directory = CAJA_DIRECTORY (node->data);
        new_directory_location = NULL;

        if (g_file_equal (directory->details->location, old_location))
        {
            new_directory_location = g_object_ref (new_location);
        }
        else
        {
            relative_path = g_file_get_relative_path (old_location,
                            directory->details->location);
            if (relative_path != NULL)
            {
                new_directory_location = g_file_resolve_relative_path (new_location, relative_path);
                g_free (relative_path);

            }
        }

        if (new_directory_location)
        {
            change_directory_location (directory, new_directory_location);
            g_object_unref (new_directory_location);

            /* Collect affected files. */
            if (directory->details->as_file != NULL)
            {
                affected_files = g_list_prepend
                                 (affected_files,
                                  caja_file_ref (directory->details->as_file));
            }
            affected_files = g_list_concat
                             (affected_files,
                              caja_file_list_copy (directory->details->file_list));
        }

        caja_directory_unref (directory);
    }

    g_list_free (collection.directories);

    return affected_files;
}

void
caja_directory_moved (const char *old_uri,
                      const char *new_uri)
{
    GList *list, *node;
    GHashTable *hash;
    GFile *old_location;
    GFile *new_location;
    CajaFile *file = NULL;

    hash = g_hash_table_new (NULL, NULL);

    old_location = g_file_new_for_uri (old_uri);
    new_location = g_file_new_for_uri (new_uri);

    list = caja_directory_moved_internal (old_location, new_location);
    for (node = list; node != NULL; node = node->next)
    {
        file = CAJA_FILE (node->data);
        hash_table_list_prepend (hash,
                                 file->details->directory,
                                 caja_file_ref (file));
    }
    caja_file_list_free (list);

    g_object_unref (old_location);
    g_object_unref (new_location);

    g_hash_table_foreach (hash, call_files_changed_unref_free_list, NULL);
    g_hash_table_destroy (hash);
}

void
caja_directory_notify_files_moved (GList *file_pairs)
{
    GList *p, *affected_files, *node;
    GFilePair *pair;
    CajaFile *file;
    CajaDirectory *old_directory, *new_directory;
    GHashTable *parent_directories;
    GList *new_files_list, *unref_list;
    GHashTable *added_lists, *changed_lists;
    char *name;
    CajaFileAttributes cancel_attributes;
    GFile *to_location = NULL;
    GFile *from_location = NULL;

    /* Make a list of added and changed files in each directory. */
    new_files_list = NULL;
    added_lists = g_hash_table_new (NULL, NULL);
    changed_lists = g_hash_table_new (NULL, NULL);
    unref_list = NULL;

    /* Make a list of parent directories that will need their counts updated. */
    parent_directories = g_hash_table_new (NULL, NULL);

    cancel_attributes = caja_file_get_all_attributes ();

    for (p = file_pairs; p != NULL; p = p->next)
    {
        pair = p->data;
        from_location = pair->from;
        to_location = pair->to;

        /* Handle overwriting a file. */
        file = caja_file_get_existing (to_location);
        if (file != NULL)
        {
            /* Mark it gone and prepare to send the changed signal. */
            caja_file_mark_gone (file);
            new_directory = file->details->directory;
            hash_table_list_prepend (changed_lists,
                                     new_directory,
                                     file);
            collect_parent_directories (parent_directories,
                                        new_directory);
        }

        /* Update any directory objects that are affected. */
        affected_files = caja_directory_moved_internal (from_location,
                         to_location);
        for (node = affected_files; node != NULL; node = node->next)
        {
            file = CAJA_FILE (node->data);
            hash_table_list_prepend (changed_lists,
                                     file->details->directory,
                                     file);
        }
        unref_list = g_list_concat (unref_list, affected_files);

        /* Move an existing file. */
        file = caja_file_get_existing (from_location);
        if (file == NULL)
        {
            /* Handle this as if it was a new file. */
            new_files_list = g_list_prepend (new_files_list,
                                             to_location);
        }
        else
        {
            /* Handle notification in the old directory. */
            old_directory = file->details->directory;
            collect_parent_directories (parent_directories, old_directory);

            /* Cancel loading of attributes in the old directory */
            caja_directory_cancel_loading_file_attributes
            (old_directory, file, cancel_attributes);

            /* Locate the new directory. */
            new_directory = get_parent_directory (to_location);
            collect_parent_directories (parent_directories, new_directory);
            /* We can unref now -- new_directory is in the
             * parent directories list so it will be
             * around until the end of this function
             * anyway.
             */
            caja_directory_unref (new_directory);

            /* Update the file's name and directory. */
            name = g_file_get_basename (to_location);
            caja_file_update_name_and_directory
            (file, name, new_directory);
            g_free (name);

            /* Update file attributes */
            caja_file_invalidate_attributes (file, CAJA_FILE_ATTRIBUTE_INFO);

            hash_table_list_prepend (changed_lists,
                                     old_directory,
                                     file);
            if (old_directory != new_directory)
            {
                hash_table_list_prepend	(added_lists,
                                         new_directory,
                                         file);
            }

            /* Unref each file once to balance out caja_file_get_by_uri. */
            unref_list = g_list_prepend (unref_list, file);
        }
    }

    /* Now send out the changed and added signals for existing file objects. */
    g_hash_table_foreach (changed_lists, call_files_changed_free_list, NULL);
    g_hash_table_destroy (changed_lists);
    g_hash_table_foreach (added_lists, call_files_added_free_list, NULL);
    g_hash_table_destroy (added_lists);

    /* Let the file objects go. */
    caja_file_list_free (unref_list);

    /* Invalidate count for each parent directory. */
    g_hash_table_foreach (parent_directories, invalidate_count_and_unref, NULL);
    g_hash_table_destroy (parent_directories);

    /* Separate handling for brand new file objects. */
    caja_directory_notify_files_added (new_files_list);
    g_list_free (new_files_list);
}

void
caja_directory_schedule_position_set (GList *position_setting_list)
{
    GList *p;
    char str[64];
    time_t now;
    const CajaFileChangesQueuePosition *item = NULL;
    CajaFile *file = NULL;

    time (&now);

    for (p = position_setting_list; p != NULL; p = p->next)
    {
        item = (CajaFileChangesQueuePosition *) p->data;

        file = caja_file_get (item->location);

        if (item->set)
        {
            g_snprintf (str, sizeof (str), "%d,%d", item->point.x, item->point.y);
        }
        else
        {
            str[0] = 0;
        }
        caja_file_set_metadata
        (file,
         CAJA_METADATA_KEY_ICON_POSITION,
         NULL,
         str);

        if (item->set)
        {
            caja_file_set_time_metadata
            (file,
             CAJA_METADATA_KEY_ICON_POSITION_TIMESTAMP,
             now);
        }
        else
        {
            caja_file_set_time_metadata
            (file,
             CAJA_METADATA_KEY_ICON_POSITION_TIMESTAMP,
             UNDEFINED_TIME);
        }

        if (item->set)
        {
            g_snprintf (str, sizeof (str), "%d", item->screen);
        }
        else
        {
            str[0] = 0;
        }
        caja_file_set_metadata
        (file,
         CAJA_METADATA_KEY_SCREEN,
         NULL,
         str);

        caja_file_unref (file);
    }
}

gboolean
caja_directory_contains_file (CajaDirectory *directory,
                              CajaFile *file)
{
    g_return_val_if_fail (CAJA_IS_DIRECTORY (directory), FALSE);
    g_return_val_if_fail (CAJA_IS_FILE (file), FALSE);

    if (caja_file_is_gone (file))
    {
        return FALSE;
    }

    if (CAJA_DIRECTORY_GET_CLASS(directory)->contains_file == NULL)
        return FALSE;
    else
        return CAJA_DIRECTORY_GET_CLASS(directory)->contains_file (directory, file);
}

void
caja_directory_call_when_ready (CajaDirectory *directory,
                                CajaFileAttributes file_attributes,
                                gboolean wait_for_all_files,
                                CajaDirectoryCallback callback,
                                gpointer callback_data)
{
    g_return_if_fail (CAJA_IS_DIRECTORY (directory));
    g_return_if_fail (callback != NULL);

    if (CAJA_DIRECTORY_GET_CLASS(directory)->call_when_ready != NULL)
    {
        CAJA_DIRECTORY_GET_CLASS(directory)->call_when_ready (directory,
                                                              file_attributes,
                                                              wait_for_all_files,
                                                              callback,
                                                              callback_data);
    }
}

void
caja_directory_cancel_callback (CajaDirectory *directory,
                                CajaDirectoryCallback callback,
                                gpointer callback_data)
{
    g_return_if_fail (CAJA_IS_DIRECTORY (directory));
    g_return_if_fail (callback != NULL);

    if (CAJA_DIRECTORY_GET_CLASS(directory)->cancel_callback != NULL)
    {
        CAJA_DIRECTORY_GET_CLASS(directory)->cancel_callback (directory, callback, callback_data);
    }
}

void
caja_directory_file_monitor_add (CajaDirectory *directory,
                                 gconstpointer client,
                                 gboolean monitor_hidden_files,
                                 CajaFileAttributes file_attributes,
                                 CajaDirectoryCallback callback,
                                 gpointer callback_data)
{
    g_return_if_fail (CAJA_IS_DIRECTORY (directory));
    g_return_if_fail (client != NULL);

    if (CAJA_DIRECTORY_GET_CLASS(directory)->file_monitor_add != NULL)
    {
        CAJA_DIRECTORY_GET_CLASS(directory)->file_monitor_add (directory,
                                                               client,
                                                               monitor_hidden_files,
                                                               file_attributes,
                                                               callback, callback_data);
    }
}

void
caja_directory_file_monitor_remove (CajaDirectory *directory,
                                    gconstpointer client)
{
    g_return_if_fail (CAJA_IS_DIRECTORY (directory));
    g_return_if_fail (client != NULL);

    if (CAJA_DIRECTORY_GET_CLASS(directory)->file_monitor_remove != NULL)
    {
        CAJA_DIRECTORY_GET_CLASS(directory)->file_monitor_remove (directory, client);
    }
}

void
caja_directory_force_reload (CajaDirectory *directory)
{
    g_return_if_fail (CAJA_IS_DIRECTORY (directory));

    if (CAJA_DIRECTORY_GET_CLASS(directory)->force_reload != NULL)
    {
        CAJA_DIRECTORY_GET_CLASS(directory)->force_reload (directory);
    }
}

gboolean
caja_directory_is_not_empty (CajaDirectory *directory)
{
    g_return_val_if_fail (CAJA_IS_DIRECTORY (directory), FALSE);

    if (CAJA_DIRECTORY_GET_CLASS(directory)->is_not_empty == NULL)
    {
        return FALSE;
    } else {
        return CAJA_DIRECTORY_GET_CLASS(directory)->is_not_empty (directory);
    }
}

static gboolean
is_tentative (gpointer data, gpointer callback_data)
{
    CajaFile *file;

    g_assert (callback_data == NULL);

    file = CAJA_FILE (data);
    /* Avoid returning files with !is_added, because these
     * will later be sent with the files_added signal, and a
     * user doing get_file_list + files_added monitoring will
     * then see the file twice */
    return !file->details->got_file_info || !file->details->is_added;
}

GList *
caja_directory_get_file_list (CajaDirectory *directory)
{
    if (CAJA_DIRECTORY_GET_CLASS(directory)->get_file_list == NULL)
    {
        return NULL;
    } else {
        return CAJA_DIRECTORY_GET_CLASS(directory)->get_file_list (directory);
    }
}

static GList *
real_get_file_list (CajaDirectory *directory)
{
    GList *tentative_files, *non_tentative_files;

    tentative_files = eel_g_list_partition
                      (g_list_copy (directory->details->file_list),
                       is_tentative, NULL, &non_tentative_files);
    g_list_free (tentative_files);

    caja_file_list_ref (non_tentative_files);
    return non_tentative_files;
}

static gboolean
real_is_editable (CajaDirectory *directory)
{
    return TRUE;
}

gboolean
caja_directory_is_editable (CajaDirectory *directory)
{
    if (CAJA_DIRECTORY_GET_CLASS(directory)->is_editable == NULL)
    {
        return FALSE;
    } else {
        return CAJA_DIRECTORY_GET_CLASS(directory)->is_editable (directory);
    }
}

GList *
caja_directory_match_pattern (CajaDirectory *directory, const char *pattern)
{
    GList *files, *l, *ret;
    GPatternSpec *spec;


    ret = NULL;
    spec = g_pattern_spec_new (pattern);

    files = caja_directory_get_file_list (directory);
    for (l = files; l; l = l->next)
    {
        CajaFile *file;
        char *name;

        file = CAJA_FILE (l->data);
        name = caja_file_get_display_name (file);

        if (g_pattern_match_string (spec, name))
        {
            ret = g_list_prepend(ret, caja_file_ref (file));
        }

        g_free (name);
    }

    g_pattern_spec_free (spec);
    caja_file_list_free (files);

    return ret;
}

/**
 * caja_directory_list_ref
 *
 * Ref all the directories in a list.
 * @list: GList of directories.
 **/
GList *
caja_directory_list_ref (GList *list)
{
    g_list_foreach (list, (GFunc) caja_directory_ref, NULL);
    return list;
}

/**
 * caja_directory_list_unref
 *
 * Unref all the directories in a list.
 * @list: GList of directories.
 **/
void
caja_directory_list_unref (GList *list)
{
    g_list_foreach (list, (GFunc) caja_directory_unref, NULL);
}

/**
 * caja_directory_list_free
 *
 * Free a list of directories after unrefing them.
 * @list: GList of directories.
 **/
void
caja_directory_list_free (GList *list)
{
    caja_directory_list_unref (list);
    g_list_free (list);
}

/**
 * caja_directory_list_copy
 *
 * Copy the list of directories, making a new ref of each,
 * @list: GList of directories.
 **/
GList *
caja_directory_list_copy (GList *list)
{
    return g_list_copy (caja_directory_list_ref (list));
}

static int
compare_by_uri (CajaDirectory *a, CajaDirectory *b)
{
    char *uri_a, *uri_b;
    int res;

    uri_a = g_file_get_uri (a->details->location);
    uri_b = g_file_get_uri (b->details->location);

    res = strcmp (uri_a, uri_b);

    g_free (uri_a);
    g_free (uri_b);

    return res;
}

static int
compare_by_uri_cover (gconstpointer a, gconstpointer b)
{
    return compare_by_uri (CAJA_DIRECTORY (a), CAJA_DIRECTORY (b));
}

/**
 * caja_directory_list_sort_by_uri
 *
 * Sort the list of directories by directory uri.
 * @list: GList of directories.
 **/
GList *
caja_directory_list_sort_by_uri (GList *list)
{
    return g_list_sort (list, compare_by_uri_cover);
}

gboolean
caja_directory_is_desktop_directory (CajaDirectory   *directory)
{
    if (directory->details->location == NULL)
    {
        return FALSE;
    }

    return caja_is_desktop_directory (directory->details->location);
}

#if !defined (CAJA_OMIT_SELF_CHECK)

#include <eel/eel-debug.h>
#include "caja-file-attributes.h"

static int data_dummy;
static gboolean got_files_flag;

static void
got_files_callback (CajaDirectory *directory, GList *files, gpointer callback_data)
{
    g_assert (CAJA_IS_DIRECTORY (directory));
    g_assert (g_list_length (files) > 10);
    g_assert (callback_data == &data_dummy);

    got_files_flag = TRUE;
}

/* Return the number of extant CajaDirectories */
int
caja_directory_number_outstanding (void)
{
    return directories ? g_hash_table_size (directories) : 0;
}

void
caja_self_check_directory (void)
{
    CajaDirectory *directory;
    CajaFile *file;

    directory = caja_directory_get_by_uri ("file:///etc");
    file = caja_file_get_by_uri ("file:///etc/passwd");

    EEL_CHECK_INTEGER_RESULT (g_hash_table_size (directories), 1);

    caja_directory_file_monitor_add
    (directory, &data_dummy,
     TRUE, 0, NULL, NULL);

    /* FIXME: these need to be updated to the new metadata infrastructure
     *  as make check doesn't pass.
    caja_file_set_metadata (file, "test", "default", "value");
    EEL_CHECK_STRING_RESULT (caja_file_get_metadata (file, "test", "default"), "value");

    caja_file_set_boolean_metadata (file, "test_boolean", TRUE, TRUE);
    EEL_CHECK_BOOLEAN_RESULT (caja_file_get_boolean_metadata (file, "test_boolean", TRUE), TRUE);
    caja_file_set_boolean_metadata (file, "test_boolean", TRUE, FALSE);
    EEL_CHECK_BOOLEAN_RESULT (caja_file_get_boolean_metadata (file, "test_boolean", TRUE), FALSE);
    EEL_CHECK_BOOLEAN_RESULT (caja_file_get_boolean_metadata (NULL, "test_boolean", TRUE), TRUE);

    caja_file_set_integer_metadata (file, "test_integer", 0, 17);
    EEL_CHECK_INTEGER_RESULT (caja_file_get_integer_metadata (file, "test_integer", 0), 17);
    caja_file_set_integer_metadata (file, "test_integer", 0, -1);
    EEL_CHECK_INTEGER_RESULT (caja_file_get_integer_metadata (file, "test_integer", 0), -1);
    caja_file_set_integer_metadata (file, "test_integer", 42, 42);
    EEL_CHECK_INTEGER_RESULT (caja_file_get_integer_metadata (file, "test_integer", 42), 42);
    EEL_CHECK_INTEGER_RESULT (caja_file_get_integer_metadata (NULL, "test_integer", 42), 42);
    EEL_CHECK_INTEGER_RESULT (caja_file_get_integer_metadata (file, "nonexistent_key", 42), 42);
    */

    EEL_CHECK_BOOLEAN_RESULT (caja_directory_get_by_uri ("file:///etc") == directory, TRUE);
    caja_directory_unref (directory);

    EEL_CHECK_BOOLEAN_RESULT (caja_directory_get_by_uri ("file:///etc/") == directory, TRUE);
    caja_directory_unref (directory);

    EEL_CHECK_BOOLEAN_RESULT (caja_directory_get_by_uri ("file:///etc////") == directory, TRUE);
    caja_directory_unref (directory);

    caja_file_unref (file);

    caja_directory_file_monitor_remove (directory, &data_dummy);

    caja_directory_unref (directory);

    while (g_hash_table_size (directories) != 0)
    {
        gtk_main_iteration ();
    }

    EEL_CHECK_INTEGER_RESULT (g_hash_table_size (directories), 0);

    directory = caja_directory_get_by_uri ("file:///etc");

    got_files_flag = FALSE;

    caja_directory_call_when_ready (directory,
                                    CAJA_FILE_ATTRIBUTE_INFO |
                                    CAJA_FILE_ATTRIBUTE_DEEP_COUNTS,
                                    TRUE,
                                    got_files_callback, &data_dummy);

    while (!got_files_flag)
    {
        gtk_main_iteration ();
    }

    EEL_CHECK_BOOLEAN_RESULT (directory->details->file_list == NULL, TRUE);

    EEL_CHECK_INTEGER_RESULT (g_hash_table_size (directories), 1);

    file = caja_file_get_by_uri ("file:///etc/passwd");

    /* EEL_CHECK_STRING_RESULT (caja_file_get_metadata (file, "test", "default"), "value"); */

    caja_file_unref (file);

    caja_directory_unref (directory);

    EEL_CHECK_INTEGER_RESULT (g_hash_table_size (directories), 0);
}

#endif /* !CAJA_OMIT_SELF_CHECK */
