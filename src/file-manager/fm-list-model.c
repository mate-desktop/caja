/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-list-model.h - a GtkTreeModel for file lists.

   Copyright (C) 2001, 2002 Anders Carlsson
   Copyright (C) 2003, Soeren Sandmann
   Copyright (C) 2004, Novell, Inc.

   The Mate Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Mate Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Mate Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
   Boston, MA 02110-1301, USA.

   Authors: Anders Carlsson <andersca@gnu.org>, Soeren Sandmann (sandmann@daimi.au.dk), Dave Camp <dave@ximian.com>
*/

#include <config.h>
#include "fm-list-model.h"
#include <libegg/eggtreemultidnd.h>

#include <string.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libcaja-private/caja-dnd.h>
#include <glib.h>

#include <src/glibcompat.h> /* for g_list_free_full */

enum
{
    SUBDIRECTORY_UNLOADED,
    LAST_SIGNAL
};

static GQuark attribute_name_q,
       attribute_modification_date_q,
       attribute_date_modified_q;

/* msec delay after Loading... dummy row turns into (empty) */
#define LOADING_TO_EMPTY_DELAY 100

static guint list_model_signals[LAST_SIGNAL] = { 0 };

static int fm_list_model_file_entry_compare_func (gconstpointer a,
        gconstpointer b,
        gpointer      user_data);
static void fm_list_model_tree_model_init (GtkTreeModelIface *iface);
static void fm_list_model_sortable_init (GtkTreeSortableIface *iface);
static void fm_list_model_multi_drag_source_init (EggTreeMultiDragSourceIface *iface);

struct FMListModelDetails
{
    GSequence *files;
    GHashTable *directory_reverse_map; /* map from directory to GSequenceIter's */
    GHashTable *top_reverse_map;	   /* map from files in top dir to GSequenceIter's */

    int stamp;

    GQuark sort_attribute;
    GtkSortType order;

    gboolean sort_directories_first;

    GtkTreeView *drag_view;
    int drag_begin_x;
    int drag_begin_y;

    GPtrArray *columns;

    GList *highlight_files;
};

typedef struct
{
    FMListModel *model;

    GList *path_list;
} DragDataGetInfo;

typedef struct FileEntry FileEntry;

struct FileEntry
{
    CajaFile *file;
    GHashTable *reverse_map;	/* map from files to GSequenceIter's */
    CajaDirectory *subdirectory;
    FileEntry *parent;
    GSequence *files;
    GSequenceIter *ptr;
    guint loaded : 1;
};

G_DEFINE_TYPE_WITH_CODE (FMListModel, fm_list_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
                                 fm_list_model_tree_model_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_SORTABLE,
                                 fm_list_model_sortable_init)
                         G_IMPLEMENT_INTERFACE (EGG_TYPE_TREE_MULTI_DRAG_SOURCE,
                                 fm_list_model_multi_drag_source_init));

static const GtkTargetEntry drag_types [] =
{
    { CAJA_ICON_DND_MATE_ICON_LIST_TYPE, 0, CAJA_ICON_DND_MATE_ICON_LIST },
    { CAJA_ICON_DND_URI_LIST_TYPE, 0, CAJA_ICON_DND_URI_LIST },
};

static GtkTargetList *drag_target_list = NULL;

static void
file_entry_free (FileEntry *file_entry)
{
    caja_file_unref (file_entry->file);
    if (file_entry->reverse_map)
    {
        g_hash_table_destroy (file_entry->reverse_map);
        file_entry->reverse_map = NULL;
    }
    if (file_entry->subdirectory != NULL)
    {
        caja_directory_unref (file_entry->subdirectory);
    }
    if (file_entry->files != NULL)
    {
        g_sequence_free (file_entry->files);
    }
    g_free (file_entry);
}

static GtkTreeModelFlags
fm_list_model_get_flags (GtkTreeModel *tree_model)
{
    return GTK_TREE_MODEL_ITERS_PERSIST;
}

static int
fm_list_model_get_n_columns (GtkTreeModel *tree_model)
{
    return FM_LIST_MODEL_NUM_COLUMNS + FM_LIST_MODEL (tree_model)->details->columns->len;
}

static GType
fm_list_model_get_column_type (GtkTreeModel *tree_model, int index)
{
    switch (index)
    {
    case FM_LIST_MODEL_FILE_COLUMN:
        return CAJA_TYPE_FILE;
    case FM_LIST_MODEL_SUBDIRECTORY_COLUMN:
        return CAJA_TYPE_DIRECTORY;
    case FM_LIST_MODEL_SMALLEST_ICON_COLUMN:
    case FM_LIST_MODEL_SMALLER_ICON_COLUMN:
    case FM_LIST_MODEL_SMALL_ICON_COLUMN:
    case FM_LIST_MODEL_STANDARD_ICON_COLUMN:
    case FM_LIST_MODEL_LARGE_ICON_COLUMN:
    case FM_LIST_MODEL_LARGER_ICON_COLUMN:
    case FM_LIST_MODEL_LARGEST_ICON_COLUMN:
        return GDK_TYPE_PIXBUF;
    case FM_LIST_MODEL_FILE_NAME_IS_EDITABLE_COLUMN:
        return G_TYPE_BOOLEAN;
    default:
        if (index < FM_LIST_MODEL_NUM_COLUMNS + FM_LIST_MODEL (tree_model)->details->columns->len)
        {
            return G_TYPE_STRING;
        }
        else
        {
            return G_TYPE_INVALID;
        }
    }
}

static void
fm_list_model_ptr_to_iter (FMListModel *model, GSequenceIter *ptr, GtkTreeIter *iter)
{
    g_assert (!g_sequence_iter_is_end (ptr));
    if (iter != NULL)
    {
        iter->stamp = model->details->stamp;
        iter->user_data = ptr;
    }
}

static gboolean
fm_list_model_get_iter (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreePath *path)
{
    FMListModel *model;
    GSequence *files;
    GSequenceIter *ptr;
    FileEntry *file_entry;
    int i, d;

    model = (FMListModel *)tree_model;
    ptr = NULL;

    files = model->details->files;
    for (d = 0; d < gtk_tree_path_get_depth (path); d++)
    {
        i = gtk_tree_path_get_indices (path)[d];

        if (files == NULL || i >= g_sequence_get_length (files))
        {
            return FALSE;
        }

        ptr = g_sequence_get_iter_at_pos (files, i);
        file_entry = g_sequence_get (ptr);
        files = file_entry->files;
    }

    fm_list_model_ptr_to_iter (model, ptr, iter);

    return TRUE;
}

