/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Caja
 *
 * Copyright (C) 1999, 2000 Eazel, Inc.
 *
 * Caja is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Caja is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors: John Sullivan <sullivan@eazel.com>
 */

/* caja-bookmark-list.c - implementation of centralized list of bookmarks.
 */

#include <config.h>
#include <string.h>

#include <gio/gio.h>

#include <libcaja-private/caja-file-utilities.h>
#include <libcaja-private/caja-file.h>
#include <libcaja-private/caja-icon-names.h>

#include "caja-bookmark-list.h"

#define LOAD_JOB 1
#define SAVE_JOB 2

enum
{
    CONTENTS_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };
static char *window_geometry;
static CajaBookmarkList *singleton = NULL;

/* forward declarations */

static void        caja_bookmark_list_load_file     (CajaBookmarkList *bookmarks);
static void        caja_bookmark_list_save_file     (CajaBookmarkList *bookmarks);

G_DEFINE_TYPE(CajaBookmarkList, caja_bookmark_list, G_TYPE_OBJECT)

static CajaBookmark *
new_bookmark_from_uri (const char *uri, const char *label)
{
    CajaBookmark *new_bookmark;
    char *name;
    gboolean has_label;
    GFile *location;
    gboolean native;

    location = NULL;
    if (uri)
    {
        location = g_file_new_for_uri (uri);
    }

    has_label = FALSE;
    if (!label)
    {
        name = caja_compute_title_for_location (location);
    }
    else
    {
        name = g_strdup (label);
        has_label = TRUE;
    }

    new_bookmark = NULL;

    if (uri)
    {
        CajaFile *file;
        GIcon *icon;

        native = g_file_is_native (location);
        file = caja_file_get (location);

        icon = NULL;

        if (caja_file_check_if_ready (file,
                                      CAJA_FILE_ATTRIBUTES_FOR_ICON))
        {
            icon = caja_file_get_gicon (file, 0);
        }
        caja_file_unref (file);

        if (icon == NULL)
        {
            icon = native ? g_themed_icon_new (CAJA_ICON_FOLDER) :
                   g_themed_icon_new (CAJA_ICON_FOLDER_REMOTE);
        }

        new_bookmark = caja_bookmark_new (location, name, has_label, icon);

        g_object_unref (icon);

    }
    g_free (name);
    g_object_unref (location);
    return new_bookmark;
}

static GFile *
caja_bookmark_list_get_legacy_file (void)
{
    char *filename;
    GFile *file;

    filename = g_build_filename (g_get_home_dir (),
                                 ".gtk-bookmarks",
                                 NULL);
    file = g_file_new_for_path (filename);

    g_free (filename);

    return file;
}

static GFile *
caja_bookmark_list_get_file (void)
{
    char *filename;
    GFile *file;

    filename = g_build_filename (g_get_user_config_dir (),
                                 "gtk-3.0",
                                 "bookmarks",
                                 NULL);
    file = g_file_new_for_path (filename);

    g_free (filename);

    return file;
}

/* Initialization.  */

static void
bookmark_in_list_changed_callback (CajaBookmark     *bookmark,
                                   CajaBookmarkList *bookmarks)
{
    g_assert (CAJA_IS_BOOKMARK (bookmark));
    g_assert (CAJA_IS_BOOKMARK_LIST (bookmarks));

    /* Save changes so we'll have the good icon next time. */
    caja_bookmark_list_save_file (bookmarks);
}

static void
stop_monitoring_bookmark (CajaBookmarkList *bookmarks,
                          CajaBookmark     *bookmark)
{
    g_signal_handlers_disconnect_by_func (bookmark,
                                          bookmark_in_list_changed_callback,
                                          bookmarks);
}

static void
stop_monitoring_one (gpointer data, gpointer user_data)
{
    g_assert (CAJA_IS_BOOKMARK (data));
    g_assert (CAJA_IS_BOOKMARK_LIST (user_data));

    stop_monitoring_bookmark (CAJA_BOOKMARK_LIST (user_data),
                              CAJA_BOOKMARK (data));
}

static void
clear (CajaBookmarkList *bookmarks)
{
    g_list_foreach (bookmarks->list, stop_monitoring_one, bookmarks);
    g_list_free_full (bookmarks->list, g_object_unref);
    bookmarks->list = NULL;
}

