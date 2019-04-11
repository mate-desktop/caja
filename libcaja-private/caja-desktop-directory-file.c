/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-desktop-directory-file.c: Subclass of CajaFile to help implement the
   virtual desktop.

   Copyright (C) 2003 Red Hat, Inc.

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

   Author: Alexander Larsson <alexl@redhat.com>
*/

#include <config.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>

#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-macros.h>

#include "caja-desktop-directory-file.h"
#include "caja-desktop-metadata.h"
#include "caja-directory-notify.h"
#include "caja-directory-private.h"
#include "caja-file-attributes.h"
#include "caja-file-private.h"
#include "caja-file-utilities.h"
#include "caja-desktop-directory.h"
#include "caja-metadata.h"

struct CajaDesktopDirectoryFileDetails
{
    CajaDesktopDirectory *desktop_directory;

    CajaFile *real_dir_file;

    GHashTable *callbacks;
    GHashTable *monitors;
};

typedef struct
{
    CajaDesktopDirectoryFile *desktop_file;
    CajaFileCallback callback;
    gpointer callback_data;

    CajaFileAttributes delegated_attributes;
    CajaFileAttributes non_delegated_attributes;

    GList *non_ready_files;

    gboolean initializing;
} DesktopCallback;

typedef struct
{
    CajaDesktopDirectoryFile *desktop_file;

    CajaFileAttributes delegated_attributes;
    CajaFileAttributes non_delegated_attributes;
} DesktopMonitor;


static void caja_desktop_directory_file_init       (gpointer   object,
        gpointer   klass);
static void caja_desktop_directory_file_class_init (gpointer   klass);

EEL_CLASS_BOILERPLATE (CajaDesktopDirectoryFile,
                       caja_desktop_directory_file,
                       CAJA_TYPE_FILE)

static guint
desktop_callback_hash (gconstpointer desktop_callback_as_pointer)
{
    const DesktopCallback *desktop_callback;

    desktop_callback = desktop_callback_as_pointer;
    return GPOINTER_TO_UINT (desktop_callback->callback)
           ^ GPOINTER_TO_UINT (desktop_callback->callback_data);
}

static gboolean
desktop_callback_equal (gconstpointer desktop_callback_as_pointer,
                        gconstpointer desktop_callback_as_pointer_2)
{
    const DesktopCallback *desktop_callback, *desktop_callback_2;

    desktop_callback = desktop_callback_as_pointer;
    desktop_callback_2 = desktop_callback_as_pointer_2;

    return desktop_callback->callback == desktop_callback_2->callback
           && desktop_callback->callback_data == desktop_callback_2->callback_data;
}


static void
real_file_changed_callback (CajaFile *real_file,
                            gpointer callback_data)
{
    CajaDesktopDirectoryFile *desktop_file;

    desktop_file = CAJA_DESKTOP_DIRECTORY_FILE (callback_data);
    caja_file_changed (CAJA_FILE (desktop_file));
}

static CajaFileAttributes
get_delegated_attributes_mask (void)
{
    return CAJA_FILE_ATTRIBUTE_DEEP_COUNTS |
           CAJA_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT |
           CAJA_FILE_ATTRIBUTE_DIRECTORY_ITEM_MIME_TYPES;
}

static void
partition_attributes (CajaFileAttributes attributes,
                      CajaFileAttributes *delegated_attributes,
                      CajaFileAttributes *non_delegated_attributes)
{
    CajaFileAttributes mask;

    mask = get_delegated_attributes_mask ();

    *delegated_attributes = attributes & mask;
    *non_delegated_attributes = attributes & ~mask;
}

static void
desktop_directory_file_monitor_add (CajaFile *file,
                                    gconstpointer client,
                                    CajaFileAttributes attributes)
{
    CajaDesktopDirectoryFile *desktop_file;
    DesktopMonitor *monitor;

    desktop_file = CAJA_DESKTOP_DIRECTORY_FILE (file);

    /* Map the client to a unique value so this doesn't interfere
     * with direct monitoring of the file by the same client.
     */
    monitor = g_hash_table_lookup (desktop_file->details->monitors, client);
    if (monitor != NULL)
    {
        g_assert (monitor->desktop_file == desktop_file);
    }
    else
    {
        monitor = g_new0 (DesktopMonitor, 1);
        monitor->desktop_file = desktop_file;
        g_hash_table_insert (desktop_file->details->monitors,
                             (gpointer) client, monitor);
    }

    partition_attributes (attributes,
                          &monitor->delegated_attributes,
                          &monitor->non_delegated_attributes);

    /* Pawn off partioned attributes to real dir file */
    caja_file_monitor_add (desktop_file->details->real_dir_file,
                           monitor, monitor->delegated_attributes);

    /* Do the rest ourself */
    caja_directory_monitor_add_internal
    (file->details->directory, file,
     client, TRUE,
     monitor->non_delegated_attributes,
     NULL, NULL);
}

