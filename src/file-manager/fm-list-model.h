/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-list-model.h - a GtkTreeModel for file lists.

   Copyright (C) 2001, 2002 Anders Carlsson

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

   Authors: Anders Carlsson <andersca@gnu.org>
*/

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <libcaja-private/caja-file.h>
#include <libcaja-private/caja-directory.h>
#include <libcaja-extension/caja-column.h>

#ifndef FM_LIST_MODEL_H
#define FM_LIST_MODEL_H

#define FM_TYPE_LIST_MODEL fm_list_model_get_type()
#define FM_LIST_MODEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), FM_TYPE_LIST_MODEL, FMListModel))
#define FM_LIST_MODEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), FM_TYPE_LIST_MODEL, FMListModelClass))
#define FM_IS_LIST_MODEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FM_TYPE_LIST_MODEL))
#define FM_IS_LIST_MODEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), FM_TYPE_LIST_MODEL))
#define FM_LIST_MODEL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), FM_TYPE_LIST_MODEL, FMListModelClass))

enum
{
    FM_LIST_MODEL_FILE_COLUMN,
    FM_LIST_MODEL_SUBDIRECTORY_COLUMN,
    FM_LIST_MODEL_SMALLEST_ICON_COLUMN,
    FM_LIST_MODEL_SMALLER_ICON_COLUMN,
    FM_LIST_MODEL_SMALL_ICON_COLUMN,
    FM_LIST_MODEL_STANDARD_ICON_COLUMN,
    FM_LIST_MODEL_LARGE_ICON_COLUMN,
    FM_LIST_MODEL_LARGER_ICON_COLUMN,
    FM_LIST_MODEL_LARGEST_ICON_COLUMN,
    FM_LIST_MODEL_FILE_NAME_IS_EDITABLE_COLUMN,
    FM_LIST_MODEL_NUM_COLUMNS
};

typedef struct FMListModelDetails FMListModelDetails;

typedef struct FMListModel
{
    GObject parent_instance;
    FMListModelDetails *details;
} FMListModel;

typedef struct
{
    GObjectClass parent_class;

    void (* subdirectory_unloaded)(FMListModel *model,
                                   CajaDirectory *subdirectory);
} FMListModelClass;

GType    fm_list_model_get_type                          (void);
gboolean fm_list_model_add_file                          (FMListModel          *model,
        CajaFile         *file,
        CajaDirectory    *directory);
void     fm_list_model_file_changed                      (FMListModel          *model,
        CajaFile         *file,
        CajaDirectory    *directory);
gboolean fm_list_model_is_empty                          (FMListModel          *model);
guint    fm_list_model_get_length                        (FMListModel          *model);
void     fm_list_model_remove_file                       (FMListModel          *model,
        CajaFile         *file,
        CajaDirectory    *directory);
void     fm_list_model_clear                             (FMListModel          *model);
gboolean fm_list_model_get_tree_iter_from_file           (FMListModel          *model,
        CajaFile         *file,
        CajaDirectory    *directory,
        GtkTreeIter          *iter);
GList *  fm_list_model_get_all_iters_for_file            (FMListModel          *model,
        CajaFile         *file);
gboolean fm_list_model_get_first_iter_for_file           (FMListModel          *model,
        CajaFile         *file,
        GtkTreeIter          *iter);
void     fm_list_model_set_should_sort_directories_first (FMListModel          *model,
        gboolean              sort_directories_first);

int      fm_list_model_get_sort_column_id_from_attribute (FMListModel *model,
        GQuark       attribute);
GQuark   fm_list_model_get_attribute_from_sort_column_id (FMListModel *model,
        int sort_column_id);
void     fm_list_model_sort_files                        (FMListModel *model,
        GList **files);

CajaZoomLevel fm_list_model_get_zoom_level_from_column_id (int               column);
int               fm_list_model_get_column_id_from_zoom_level (CajaZoomLevel zoom_level);

CajaFile *    fm_list_model_file_for_path (FMListModel *model, GtkTreePath *path);
gboolean          fm_list_model_load_subdirectory (FMListModel *model, GtkTreePath *path, CajaDirectory **directory);
void              fm_list_model_unload_subdirectory (FMListModel *model, GtkTreeIter *iter);

void              fm_list_model_set_drag_view (FMListModel *model,
        GtkTreeView *view,
        int begin_x,
        int begin_y);

GtkTargetList *   fm_list_model_get_drag_target_list (void);

int               fm_list_model_compare_func (FMListModel *model,
        CajaFile *file1,
        CajaFile *file2);


int               fm_list_model_add_column (FMListModel *model,
        CajaColumn *column);
int               fm_list_model_get_column_number (FMListModel *model,
        const char *column_name);

void              fm_list_model_subdirectory_done_loading (FMListModel       *model,
        CajaDirectory *directory);

void              fm_list_model_set_highlight_for_files (FMListModel *model,
        GList *files);

#endif /* FM_LIST_MODEL_H */