static void
do_finalize (GObject *object)
{
    if (CAJA_BOOKMARK_LIST (object)->monitor != NULL)
    {
        g_file_monitor_cancel (CAJA_BOOKMARK_LIST (object)->monitor);
        CAJA_BOOKMARK_LIST (object)->monitor = NULL;
    }

    g_queue_free (CAJA_BOOKMARK_LIST (object)->pending_ops);

    clear (CAJA_BOOKMARK_LIST (object));

    G_OBJECT_CLASS (caja_bookmark_list_parent_class)->finalize (object);
}

static GObject *
do_constructor (GType type,
                guint n_construct_params,
                GObjectConstructParam *construct_params)
{
    GObject *retval;

    if (singleton != NULL)
    {
        return g_object_ref (G_OBJECT (singleton));
    }

    retval = G_OBJECT_CLASS (caja_bookmark_list_parent_class)->constructor
             (type, n_construct_params, construct_params);

    singleton = CAJA_BOOKMARK_LIST (retval);
    g_object_add_weak_pointer (retval, (gpointer) &singleton);

    return retval;
}


static void
caja_bookmark_list_class_init (CajaBookmarkListClass *class)
{
    GObjectClass *object_class = G_OBJECT_CLASS (class);

    object_class->finalize = do_finalize;
    object_class->constructor = do_constructor;

    signals[CONTENTS_CHANGED] =
        g_signal_new ("contents_changed",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (CajaBookmarkListClass,
                                       contents_changed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
}

static void
bookmark_monitor_changed_cb (GFileMonitor      *monitor,
                             GFile             *child,
                             GFile             *other_file,
                             GFileMonitorEvent  eflags,
                             gpointer           user_data)
{
    if (eflags == G_FILE_MONITOR_EVENT_CHANGED ||
            eflags == G_FILE_MONITOR_EVENT_CREATED)
    {
        g_return_if_fail (CAJA_IS_BOOKMARK_LIST (CAJA_BOOKMARK_LIST (user_data)));
        caja_bookmark_list_load_file (CAJA_BOOKMARK_LIST (user_data));
    }
}

static void
caja_bookmark_list_init (CajaBookmarkList *bookmarks)
{
    GFile *file;

    bookmarks->pending_ops = g_queue_new ();

    caja_bookmark_list_load_file (bookmarks);

    file = caja_bookmark_list_get_file ();
    bookmarks->monitor = g_file_monitor_file (file, 0, NULL, NULL);
    g_file_monitor_set_rate_limit (bookmarks->monitor, 1000);

    g_signal_connect (bookmarks->monitor, "changed",
                      G_CALLBACK (bookmark_monitor_changed_cb), bookmarks);

    g_object_unref (file);
}

static void
insert_bookmark_internal (CajaBookmarkList *bookmarks,
                          CajaBookmark     *bookmark,
                          int                   index)
{
    bookmarks->list = g_list_insert (bookmarks->list, bookmark, index);

    g_signal_connect_object (bookmark, "contents_changed",
                             G_CALLBACK (bookmark_in_list_changed_callback), bookmarks, 0);
}

/**
 * caja_bookmark_list_append:
 *
 * Append a bookmark to a bookmark list.
 * @bookmarks: CajaBookmarkList to append to.
 * @bookmark: Bookmark to append a copy of.
 **/
void
caja_bookmark_list_append (CajaBookmarkList *bookmarks,
                           CajaBookmark     *bookmark)
{
    g_return_if_fail (CAJA_IS_BOOKMARK_LIST (bookmarks));
    g_return_if_fail (CAJA_IS_BOOKMARK (bookmark));

    insert_bookmark_internal (bookmarks,
                              caja_bookmark_copy (bookmark),
                              -1);

    caja_bookmark_list_save_file (bookmarks);
}

/**
 * caja_bookmark_list_contains:
 *
 * Check whether a bookmark with matching name and url is already in the list.
 * @bookmarks: CajaBookmarkList to check contents of.
 * @bookmark: CajaBookmark to match against.
 *
 * Return value: TRUE if matching bookmark is in list, FALSE otherwise
 **/
gboolean
caja_bookmark_list_contains (CajaBookmarkList *bookmarks,
                             CajaBookmark     *bookmark)
{
    g_return_val_if_fail (CAJA_IS_BOOKMARK_LIST (bookmarks), FALSE);
    g_return_val_if_fail (CAJA_IS_BOOKMARK (bookmark), FALSE);

    return g_list_find_custom (bookmarks->list,
                               (gpointer)bookmark,
                               caja_bookmark_compare_with)
           != NULL;
}

/**
 * caja_bookmark_list_delete_item_at:
 *
 * Delete the bookmark at the specified position.
 * @bookmarks: the list of bookmarks.
 * @index: index, must be less than length of list.
 **/
void
caja_bookmark_list_delete_item_at (CajaBookmarkList *bookmarks,
                                   guint                 index)
{
    GList *doomed;

    g_return_if_fail (CAJA_IS_BOOKMARK_LIST (bookmarks));
    g_return_if_fail (index < g_list_length (bookmarks->list));

    doomed = g_list_nth (bookmarks->list, index);
    g_return_if_fail (doomed != NULL);

    bookmarks->list = g_list_remove_link (bookmarks->list, doomed);

    g_assert (CAJA_IS_BOOKMARK (doomed->data));
    stop_monitoring_bookmark (bookmarks, CAJA_BOOKMARK (doomed->data));
    g_object_unref (doomed->data);

    g_list_free_1 (doomed);

    caja_bookmark_list_save_file (bookmarks);
}

/**
 * caja_bookmark_list_move_item:
 *
 * Move the item from the given position to the destination.
 * @index: the index of the first bookmark.
 * @destination: the index of the second bookmark.
 **/
void
caja_bookmark_list_move_item (CajaBookmarkList *bookmarks,
                              guint index,
                              guint destination)
{
    GList *bookmark_item;

    if (index == destination)
    {
        return;
    }

    bookmark_item = g_list_nth (bookmarks->list, index);
    g_return_if_fail (bookmark_item != NULL);

    bookmarks->list = g_list_remove_link (bookmarks->list,
                                          bookmark_item);

    if (index < destination)
    {
        bookmarks->list = g_list_insert (bookmarks->list,
                                         bookmark_item->data,
                                         destination - 1);
    }
    else
    {
        bookmarks->list = g_list_insert (bookmarks->list,
                                         bookmark_item->data,
                                         destination);
    }

    caja_bookmark_list_save_file (bookmarks);
}

/**
 * caja_bookmark_list_delete_items_with_uri:
 *
 * Delete all bookmarks with the given uri.
 * @bookmarks: the list of bookmarks.
 * @uri: The uri to match.
 **/
void
caja_bookmark_list_delete_items_with_uri (CajaBookmarkList *bookmarks,
        const char           *uri)
{
    GList *node, *next;
    gboolean list_changed;

    g_return_if_fail (CAJA_IS_BOOKMARK_LIST (bookmarks));
    g_return_if_fail (uri != NULL);

    list_changed = FALSE;
    for (node = bookmarks->list; node != NULL;  node = next)
    {
        char *bookmark_uri;

        next = node->next;

        bookmark_uri = caja_bookmark_get_uri (CAJA_BOOKMARK (node->data));
        if (g_strcmp0 (bookmark_uri, uri) == 0)
        {
            bookmarks->list = g_list_remove_link (bookmarks->list, node);
            stop_monitoring_bookmark (bookmarks, CAJA_BOOKMARK (node->data));
            g_object_unref (node->data);
            g_list_free_1 (node);
            list_changed = TRUE;
        }
        g_free (bookmark_uri);
    }

    if (list_changed)
    {
        caja_bookmark_list_save_file (bookmarks);
    }
}

/**
 * caja_bookmark_list_get_window_geometry:
 *
 * Get a string representing the bookmark_list's window's geometry.
 * This is the value set earlier by caja_bookmark_list_set_window_geometry.
 * @bookmarks: the list of bookmarks associated with the window.
 * Return value: string representation of window's geometry, suitable for
 * passing to mate_parse_geometry(), or NULL if
 * no window geometry has yet been saved for this bookmark list.
 **/
const char *
caja_bookmark_list_get_window_geometry (CajaBookmarkList *bookmarks)
{
    return window_geometry;
}

/**
 * caja_bookmark_list_insert_item:
 *
 * Insert a bookmark at a specified position.
 * @bookmarks: the list of bookmarks.
 * @index: the position to insert the bookmark at.
 * @new_bookmark: the bookmark to insert a copy of.
 **/
void
caja_bookmark_list_insert_item (CajaBookmarkList *bookmarks,
                                CajaBookmark     *new_bookmark,
                                guint                 index)
{
    g_return_if_fail (CAJA_IS_BOOKMARK_LIST (bookmarks));
    g_return_if_fail (index <= g_list_length (bookmarks->list));

    insert_bookmark_internal (bookmarks,
                              caja_bookmark_copy (new_bookmark),
                              index);

    caja_bookmark_list_save_file (bookmarks);
}

/**
 * caja_bookmark_list_item_at:
 *
 * Get the bookmark at the specified position.
 * @bookmarks: the list of bookmarks.
 * @index: index, must be less than length of list.
 *
 * Return value: the bookmark at position @index in @bookmarks.
 **/
CajaBookmark *
caja_bookmark_list_item_at (CajaBookmarkList *bookmarks, guint index)
{
    g_return_val_if_fail (CAJA_IS_BOOKMARK_LIST (bookmarks), NULL);
    g_return_val_if_fail (index < g_list_length (bookmarks->list), NULL);

    return CAJA_BOOKMARK (g_list_nth_data (bookmarks->list, index));
}

/**
 * caja_bookmark_list_length:
 *
 * Get the number of bookmarks in the list.
 * @bookmarks: the list of bookmarks.
 *
 * Return value: the length of the bookmark list.
 **/
guint
caja_bookmark_list_length (CajaBookmarkList *bookmarks)
{
    g_return_val_if_fail (CAJA_IS_BOOKMARK_LIST(bookmarks), 0);

    return g_list_length (bookmarks->list);
}

static void
load_file_finish (CajaBookmarkList *bookmarks,
                  GObject *source,
                  GAsyncResult *res)
{
    GError *error = NULL;
    gchar *contents = NULL;

    g_file_load_contents_finish (G_FILE (source),
                                 res, &contents, NULL, NULL, &error);

    if (error == NULL)
    {
        char **lines;
        int i;

        lines = g_strsplit (contents, "\n", -1);
        for (i = 0; lines[i]; i++)
        {
            /* Ignore empty or invalid lines that cannot be parsed properly */
            if (lines[i][0] != '\0' && lines[i][0] != ' ')
            {
                /* gtk 2.7/2.8 might have labels appended to bookmarks which are separated by a space */
                /* we must seperate the bookmark uri and the potential label */
                char *space, *label;

                label = NULL;
                space = strchr (lines[i], ' ');
                if (space)
                {
                    *space = '\0';
                    label = g_strdup (space + 1);
                }
                insert_bookmark_internal (bookmarks,
                                          new_bookmark_from_uri (lines[i], label),
                                          -1);

                g_free (label);
            }
        }
        g_free (contents);
        g_strfreev (lines);

        g_signal_emit (bookmarks, signals[CONTENTS_CHANGED], 0);
    }
    else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
    {
        g_warning ("Could not load bookmark file: %s\n", error->message);
        g_error_free (error);
    }
}

static void
load_file_async (CajaBookmarkList *self,
                 GAsyncReadyCallback callback)
{
    GFile *file;

    file = caja_bookmark_list_get_file ();
    if (!g_file_query_exists (file, NULL)) {
            file = caja_bookmark_list_get_legacy_file ();
    }

    /* Wipe out old list. */
    clear (self);

    /* keep the bookmark list alive */
    g_object_ref (self);
    g_file_load_contents_async (file, NULL, callback, self);

    g_object_unref (file);
}

static void
save_file_finish (CajaBookmarkList *bookmarks,
                  GObject *source,
                  GAsyncResult *res)
{
    GError *error = NULL;
    GFile *file;

    g_file_replace_contents_finish (G_FILE (source),
                                    res, NULL, &error);

    if (error != NULL)
    {
        g_warning ("Unable to replace contents of the bookmarks file: %s",
                   error->message);
        g_error_free (error);
    }

    file = caja_bookmark_list_get_file ();

    /* re-enable bookmark file monitoring */
    bookmarks->monitor = g_file_monitor_file (file, 0, NULL, NULL);
    g_file_monitor_set_rate_limit (bookmarks->monitor, 1000);
    g_signal_connect (bookmarks->monitor, "changed",
                      G_CALLBACK (bookmark_monitor_changed_cb), bookmarks);

    g_object_unref (file);
}

static void
save_file_async (CajaBookmarkList *bookmarks,
                 GAsyncReadyCallback callback)
{
    GFile *file;
    GList *l;
    GString *bookmark_string;
    GFile *parent;
    char *path;

    /* temporarily disable bookmark file monitoring when writing file */
    if (bookmarks->monitor != NULL)
    {
        g_file_monitor_cancel (bookmarks->monitor);
        bookmarks->monitor = NULL;
    }

    file = caja_bookmark_list_get_file ();
    bookmark_string = g_string_new (NULL);

    for (l = bookmarks->list; l; l = l->next)
    {
        CajaBookmark *bookmark;

        bookmark = CAJA_BOOKMARK (l->data);

        /* make sure we save label if it has one for compatibility with GTK 2.7 and 2.8 */
        if (caja_bookmark_get_has_custom_name (bookmark))
        {
            char *label, *uri;
            label = caja_bookmark_get_name (bookmark);
            uri = caja_bookmark_get_uri (bookmark);
            g_string_append_printf (bookmark_string,
                                    "%s %s\n", uri, label);
            g_free (uri);
            g_free (label);
        }
        else
        {
            char *uri;
            uri = caja_bookmark_get_uri (bookmark);
            g_string_append_printf (bookmark_string, "%s\n", uri);
            g_free (uri);
        }
    }

    /* keep the bookmark list alive */
    g_object_ref (bookmarks);

    parent = g_file_get_parent (file);
    path = g_file_get_path (parent);
    g_mkdir_with_parents (path, 0700);
    g_free (path);
    g_object_unref (parent);

    g_file_replace_contents_async (file, bookmark_string->str,
                                   bookmark_string->len, NULL,
                                   FALSE, 0, NULL, callback,
                                   bookmarks);

    g_object_unref (file);
}

static void
process_next_op (CajaBookmarkList *bookmarks);

static void
op_processed_cb (GObject *source,
                 GAsyncResult *res,
                 gpointer user_data)
{
    CajaBookmarkList *self = user_data;
    int op;

    op = GPOINTER_TO_INT (g_queue_pop_tail (self->pending_ops));

    if (op == LOAD_JOB)
    {
        load_file_finish (self, source, res);
    }
    else
    {
        save_file_finish (self, source, res);
    }

    if (!g_queue_is_empty (self->pending_ops))
    {
        process_next_op (self);
    }

    /* release the reference acquired during the _async method */
    g_object_unref (self);
}

static void
process_next_op (CajaBookmarkList *bookmarks)
{
    gint op;

    op = GPOINTER_TO_INT (g_queue_peek_tail (bookmarks->pending_ops));

    if (op == LOAD_JOB)
    {
        load_file_async (bookmarks, op_processed_cb);
    }
    else
    {
        save_file_async (bookmarks, op_processed_cb);
    }
}

/**
 * caja_bookmark_list_load_file:
 *
 * Reads bookmarks from file, clobbering contents in memory.
 * @bookmarks: the list of bookmarks to fill with file contents.
 **/
static void
caja_bookmark_list_load_file (CajaBookmarkList *bookmarks)
{
    g_queue_push_head (bookmarks->pending_ops, GINT_TO_POINTER (LOAD_JOB));

    if (g_queue_get_length (bookmarks->pending_ops) == 1)
    {
        process_next_op (bookmarks);
    }
}

/**
 * caja_bookmark_list_save_file:
 *
 * Save bookmarks to disk.
 * @bookmarks: the list of bookmarks to save.
 **/
static void
caja_bookmark_list_save_file (CajaBookmarkList *bookmarks)
{
    g_signal_emit (bookmarks, signals[CONTENTS_CHANGED], 0);

    g_queue_push_head (bookmarks->pending_ops, GINT_TO_POINTER (SAVE_JOB));

    if (g_queue_get_length (bookmarks->pending_ops) == 1)
    {
        process_next_op (bookmarks);
    }
}

/**
 * caja_bookmark_list_new:
 *
 * Create a new bookmark_list, with contents read from disk.
 *
 * Return value: A pointer to the new widget.
 **/
CajaBookmarkList *
caja_bookmark_list_new (void)
{
    CajaBookmarkList *list;

    list = CAJA_BOOKMARK_LIST (g_object_new (CAJA_TYPE_BOOKMARK_LIST, NULL));

    return list;
}

/**
 * caja_bookmark_list_set_window_geometry:
 *
 * Set a bookmarks window's geometry (position & size), in string form. This is
 * stored to disk by this class, and can be retrieved later in
 * the same session or in a future session.
 * @bookmarks: the list of bookmarks associated with the window.
 * @geometry: the new window geometry string.
 **/
void
caja_bookmark_list_set_window_geometry (CajaBookmarkList *bookmarks,
                                        const char           *geometry)
{
    g_return_if_fail (CAJA_IS_BOOKMARK_LIST (bookmarks));
    g_return_if_fail (geometry != NULL);

    g_free (window_geometry);
    window_geometry = g_strdup (geometry);

    caja_bookmark_list_save_file (bookmarks);
}