static GtkTreePath *
fm_list_model_get_path (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
    GtkTreePath *path;
    FMListModel *model;
    GSequenceIter *ptr;
    FileEntry *file_entry;


    model = (FMListModel *)tree_model;

    g_return_val_if_fail (iter->stamp == model->details->stamp, NULL);

    if (g_sequence_iter_is_end (iter->user_data))
    {
        /* FIXME is this right? */
        return NULL;
    }

    path = gtk_tree_path_new ();
    ptr = iter->user_data;
    while (ptr != NULL)
    {
        gtk_tree_path_prepend_index (path, g_sequence_iter_get_position (ptr));
        file_entry = g_sequence_get (ptr);
        if (file_entry->parent != NULL)
        {
            ptr = file_entry->parent->ptr;
        }
        else
        {
            ptr = NULL;
        }
    }

    return path;
}

static void
fm_list_model_get_value (GtkTreeModel *tree_model, GtkTreeIter *iter, int column, GValue *value)
{
    FMListModel *model;
    FileEntry *file_entry;
    CajaFile *file;
    char *str;
    GdkPixbuf *icon, *rendered_icon;
    GIcon *gicon, *emblemed_icon, *emblem_icon;
    CajaIconInfo *icon_info;
    GEmblem *emblem;
    GList *emblem_icons, *l;
    int icon_size;
    CajaZoomLevel zoom_level;
    CajaFile *parent_file;
    char *emblems_to_ignore[3];
    int i;
    CajaFileIconFlags flags;

    model = (FMListModel *)tree_model;

    g_return_if_fail (model->details->stamp == iter->stamp);
    g_return_if_fail (!g_sequence_iter_is_end (iter->user_data));

    file_entry = g_sequence_get (iter->user_data);
    file = file_entry->file;

    switch (column)
    {
    case FM_LIST_MODEL_FILE_COLUMN:
        g_value_init (value, CAJA_TYPE_FILE);

        g_value_set_object (value, file);
        break;
    case FM_LIST_MODEL_SUBDIRECTORY_COLUMN:
        g_value_init (value, CAJA_TYPE_DIRECTORY);

        g_value_set_object (value, file_entry->subdirectory);
        break;
    case FM_LIST_MODEL_SMALLEST_ICON_COLUMN:
    case FM_LIST_MODEL_SMALLER_ICON_COLUMN:
    case FM_LIST_MODEL_SMALL_ICON_COLUMN:
    case FM_LIST_MODEL_STANDARD_ICON_COLUMN:
    case FM_LIST_MODEL_LARGE_ICON_COLUMN:
    case FM_LIST_MODEL_LARGER_ICON_COLUMN:
    case FM_LIST_MODEL_LARGEST_ICON_COLUMN:
        g_value_init (value, GDK_TYPE_PIXBUF);

        if (file != NULL)
        {
            zoom_level = fm_list_model_get_zoom_level_from_column_id (column);
            icon_size = caja_get_icon_size_for_zoom_level (zoom_level);

            flags = CAJA_FILE_ICON_FLAGS_USE_THUMBNAILS |
                    CAJA_FILE_ICON_FLAGS_FORCE_THUMBNAIL_SIZE |
                    CAJA_FILE_ICON_FLAGS_USE_MOUNT_ICON_AS_EMBLEM;
            if (model->details->drag_view != NULL)
            {
                GtkTreePath *path_a, *path_b;

                gtk_tree_view_get_drag_dest_row (model->details->drag_view,
                                                 &path_a,
                                                 NULL);
                if (path_a != NULL)
                {
                    path_b = gtk_tree_model_get_path (tree_model, iter);

                    if (gtk_tree_path_compare (path_a, path_b) == 0)
                    {
                        flags |= CAJA_FILE_ICON_FLAGS_FOR_DRAG_ACCEPT;
                    }

                    gtk_tree_path_free (path_a);
                    gtk_tree_path_free (path_b);
                }
            }

            gicon = caja_file_get_gicon (file, flags);

            /* render emblems with GEmblemedIcon */
            parent_file = caja_file_get_parent (file);
            i = 0;
            emblems_to_ignore[i++] = CAJA_FILE_EMBLEM_NAME_TRASH;
            if (parent_file) {
            	if (!caja_file_can_write (parent_file)) {
                    emblems_to_ignore[i++] = CAJA_FILE_EMBLEM_NAME_CANT_WRITE;
            	}
            	caja_file_unref (parent_file);
            }
            emblems_to_ignore[i++] = NULL;

            emblem = NULL;
            emblem_icons = caja_file_get_emblem_icons (file,
            					       emblems_to_ignore);

            if (emblem_icons != NULL) {
                emblem_icon = emblem_icons->data;
                emblem = g_emblem_new (emblem_icon);
                emblemed_icon = g_emblemed_icon_new (gicon, emblem);

                g_object_unref (emblem);

            	for (l = emblem_icons->next; l != NULL; l = l->next) {
            	    emblem_icon = l->data;
            	    emblem = g_emblem_new (emblem_icon);
            	    g_emblemed_icon_add_emblem
            	        (G_EMBLEMED_ICON (emblemed_icon), emblem);

                    g_object_unref (emblem);
            	}

                g_list_free_full (emblem_icons, g_object_unref);

            	g_object_unref (gicon);
            	gicon = emblemed_icon;
            }

            icon_info = caja_icon_info_lookup (gicon, icon_size);
            icon = caja_icon_info_get_pixbuf_at_size (icon_info, icon_size);

            g_object_unref (icon_info);
            g_object_unref (gicon);

            if (model->details->highlight_files != NULL &&
                    g_list_find_custom (model->details->highlight_files,
                                        file, (GCompareFunc) caja_file_compare_location))
            {
                rendered_icon = eel_gdk_pixbuf_render (icon, 1, 255, 255, 0, 0);

                if (rendered_icon != NULL)
                {
                    g_object_unref (icon);
                    icon = rendered_icon;
                }
            }

            g_value_set_object (value, icon);
            g_object_unref (icon);
        }
        break;
    case FM_LIST_MODEL_FILE_NAME_IS_EDITABLE_COLUMN:
        g_value_init (value, G_TYPE_BOOLEAN);

        g_value_set_boolean (value, file != NULL && caja_file_can_rename (file));
        break;
    default:
        if (column >= FM_LIST_MODEL_NUM_COLUMNS || column < FM_LIST_MODEL_NUM_COLUMNS + model->details->columns->len)
        {
            CajaColumn *caja_column;
            GQuark attribute;
            caja_column = model->details->columns->pdata[column - FM_LIST_MODEL_NUM_COLUMNS];

            g_value_init (value, G_TYPE_STRING);
            g_object_get (caja_column,
                          "attribute_q", &attribute,
                          NULL);
            if (file != NULL)
            {
                str = caja_file_get_string_attribute_with_default_q (file,
                        attribute);
                g_value_take_string (value, str);
            }
            else if (attribute == attribute_name_q)
            {
                if (file_entry->parent->loaded)
                {
                    g_value_set_string (value, _("(Empty)"));
                }
                else
                {
                    g_value_set_string (value, _("Loading..."));
                }
            }
        }
        else
        {
            g_assert_not_reached ();
        }
    }
}