static void
desktop_directory_file_monitor_remove (CajaFile *file,
                                       gconstpointer client)
{
    CajaDesktopDirectoryFile *desktop_file;
    DesktopMonitor *monitor;

    desktop_file = CAJA_DESKTOP_DIRECTORY_FILE (file);

    /* Map the client to the value used by the earlier add call. */
    monitor = g_hash_table_lookup (desktop_file->details->monitors, client);
    if (monitor == NULL)
    {
        return;
    }

    /* Call through to the real file remove calls. */
    g_hash_table_remove (desktop_file->details->monitors, client);

    /* Remove the locally handled parts */
    caja_directory_monitor_remove_internal
    (file->details->directory, file, client);
}

static void
desktop_callback_destroy (DesktopCallback *desktop_callback)
{
    g_assert (desktop_callback != NULL);
    g_assert (CAJA_IS_DESKTOP_DIRECTORY_FILE (desktop_callback->desktop_file));

    caja_file_unref (CAJA_FILE (desktop_callback->desktop_file));
    g_list_free (desktop_callback->non_ready_files);
    g_free (desktop_callback);
}

static void
desktop_callback_check_done (DesktopCallback *desktop_callback)
{
    /* Check if we are ready. */
    if (desktop_callback->initializing ||
            desktop_callback->non_ready_files != NULL)
    {
        return;
    }

    /* Ensure our metadata is updated before calling back. */
    caja_desktop_update_metadata_from_keyfile(CAJA_FILE (desktop_callback->desktop_file),
                                               "directory");

    /* Remove from the hash table before sending it. */
    g_hash_table_remove (desktop_callback->desktop_file->details->callbacks,
                         desktop_callback);

    /* We are ready, so do the real callback. */
    (* desktop_callback->callback) (CAJA_FILE (desktop_callback->desktop_file),
                                    desktop_callback->callback_data);

    /* And we are done. */
    desktop_callback_destroy (desktop_callback);
}

static void
desktop_callback_remove_file (DesktopCallback *desktop_callback,
                              CajaFile *file)
{
    desktop_callback->non_ready_files = g_list_remove
                                        (desktop_callback->non_ready_files, file);
    desktop_callback_check_done (desktop_callback);
}

static void
ready_callback (CajaFile *file,
                gpointer callback_data)
{
    DesktopCallback *desktop_callback;

    g_assert (CAJA_IS_FILE (file));
    g_assert (callback_data != NULL);

    desktop_callback = callback_data;
    g_assert (g_list_find (desktop_callback->non_ready_files, file) != NULL);

    desktop_callback_remove_file (desktop_callback, file);
}

static void
desktop_directory_file_call_when_ready (CajaFile *file,
                                        CajaFileAttributes attributes,
                                        CajaFileCallback callback,
                                        gpointer callback_data)

{
    CajaDesktopDirectoryFile *desktop_file;
    DesktopCallback search_key, *desktop_callback;

    desktop_file = CAJA_DESKTOP_DIRECTORY_FILE (file);

    /* Check to be sure we aren't overwriting. */
    search_key.callback = callback;
    search_key.callback_data = callback_data;
    if (g_hash_table_lookup (desktop_file->details->callbacks, &search_key) != NULL)
    {
        g_warning ("tried to add a new callback while an old one was pending");
        return;
    }

    /* Create a desktop_callback record. */
    desktop_callback = g_new0 (DesktopCallback, 1);
    caja_file_ref (file);
    desktop_callback->desktop_file = desktop_file;
    desktop_callback->callback = callback;
    desktop_callback->callback_data = callback_data;
    desktop_callback->initializing = TRUE;

    partition_attributes (attributes,
                          &desktop_callback->delegated_attributes,
                          &desktop_callback->non_delegated_attributes);

    desktop_callback->non_ready_files = g_list_prepend
                                        (desktop_callback->non_ready_files, file);
    desktop_callback->non_ready_files = g_list_prepend
                                        (desktop_callback->non_ready_files, desktop_file->details->real_dir_file);

    /* Put it in the hash table. */
    g_hash_table_insert (desktop_file->details->callbacks,
                         desktop_callback, desktop_callback);

    /* Now connect to each file's call_when_ready. */
    caja_directory_call_when_ready_internal
    (file->details->directory, file,
     desktop_callback->non_delegated_attributes,
     FALSE, NULL, ready_callback, desktop_callback);
    caja_file_call_when_ready
    (desktop_file->details->real_dir_file,
     desktop_callback->delegated_attributes,
     ready_callback, desktop_callback);

    desktop_callback->initializing = FALSE;

    /* Check if any files became read while we were connecting up
     * the call_when_ready callbacks (also handles the pathological
     * case where there are no files at all).
     */
    desktop_callback_check_done (desktop_callback);

}