static gboolean
fm_list_model_iter_next (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
    FMListModel *model;

    model = (FMListModel *)tree_model;

    g_return_val_if_fail (model->details->stamp == iter->stamp, FALSE);

    iter->user_data = g_sequence_iter_next (iter->user_data);

    return !g_sequence_iter_is_end (iter->user_data);
}

static gboolean
fm_list_model_iter_children (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *parent)
{
    FMListModel *model;
    GSequence *files;
    FileEntry *file_entry;

    model = (FMListModel *)tree_model;

    if (parent == NULL)
    {
        files = model->details->files;
    }
    else
    {
        file_entry = g_sequence_get (parent->user_data);
        files = file_entry->files;
    }

    if (files == NULL || g_sequence_get_length (files) == 0)
    {
        return FALSE;
    }

    iter->stamp = model->details->stamp;
    iter->user_data = g_sequence_get_begin_iter (files);

    return TRUE;
}

static gboolean
fm_list_model_iter_has_child (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
    FileEntry *file_entry;

    if (iter == NULL)
    {
        return !fm_list_model_is_empty (FM_LIST_MODEL (tree_model));
    }

    file_entry = g_sequence_get (iter->user_data);

    return (file_entry->files != NULL && g_sequence_get_length (file_entry->files) > 0);
}

static int
fm_list_model_iter_n_children (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
    FMListModel *model;
    GSequence *files;
    FileEntry *file_entry;

    model = (FMListModel *)tree_model;

    if (iter == NULL)
    {
        files = model->details->files;
    }
    else
    {
        file_entry = g_sequence_get (iter->user_data);
        files = file_entry->files;
    }

    return g_sequence_get_length (files);
}

static gboolean
fm_list_model_iter_nth_child (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *parent, int n)
{
    FMListModel *model;
    GSequenceIter *child;
    GSequence *files;
    FileEntry *file_entry;

    model = (FMListModel *)tree_model;

    if (parent != NULL)
    {
        file_entry = g_sequence_get (parent->user_data);
        files = file_entry->files;
    }
    else
    {
        files = model->details->files;
    }

    child = g_sequence_get_iter_at_pos (files, n);

    if (g_sequence_iter_is_end (child))
    {
        return FALSE;
    }

    iter->stamp = model->details->stamp;
    iter->user_data = child;

    return TRUE;
}

static gboolean
fm_list_model_iter_parent (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *child)
{
    FMListModel *model;
    FileEntry *file_entry;

    model = (FMListModel *)tree_model;

    file_entry = g_sequence_get (child->user_data);

    if (file_entry->parent == NULL)
    {
        return FALSE;
    }

    iter->stamp = model->details->stamp;
    iter->user_data = file_entry->parent->ptr;

    return TRUE;
}

static GSequenceIter *
lookup_file (FMListModel *model, CajaFile *file,
             CajaDirectory *directory)
{
    FileEntry *file_entry;
    GSequenceIter *ptr, *parent_ptr;

    parent_ptr = NULL;
    if (directory)
    {
        parent_ptr = g_hash_table_lookup (model->details->directory_reverse_map,
                                          directory);
    }

    if (parent_ptr)
    {
        file_entry = g_sequence_get (parent_ptr);
        ptr = g_hash_table_lookup (file_entry->reverse_map, file);
    }
    else
    {
        ptr = g_hash_table_lookup (model->details->top_reverse_map, file);
    }

    if (ptr)
    {
        g_assert (((FileEntry *)g_sequence_get (ptr))->file == file);
    }

    return ptr;
}


struct GetIters
{
    FMListModel *model;
    CajaFile *file;
    GList *iters;
};

static void
dir_to_iters (struct GetIters *data,
              GHashTable *reverse_map)
{
    GSequenceIter *ptr;

    ptr = g_hash_table_lookup (reverse_map, data->file);
    if (ptr)
    {
        GtkTreeIter *iter;
        iter = g_new0 (GtkTreeIter, 1);
        fm_list_model_ptr_to_iter (data->model, ptr, iter);
        data->iters = g_list_prepend (data->iters, iter);
    }
}

static void
file_to_iter_cb (gpointer  key,
                 gpointer  value,
                 gpointer  user_data)
{
    struct GetIters *data;
    FileEntry *dir_file_entry;

    data = user_data;
    dir_file_entry = g_sequence_get ((GSequenceIter *)value);
    dir_to_iters (data, dir_file_entry->reverse_map);
}

GList *
fm_list_model_get_all_iters_for_file (FMListModel *model, CajaFile *file)
{
    struct GetIters data;

    data.file = file;
    data.model = model;
    data.iters = NULL;

    dir_to_iters (&data, model->details->top_reverse_map);
    g_hash_table_foreach (model->details->directory_reverse_map,
                          file_to_iter_cb, &data);

    return g_list_reverse (data.iters);
}

gboolean
fm_list_model_get_first_iter_for_file (FMListModel          *model,
                                       CajaFile         *file,
                                       GtkTreeIter          *iter)
{
    GList *list;
    gboolean res;

    res = FALSE;

    list = fm_list_model_get_all_iters_for_file (model, file);
    if (list != NULL)
    {
        res = TRUE;
        *iter = *(GtkTreeIter *)list->data;
    }
    g_list_free_full (list, g_free);

    return res;
}


gboolean
fm_list_model_get_tree_iter_from_file (FMListModel *model, CajaFile *file,
                                       CajaDirectory *directory,
                                       GtkTreeIter *iter)
{
    GSequenceIter *ptr;

    ptr = lookup_file (model, file, directory);
    if (!ptr)
    {
        return FALSE;
    }

    fm_list_model_ptr_to_iter (model, ptr, iter);

    return TRUE;
}

static int
fm_list_model_file_entry_compare_func (gconstpointer a,
                                       gconstpointer b,
                                       gpointer      user_data)
{
    FileEntry *file_entry1;
    FileEntry *file_entry2;
    FMListModel *model;
    int result;

    model = (FMListModel *)user_data;

    file_entry1 = (FileEntry *)a;
    file_entry2 = (FileEntry *)b;

    if (file_entry1->file != NULL && file_entry2->file != NULL)
    {
        result = caja_file_compare_for_sort_by_attribute_q (file_entry1->file, file_entry2->file,
                 model->details->sort_attribute,
                 model->details->sort_directories_first,
                 (model->details->order == GTK_SORT_DESCENDING));
    }
    else if (file_entry1->file == NULL)
    {
        return -1;
    }
    else
    {
        return 1;
    }

    return result;
}

int
fm_list_model_compare_func (FMListModel *model,
                            CajaFile *file1,
                            CajaFile *file2)
{
    int result;

    result = caja_file_compare_for_sort_by_attribute_q (file1, file2,
             model->details->sort_attribute,
             model->details->sort_directories_first,
             (model->details->order == GTK_SORT_DESCENDING));

    return result;
}

static void
fm_list_model_sort_file_entries (FMListModel *model, GSequence *files, GtkTreePath *path)
{
    GSequenceIter **old_order;
    GtkTreeIter iter;
    int *new_order;
    int length;
    int i;
    FileEntry *file_entry;
    gboolean has_iter;

    length = g_sequence_get_length (files);

    if (length <= 1)
    {
        return;
    }

    /* generate old order of GSequenceIter's */
    old_order = g_new (GSequenceIter *, length);
    for (i = 0; i < length; ++i)
    {
        GSequenceIter *ptr = g_sequence_get_iter_at_pos (files, i);

        file_entry = g_sequence_get (ptr);
        if (file_entry->files != NULL)
        {
            gtk_tree_path_append_index (path, i);
            fm_list_model_sort_file_entries (model, file_entry->files, path);
            gtk_tree_path_up (path);
        }

        old_order[i] = ptr;
    }

    /* sort */
    g_sequence_sort (files, fm_list_model_file_entry_compare_func, model);

    /* generate new order */
    new_order = g_new (int, length);
    /* Note: new_order[newpos] = oldpos */
    for (i = 0; i < length; ++i)
    {
        new_order[g_sequence_iter_get_position (old_order[i])] = i;
    }

    /* Let the world know about our new order */

    g_assert (new_order != NULL);

    has_iter = FALSE;
    if (gtk_tree_path_get_depth (path) != 0)
    {
        gboolean get_iter_result;
        has_iter = TRUE;
        get_iter_result = gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, path);
        g_assert (get_iter_result);
    }

    gtk_tree_model_rows_reordered (GTK_TREE_MODEL (model),
                                   path, has_iter ? &iter : NULL, new_order);

    g_free (old_order);
    g_free (new_order);
}

static void
fm_list_model_sort (FMListModel *model)
{
    GtkTreePath *path;

    path = gtk_tree_path_new ();

    fm_list_model_sort_file_entries (model, model->details->files, path);

    gtk_tree_path_free (path);
}

static gboolean
fm_list_model_get_sort_column_id (GtkTreeSortable *sortable,
                                  gint            *sort_column_id,
                                  GtkSortType     *order)
{
    FMListModel *model;
    int id;

    model = (FMListModel *)sortable;

    id = fm_list_model_get_sort_column_id_from_attribute
         (model, model->details->sort_attribute);

    if (id == -1)
    {
        return FALSE;
    }

    if (sort_column_id != NULL)
    {
        *sort_column_id = id;
    }

    if (order != NULL)
    {
        *order = model->details->order;
    }

    return TRUE;
}

static void
fm_list_model_set_sort_column_id (GtkTreeSortable *sortable, gint sort_column_id, GtkSortType order)
{
    FMListModel *model;

    model = (FMListModel *)sortable;

    model->details->sort_attribute = fm_list_model_get_attribute_from_sort_column_id (model, sort_column_id);

    model->details->order = order;

    fm_list_model_sort (model);
    gtk_tree_sortable_sort_column_changed (sortable);
}

static gboolean
fm_list_model_has_default_sort_func (GtkTreeSortable *sortable)
{
    return FALSE;
}

static gboolean
fm_list_model_multi_row_draggable (EggTreeMultiDragSource *drag_source, GList *path_list)
{
    return TRUE;
}

static void
each_path_get_data_binder (CajaDragEachSelectedItemDataGet data_get,
                           gpointer context,
                           gpointer data)
{
    DragDataGetInfo *info;
    GList *l;
    CajaFile *file;
    GtkTreeRowReference *row;
    GtkTreePath *path;
    char *uri;
    GdkRectangle cell_area;
    GtkTreeViewColumn *column;

    info = context;

    g_return_if_fail (info->model->details->drag_view);

    column = gtk_tree_view_get_column (info->model->details->drag_view, 0);

    for (l = info->path_list; l != NULL; l = l->next)
    {
        row = l->data;

        path = gtk_tree_row_reference_get_path (row);
        file = fm_list_model_file_for_path (info->model, path);
        if (file)
        {
            gtk_tree_view_get_cell_area
            (info->model->details->drag_view,
             path,
             column,
             &cell_area);

            uri = caja_file_get_uri (file);

            (*data_get) (uri,
                         0,
                         cell_area.y - info->model->details->drag_begin_y,
                         cell_area.width, cell_area.height,
                         data);

            g_free (uri);

            caja_file_unref (file);
        }

        gtk_tree_path_free (path);
    }
}