static void
desktop_directory_file_cancel_call_when_ready (CajaFile *file,
        CajaFileCallback callback,
        gpointer callback_data)
{
    CajaDesktopDirectoryFile *desktop_file;
    DesktopCallback search_key, *desktop_callback;

    desktop_file = CAJA_DESKTOP_DIRECTORY_FILE (file);

    /* Find the entry in the table. */
    search_key.callback = callback;
    search_key.callback_data = callback_data;
    desktop_callback = g_hash_table_lookup (desktop_file->details->callbacks, &search_key);
    if (desktop_callback == NULL)
    {
        return;
    }

    /* Remove from the hash table before working with it. */
    g_hash_table_remove (desktop_callback->desktop_file->details->callbacks, desktop_callback);

    /* Tell the real directory to cancel the call. */
    caja_directory_cancel_callback_internal
    (file->details->directory, file,
     NULL, ready_callback, desktop_callback);

    caja_file_cancel_call_when_ready
    (desktop_file->details->real_dir_file,
     ready_callback, desktop_callback);

    desktop_callback_destroy (desktop_callback);
}

static gboolean
real_check_if_ready (CajaFile *file,
                     CajaFileAttributes attributes)
{
    return caja_directory_check_if_ready_internal
           (file->details->directory, file,
            attributes);
}

static gboolean
desktop_directory_file_check_if_ready (CajaFile *file,
                                       CajaFileAttributes attributes)
{
    CajaFileAttributes delegated_attributes, non_delegated_attributes;
    CajaDesktopDirectoryFile *desktop_file;

    desktop_file = CAJA_DESKTOP_DIRECTORY_FILE (file);

    partition_attributes (attributes,
                          &delegated_attributes,
                          &non_delegated_attributes);

    return real_check_if_ready (file, non_delegated_attributes) &&
           caja_file_check_if_ready (desktop_file->details->real_dir_file,
                                     delegated_attributes);
}

static gboolean
desktop_directory_file_get_item_count (CajaFile *file,
                                       guint *count,
                                       gboolean *count_unreadable)
{
    CajaDesktopDirectoryFile *desktop_file;
    gboolean got_count;

    desktop_file = CAJA_DESKTOP_DIRECTORY_FILE (file);

    got_count = caja_file_get_directory_item_count (desktop_file->details->real_dir_file,
                count,
                count_unreadable);

    if (count)
    {
        *count += g_list_length (file->details->directory->details->file_list);
    }

    return got_count;
}

static CajaRequestStatus
desktop_directory_file_get_deep_counts (CajaFile *file,
                                        guint *directory_count,
                                        guint *file_count,
                                        guint *unreadable_directory_count,
                                        goffset *total_size,
                                        goffset *total_size_on_disk)
{
    CajaDesktopDirectoryFile *desktop_file;
    CajaRequestStatus status;

    desktop_file = CAJA_DESKTOP_DIRECTORY_FILE (file);

    status = caja_file_get_deep_counts (desktop_file->details->real_dir_file,
                                        directory_count,
                                        file_count,
                                        unreadable_directory_count,
                                        total_size,
                                        total_size_on_disk,
                                        TRUE);

    if (file_count)
    {
        *file_count += g_list_length (file->details->directory->details->file_list);
    }

    return status;
}

static gboolean
desktop_directory_file_get_date (CajaFile *file,
                                 CajaDateType date_type,
                                 time_t *date)
{
    CajaDesktopDirectoryFile *desktop_file;

    desktop_file = CAJA_DESKTOP_DIRECTORY_FILE (file);

    return caja_file_get_date (desktop_file->details->real_dir_file,
                               date_type,
                               date);
}