static gboolean
fm_list_model_multi_drag_data_get (EggTreeMultiDragSource *drag_source,
                                   GList *path_list,
                                   GtkSelectionData *selection_data)
{
    FMListModel *model;
    DragDataGetInfo context;
    guint target_info;

    model = FM_LIST_MODEL (drag_source);

    context.model = model;
    context.path_list = path_list;

    if (!drag_target_list)
    {
        drag_target_list = fm_list_model_get_drag_target_list ();
    }

    if (gtk_target_list_find (drag_target_list,
                              gtk_selection_data_get_target (selection_data),
                              &target_info))
    {
        caja_drag_drag_data_get (NULL,
                                 NULL,
                                 selection_data,
                                 target_info,
                                 GDK_CURRENT_TIME,
                                 &context,
                                 each_path_get_data_binder);
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

static gboolean
fm_list_model_multi_drag_data_delete (EggTreeMultiDragSource *drag_source, GList *path_list)
{
    return TRUE;
}

static void
add_dummy_row (FMListModel *model, FileEntry *parent_entry)
{
    FileEntry *dummy_file_entry;
    GtkTreeIter iter;
    GtkTreePath *path;

    dummy_file_entry = g_new0 (FileEntry, 1);
    dummy_file_entry->parent = parent_entry;
    dummy_file_entry->ptr = g_sequence_insert_sorted (parent_entry->files, dummy_file_entry,
                            fm_list_model_file_entry_compare_func, model);
    iter.stamp = model->details->stamp;
    iter.user_data = dummy_file_entry->ptr;

    path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
    gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, &iter);
    gtk_tree_path_free (path);
}

gboolean
fm_list_model_add_file (FMListModel *model, CajaFile *file,
                        CajaDirectory *directory)
{
    GtkTreeIter iter;
    GtkTreePath *path;
    FileEntry *file_entry;
    GSequenceIter *ptr, *parent_ptr;
    GSequence *files;
    gboolean replace_dummy;
    GHashTable *parent_hash;

    parent_ptr = g_hash_table_lookup (model->details->directory_reverse_map,
                                      directory);
    if (parent_ptr)
    {
        file_entry = g_sequence_get (parent_ptr);
        ptr = g_hash_table_lookup (file_entry->reverse_map, file);
    }
    else
    {
        file_entry = NULL;
        ptr = g_hash_table_lookup (model->details->top_reverse_map, file);
    }

    if (ptr != NULL)
    {
        g_warning ("file already in tree (parent_ptr: %p)!!!\n", parent_ptr);
        return FALSE;
    }

    file_entry = g_new0 (FileEntry, 1);
    file_entry->file = caja_file_ref (file);
    file_entry->parent = NULL;
    file_entry->subdirectory = NULL;
    file_entry->files = NULL;

    files = model->details->files;
    parent_hash = model->details->top_reverse_map;

    replace_dummy = FALSE;

    if (parent_ptr != NULL)
    {
        file_entry->parent = g_sequence_get (parent_ptr);
        /* At this point we set loaded. Either we saw
         * "done" and ignored it waiting for this, or we do this
         * earlier, but then we replace the dummy row anyway,
         * so it doesn't matter */
        file_entry->parent->loaded = 1;
        parent_hash = file_entry->parent->reverse_map;
        files = file_entry->parent->files;
        if (g_sequence_get_length (files) == 1)
        {
            GSequenceIter *dummy_ptr = g_sequence_get_iter_at_pos (files, 0);
            FileEntry *dummy_entry = g_sequence_get (dummy_ptr);
            if (dummy_entry->file == NULL)
            {
                /* replace the dummy loading entry */
                model->details->stamp++;
                g_sequence_remove (dummy_ptr);

                replace_dummy = TRUE;
            }
        }
    }


    file_entry->ptr = g_sequence_insert_sorted (files, file_entry,
                      fm_list_model_file_entry_compare_func, model);

    g_hash_table_insert (parent_hash, file, file_entry->ptr);

    iter.stamp = model->details->stamp;
    iter.user_data = file_entry->ptr;

    path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
    if (replace_dummy)
    {
        gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
    }
    else
    {
        gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, &iter);
    }

    if (caja_file_is_directory (file))
    {
        file_entry->files = g_sequence_new ((GDestroyNotify)file_entry_free);

        add_dummy_row (model, file_entry);

        gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (model),
                                              path, &iter);
    }
    gtk_tree_path_free (path);

    return TRUE;
}

void
fm_list_model_file_changed (FMListModel *model, CajaFile *file,
                            CajaDirectory *directory)
{
    FileEntry *parent_file_entry;
    GtkTreeIter iter;
    GtkTreePath *path, *parent_path;
    GSequenceIter *ptr;
    int pos_before, pos_after, length, i, old;
    int *new_order;
    gboolean has_iter;
    GSequence *files;

    ptr = lookup_file (model, file, directory);
    if (!ptr)
    {
        return;
    }


    pos_before = g_sequence_iter_get_position (ptr);

    g_sequence_sort_changed (ptr, fm_list_model_file_entry_compare_func, model);

    pos_after = g_sequence_iter_get_position (ptr);

    if (pos_before != pos_after)
    {
        /* The file moved, we need to send rows_reordered */

        parent_file_entry = ((FileEntry *)g_sequence_get (ptr))->parent;

        if (parent_file_entry == NULL)
        {
            has_iter = FALSE;
            parent_path = gtk_tree_path_new ();
            files = model->details->files;
        }
        else
        {
            has_iter = TRUE;
            fm_list_model_ptr_to_iter (model, parent_file_entry->ptr, &iter);
            parent_path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
            files = parent_file_entry->files;
        }

        length = g_sequence_get_length (files);
        new_order = g_new (int, length);
        /* Note: new_order[newpos] = oldpos */
        for (i = 0, old = 0; i < length; ++i)
        {
            if (i == pos_after)
            {
                new_order[i] = pos_before;
            }
            else
            {
                if (old == pos_before)
                    old++;
                new_order[i] = old++;
            }
        }

        gtk_tree_model_rows_reordered (GTK_TREE_MODEL (model),
                                       parent_path, has_iter ? &iter : NULL, new_order);

        gtk_tree_path_free (parent_path);
        g_free (new_order);
    }

    fm_list_model_ptr_to_iter (model, ptr, &iter);
    path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
    gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
    gtk_tree_path_free (path);
}

gboolean
fm_list_model_is_empty (FMListModel *model)
{
    return (g_sequence_get_length (model->details->files) == 0);
}

guint
fm_list_model_get_length (FMListModel *model)
{
    return g_sequence_get_length (model->details->files);
}

static void
fm_list_model_remove (FMListModel *model, GtkTreeIter *iter)
{
    GSequenceIter *ptr, *child_ptr;
    FileEntry *file_entry, *child_file_entry, *parent_file_entry;
    GtkTreePath *path;
    GtkTreeIter parent_iter;

    ptr = iter->user_data;
    file_entry = g_sequence_get (ptr);

    if (file_entry->files != NULL)
    {
        while (g_sequence_get_length (file_entry->files) > 0)
        {
            child_ptr = g_sequence_get_begin_iter (file_entry->files);
            child_file_entry = g_sequence_get (child_ptr);
            if (child_file_entry->file != NULL)
            {
                fm_list_model_remove_file (model,
                                           child_file_entry->file,
                                           file_entry->subdirectory);
            }
            else
            {
                path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), iter);
                gtk_tree_path_append_index (path, 0);
                model->details->stamp++;
                g_sequence_remove (child_ptr);
                gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
                gtk_tree_path_free (path);
            }

            /* the parent iter didn't actually change */
            iter->stamp = model->details->stamp;
        }

    }

    if (file_entry->file != NULL)   /* Don't try to remove dummy row */
    {
        if (file_entry->parent != NULL)
        {
            g_hash_table_remove (file_entry->parent->reverse_map, file_entry->file);
        }
        else
        {
            g_hash_table_remove (model->details->top_reverse_map, file_entry->file);
        }
    }

    parent_file_entry = file_entry->parent;
    if (parent_file_entry && g_sequence_get_length (parent_file_entry->files) == 1 &&
            file_entry->file != NULL)
    {
        /* this is the last non-dummy child, add a dummy node */
        /* We need to do this before removing the last file to avoid
         * collapsing the row.
         */
        add_dummy_row (model, parent_file_entry);
    }

    if (file_entry->subdirectory != NULL)
    {
        g_signal_emit (model,
                       list_model_signals[SUBDIRECTORY_UNLOADED], 0,
                       file_entry->subdirectory);
        g_hash_table_remove (model->details->directory_reverse_map,
                             file_entry->subdirectory);
    }

    path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), iter);

    g_sequence_remove (ptr);
    model->details->stamp++;
    gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);

    gtk_tree_path_free (path);

    if (parent_file_entry && g_sequence_get_length (parent_file_entry->files) == 0)
    {
        parent_iter.stamp = model->details->stamp;
        parent_iter.user_data = parent_file_entry->ptr;
        path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &parent_iter);
        gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (model),
                                              path, &parent_iter);
        gtk_tree_path_free (path);
    }
}

void
fm_list_model_remove_file (FMListModel *model, CajaFile *file,
                           CajaDirectory *directory)
{
    GtkTreeIter iter;

    if (fm_list_model_get_tree_iter_from_file (model, file, directory, &iter))
    {
        fm_list_model_remove (model, &iter);
    }
}

static void
fm_list_model_clear_directory (FMListModel *model, GSequence *files)
{
    GtkTreeIter iter;
    FileEntry *file_entry;

    while (g_sequence_get_length (files) > 0)
    {
        iter.user_data = g_sequence_get_begin_iter (files);

        file_entry = g_sequence_get (iter.user_data);
        if (file_entry->files != NULL)
        {
            fm_list_model_clear_directory (model, file_entry->files);
        }

        iter.stamp = model->details->stamp;
        fm_list_model_remove (model, &iter);
    }
}

void
fm_list_model_clear (FMListModel *model)
{
    g_return_if_fail (model != NULL);

    fm_list_model_clear_directory (model, model->details->files);
}

CajaFile *
fm_list_model_file_for_path (FMListModel *model, GtkTreePath *path)
{
    CajaFile *file;
    GtkTreeIter iter;

    file = NULL;
    if (gtk_tree_model_get_iter (GTK_TREE_MODEL (model),
                                 &iter, path))
    {
        gtk_tree_model_get (GTK_TREE_MODEL (model),
                            &iter,
                            FM_LIST_MODEL_FILE_COLUMN, &file,
                            -1);
    }
    return file;
}

gboolean
fm_list_model_load_subdirectory (FMListModel *model, GtkTreePath *path, CajaDirectory **directory)
{
    GtkTreeIter iter;
    FileEntry *file_entry;
    CajaDirectory *subdirectory;

    if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, path))
    {
        return FALSE;
    }

    file_entry = g_sequence_get (iter.user_data);
    if (file_entry->file == NULL ||
            file_entry->subdirectory != NULL)
    {
        return FALSE;
    }

    subdirectory = caja_directory_get_for_file (file_entry->file);

    if (g_hash_table_lookup (model->details->directory_reverse_map,
                             subdirectory) != NULL)
    {
        caja_directory_unref (subdirectory);
        g_warning ("Already in directory_reverse_map, failing\n");
        return FALSE;
    }

    file_entry->subdirectory = subdirectory,
                g_hash_table_insert (model->details->directory_reverse_map,
                                     subdirectory, file_entry->ptr);
    file_entry->reverse_map = g_hash_table_new (g_direct_hash, g_direct_equal);

    /* Return a ref too */
    caja_directory_ref (subdirectory);
    *directory = subdirectory;

    return TRUE;
}

/* removes all children of the subfolder and unloads the subdirectory */
void
fm_list_model_unload_subdirectory (FMListModel *model, GtkTreeIter *iter)
{
    GSequenceIter *child_ptr;
    FileEntry *file_entry, *child_file_entry;
    GtkTreeIter child_iter;

    file_entry = g_sequence_get (iter->user_data);
    if (file_entry->file == NULL ||
            file_entry->subdirectory == NULL)
    {
        return;
    }

    file_entry->loaded = 0;

    /* Remove all children */
    while (g_sequence_get_length (file_entry->files) > 0)
    {
        child_ptr = g_sequence_get_begin_iter (file_entry->files);
        child_file_entry = g_sequence_get (child_ptr);
        if (child_file_entry->file == NULL)
        {
            /* Don't delete the dummy node */
            break;
        }
        else
        {
            fm_list_model_ptr_to_iter (model, child_ptr, &child_iter);
            fm_list_model_remove (model, &child_iter);
        }
    }

    /* Emit unload signal */
    g_signal_emit (model,
                   list_model_signals[SUBDIRECTORY_UNLOADED], 0,
                   file_entry->subdirectory);

    /* actually unload */
    g_hash_table_remove (model->details->directory_reverse_map,
                         file_entry->subdirectory);
    caja_directory_unref (file_entry->subdirectory);
    file_entry->subdirectory = NULL;

    g_assert (g_hash_table_size (file_entry->reverse_map) == 0);
    g_hash_table_destroy (file_entry->reverse_map);
    file_entry->reverse_map = NULL;
}