static char *
desktop_directory_file_get_where_string (CajaFile *file)
{
    return g_strdup (_("on the desktop"));
}


static void
monitor_destroy (gpointer data)
{
    DesktopMonitor *monitor = data;

    caja_file_monitor_remove
    (CAJA_FILE (monitor->desktop_file->details->real_dir_file), monitor);
    g_free (monitor);
}

static void
caja_desktop_directory_file_set_metadata (CajaFile           *file,
        const char             *key,
        const char             *value)
{
    caja_desktop_set_metadata_string (file, "directory", key, value);
}

static void
caja_desktop_directory_file_set_metadata_as_list (CajaFile           *file,
        const char             *key,
        char                  **value)
{
    caja_desktop_set_metadata_stringv (file, "directory", key, (const gchar **) value);
}

static void
caja_desktop_directory_file_init (gpointer object, gpointer klass)
{
    CajaDesktopDirectoryFile *desktop_file;
    CajaDesktopDirectory *desktop_directory;
    CajaDirectory *real_dir;
    CajaFile *real_dir_file;

    desktop_file = CAJA_DESKTOP_DIRECTORY_FILE (object);

    desktop_directory = CAJA_DESKTOP_DIRECTORY (caja_directory_get_by_uri (EEL_DESKTOP_URI));

    desktop_file->details = g_new0 (CajaDesktopDirectoryFileDetails, 1);
    desktop_file->details->desktop_directory = desktop_directory;

    desktop_file->details->callbacks = g_hash_table_new
                                       (desktop_callback_hash, desktop_callback_equal);
    desktop_file->details->monitors = g_hash_table_new_full (NULL, NULL,
                                      NULL, monitor_destroy);

    real_dir = caja_desktop_directory_get_real_directory (desktop_directory);
    real_dir_file = caja_directory_get_corresponding_file (real_dir);
    caja_directory_unref (real_dir);

    desktop_file->details->real_dir_file = real_dir_file;

    g_signal_connect_object (real_dir_file, "changed",
                             G_CALLBACK (real_file_changed_callback), desktop_file, 0);
}


static void
desktop_callback_remove_file_cover (gpointer key,
                                    gpointer value,
                                    gpointer callback_data)
{
    desktop_callback_remove_file
    (value, CAJA_FILE (callback_data));
}


static void
desktop_finalize (GObject *object)
{
    CajaDesktopDirectoryFile *desktop_file;
    CajaDesktopDirectory *desktop_directory;

    desktop_file = CAJA_DESKTOP_DIRECTORY_FILE (object);
    desktop_directory = desktop_file->details->desktop_directory;

    /* Todo: ghash now safe? */
    eel_g_hash_table_safe_for_each
    (desktop_file->details->callbacks,
     desktop_callback_remove_file_cover,
     desktop_file->details->real_dir_file);

    if (g_hash_table_size (desktop_file->details->callbacks) != 0)
    {
        g_warning ("call_when_ready still pending when desktop virtual file is destroyed");
    }

    g_hash_table_destroy (desktop_file->details->callbacks);
    g_hash_table_destroy (desktop_file->details->monitors);

    caja_file_unref (desktop_file->details->real_dir_file);

    g_free (desktop_file->details);

    caja_directory_unref (CAJA_DIRECTORY (desktop_directory));

    EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
caja_desktop_directory_file_class_init (gpointer klass)
{
    GObjectClass *object_class;
    CajaFileClass *file_class;

    object_class = G_OBJECT_CLASS (klass);
    file_class = CAJA_FILE_CLASS (klass);

    object_class->finalize = desktop_finalize;

    file_class->default_file_type = G_FILE_TYPE_DIRECTORY;

    file_class->monitor_add = desktop_directory_file_monitor_add;
    file_class->monitor_remove = desktop_directory_file_monitor_remove;
    file_class->call_when_ready = desktop_directory_file_call_when_ready;
    file_class->cancel_call_when_ready = desktop_directory_file_cancel_call_when_ready;
    file_class->check_if_ready = desktop_directory_file_check_if_ready;
    file_class->get_item_count = desktop_directory_file_get_item_count;
    file_class->get_deep_counts = desktop_directory_file_get_deep_counts;
    file_class->get_date = desktop_directory_file_get_date;
    file_class->get_where_string = desktop_directory_file_get_where_string;
    file_class->set_metadata = caja_desktop_directory_file_set_metadata;
    file_class->set_metadata_as_list = caja_desktop_directory_file_set_metadata_as_list;
}