void
fm_list_model_set_should_sort_directories_first (FMListModel *model, gboolean sort_directories_first)
{
    if (model->details->sort_directories_first == sort_directories_first)
    {
        return;
    }

    model->details->sort_directories_first = sort_directories_first;
    fm_list_model_sort (model);
}

int
fm_list_model_get_sort_column_id_from_attribute (FMListModel *model,
        GQuark attribute)
{
    guint i;

    if (attribute == 0)
    {
        return -1;
    }

    /* Hack - the preferences dialog sets modification_date for some
     * rather than date_modified for some reason.  Make sure that
     * works. */
    if (attribute == attribute_modification_date_q)
    {
        attribute = attribute_date_modified_q;
    }

    for (i = 0; i < model->details->columns->len; i++)
    {
        CajaColumn *column;
        GQuark column_attribute;

        column =
            CAJA_COLUMN (model->details->columns->pdata[i]);
        g_object_get (G_OBJECT (column),
                      "attribute_q", &column_attribute,
                      NULL);
        if (column_attribute == attribute)
        {
            return FM_LIST_MODEL_NUM_COLUMNS + i;
        }
    }

    return -1;
}

GQuark
fm_list_model_get_attribute_from_sort_column_id (FMListModel *model,
        int sort_column_id)
{
    CajaColumn *column;
    int index;
    GQuark attribute;

    index = sort_column_id - FM_LIST_MODEL_NUM_COLUMNS;

    if (index < 0 || index >= model->details->columns->len)
    {
        g_warning ("unknown sort column id: %d", sort_column_id);
        return 0;
    }

    column = CAJA_COLUMN (model->details->columns->pdata[index]);
    g_object_get (G_OBJECT (column), "attribute_q", &attribute, NULL);

    return attribute;
}

CajaZoomLevel
fm_list_model_get_zoom_level_from_column_id (int column)
{
    switch (column)
    {
    case FM_LIST_MODEL_SMALLEST_ICON_COLUMN:
        return CAJA_ZOOM_LEVEL_SMALLEST;
    case FM_LIST_MODEL_SMALLER_ICON_COLUMN:
        return CAJA_ZOOM_LEVEL_SMALLER;
    case FM_LIST_MODEL_SMALL_ICON_COLUMN:
        return CAJA_ZOOM_LEVEL_SMALL;
    case FM_LIST_MODEL_STANDARD_ICON_COLUMN:
        return CAJA_ZOOM_LEVEL_STANDARD;
    case FM_LIST_MODEL_LARGE_ICON_COLUMN:
        return CAJA_ZOOM_LEVEL_LARGE;
    case FM_LIST_MODEL_LARGER_ICON_COLUMN:
        return CAJA_ZOOM_LEVEL_LARGER;
    case FM_LIST_MODEL_LARGEST_ICON_COLUMN:
        return CAJA_ZOOM_LEVEL_LARGEST;
    }

    g_return_val_if_reached (CAJA_ZOOM_LEVEL_STANDARD);
}

int
fm_list_model_get_column_id_from_zoom_level (CajaZoomLevel zoom_level)
{
    switch (zoom_level)
    {
    case CAJA_ZOOM_LEVEL_SMALLEST:
        return FM_LIST_MODEL_SMALLEST_ICON_COLUMN;
    case CAJA_ZOOM_LEVEL_SMALLER:
        return FM_LIST_MODEL_SMALLER_ICON_COLUMN;
    case CAJA_ZOOM_LEVEL_SMALL:
        return FM_LIST_MODEL_SMALL_ICON_COLUMN;
    case CAJA_ZOOM_LEVEL_STANDARD:
        return FM_LIST_MODEL_STANDARD_ICON_COLUMN;
    case CAJA_ZOOM_LEVEL_LARGE:
        return FM_LIST_MODEL_LARGE_ICON_COLUMN;
    case CAJA_ZOOM_LEVEL_LARGER:
        return FM_LIST_MODEL_LARGER_ICON_COLUMN;
    case CAJA_ZOOM_LEVEL_LARGEST:
        return FM_LIST_MODEL_LARGEST_ICON_COLUMN;
    }

    g_return_val_if_reached (FM_LIST_MODEL_STANDARD_ICON_COLUMN);
}

void
fm_list_model_set_drag_view (FMListModel *model,
                             GtkTreeView *view,
                             int drag_begin_x,
                             int drag_begin_y)
{
    g_return_if_fail (model != NULL);
    g_return_if_fail (FM_IS_LIST_MODEL (model));
    g_return_if_fail (!view || GTK_IS_TREE_VIEW (view));

    model->details->drag_view = view;
    model->details->drag_begin_x = drag_begin_x;
    model->details->drag_begin_y = drag_begin_y;
}

GtkTargetList *
fm_list_model_get_drag_target_list ()
{
    GtkTargetList *target_list;

    target_list = gtk_target_list_new (drag_types, G_N_ELEMENTS (drag_types));
    gtk_target_list_add_text_targets (target_list, CAJA_ICON_DND_TEXT);

    return target_list;
}

int
fm_list_model_add_column (FMListModel *model,
                          CajaColumn *column)
{
    g_ptr_array_add (model->details->columns, column);
    g_object_ref (column);

    return FM_LIST_MODEL_NUM_COLUMNS + (model->details->columns->len - 1);
}

int
fm_list_model_get_column_number (FMListModel *model,
                                 const char *column_name)
{
    int i;

    for (i = 0; i < model->details->columns->len; i++)
    {
        CajaColumn *column;
        char *name;

        column = model->details->columns->pdata[i];

        g_object_get (G_OBJECT (column), "name", &name, NULL);

        if (!strcmp (name, column_name))
        {
            g_free (name);
            return FM_LIST_MODEL_NUM_COLUMNS + i;
        }
        g_free (name);
    }

    return -1;
}

static void
fm_list_model_dispose (GObject *object)
{
    FMListModel *model;
    int i;

    model = FM_LIST_MODEL (object);

    if (model->details->columns)
    {
        for (i = 0; i < model->details->columns->len; i++)
        {
            g_object_unref (model->details->columns->pdata[i]);
        }
        g_ptr_array_free (model->details->columns, TRUE);
        model->details->columns = NULL;
    }

    if (model->details->files)
    {
        g_sequence_free (model->details->files);
        model->details->files = NULL;
    }

    if (model->details->top_reverse_map)
    {
        g_hash_table_destroy (model->details->top_reverse_map);
        model->details->top_reverse_map = NULL;
    }
    if (model->details->directory_reverse_map)
    {
        g_hash_table_destroy (model->details->directory_reverse_map);
        model->details->directory_reverse_map = NULL;
    }

    G_OBJECT_CLASS (fm_list_model_parent_class)->dispose (object);
}

static void
fm_list_model_finalize (GObject *object)
{
    FMListModel *model;

    model = FM_LIST_MODEL (object);

    if (model->details->highlight_files != NULL)
    {
        caja_file_list_free (model->details->highlight_files);
        model->details->highlight_files = NULL;
    }

    g_free (model->details);

    G_OBJECT_CLASS (fm_list_model_parent_class)->finalize (object);
}

static void
fm_list_model_init (FMListModel *model)
{
    model->details = g_new0 (FMListModelDetails, 1);
    model->details->files = g_sequence_new ((GDestroyNotify)file_entry_free);
    model->details->top_reverse_map = g_hash_table_new (g_direct_hash, g_direct_equal);
    model->details->directory_reverse_map = g_hash_table_new (g_direct_hash, g_direct_equal);
    model->details->stamp = g_random_int ();
    model->details->sort_attribute = 0;
    model->details->columns = g_ptr_array_new ();
}

static void
fm_list_model_class_init (FMListModelClass *klass)
{
    GObjectClass *object_class;

    attribute_name_q = g_quark_from_static_string ("name");
    attribute_modification_date_q = g_quark_from_static_string ("modification_date");
    attribute_date_modified_q = g_quark_from_static_string ("date_modified");

    object_class = (GObjectClass *)klass;
    object_class->finalize = fm_list_model_finalize;
    object_class->dispose = fm_list_model_dispose;

    list_model_signals[SUBDIRECTORY_UNLOADED] =
        g_signal_new ("subdirectory_unloaded",
                      FM_TYPE_LIST_MODEL,
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (FMListModelClass, subdirectory_unloaded),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__OBJECT,
                      G_TYPE_NONE, 1,
                      CAJA_TYPE_DIRECTORY);
}

static void
fm_list_model_tree_model_init (GtkTreeModelIface *iface)
{
    iface->get_flags = fm_list_model_get_flags;
    iface->get_n_columns = fm_list_model_get_n_columns;
    iface->get_column_type = fm_list_model_get_column_type;
    iface->get_iter = fm_list_model_get_iter;
    iface->get_path = fm_list_model_get_path;
    iface->get_value = fm_list_model_get_value;
    iface->iter_next = fm_list_model_iter_next;
    iface->iter_children = fm_list_model_iter_children;
    iface->iter_has_child = fm_list_model_iter_has_child;
    iface->iter_n_children = fm_list_model_iter_n_children;
    iface->iter_nth_child = fm_list_model_iter_nth_child;
    iface->iter_parent = fm_list_model_iter_parent;
}

static void
fm_list_model_sortable_init (GtkTreeSortableIface *iface)
{
    iface->get_sort_column_id = fm_list_model_get_sort_column_id;
    iface->set_sort_column_id = fm_list_model_set_sort_column_id;
    iface->has_default_sort_func = fm_list_model_has_default_sort_func;
}

static void
fm_list_model_multi_drag_source_init (EggTreeMultiDragSourceIface *iface)
{
    iface->row_draggable = fm_list_model_multi_row_draggable;
    iface->drag_data_get = fm_list_model_multi_drag_data_get;
    iface->drag_data_delete = fm_list_model_multi_drag_data_delete;
}

void
fm_list_model_subdirectory_done_loading (FMListModel *model, CajaDirectory *directory)
{
    GtkTreeIter iter;
    GtkTreePath *path;
    FileEntry *file_entry, *dummy_entry;
    GSequenceIter *parent_ptr, *dummy_ptr;
    GSequence *files;

    if (model == NULL || model->details->directory_reverse_map == NULL)
    {
        return;
    }
    parent_ptr = g_hash_table_lookup (model->details->directory_reverse_map,
                                      directory);
    if (parent_ptr == NULL)
    {
        return;
    }

    file_entry = g_sequence_get (parent_ptr);
    files = file_entry->files;

    /* Only swap loading -> empty if we saw no files yet at "done",
     * otherwise, toggle loading at first added file to the model.
     */
    if (!caja_directory_is_not_empty (directory) &&
            g_sequence_get_length (files) == 1)
    {
        dummy_ptr = g_sequence_get_iter_at_pos (file_entry->files, 0);
        dummy_entry = g_sequence_get (dummy_ptr);
        if (dummy_entry->file == NULL)
        {
            /* was the dummy file */
            file_entry->loaded = 1;

            iter.stamp = model->details->stamp;
            iter.user_data = dummy_ptr;

            path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
            gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
            gtk_tree_path_free (path);
        }
    }
}

static void
refresh_row (gpointer data,
             gpointer user_data)
{
    CajaFile *file;
    FMListModel *model;
    GList *iters, *l;
    GtkTreePath *path;

    model = user_data;
    file = data;

    iters = fm_list_model_get_all_iters_for_file (model, file);
    for (l = iters; l != NULL; l = l->next)
    {
        path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), l->data);
        gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, l->data);

        gtk_tree_path_free (path);
    }

    g_list_free_full (iters, g_free);
}

void
fm_list_model_set_highlight_for_files (FMListModel *model,
                                       GList *files)
{
    if (model->details->highlight_files != NULL)
    {
        g_list_foreach (model->details->highlight_files,
                        refresh_row, model);
        caja_file_list_free (model->details->highlight_files);
        model->details->highlight_files = NULL;
    }

    if (files != NULL)
    {
        model->details->highlight_files = caja_file_list_copy (files);
        g_list_foreach (model->details->highlight_files,
                        refresh_row, model);

    }
}
