/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-list-view.c - implementation of list view of directory.

   Copyright (C) 2000 Eazel, Inc.
   Copyright (C) 2001, 2002 Anders Carlsson <andersca@gnu.org>

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

   Authors: John Sullivan <sullivan@eazel.com>
            Anders Carlsson <andersca@gnu.org>
	    David Emory Watson <dwatson@cs.ucr.edu>
*/

#include <config.h>
#include "fm-list-view.h"

#include <string.h>
#include "fm-error-reporting.h"
#include "fm-list-model.h"
#include <string.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-gdk-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <libegg/eggtreemultidnd.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <libcaja-extension/caja-column-provider.h>
#include <libcaja-private/caja-clipboard-monitor.h>
#include <libcaja-private/caja-column-chooser.h>
#include <libcaja-private/caja-column-utilities.h>
#include <libcaja-private/caja-debug-log.h>
#include <libcaja-private/caja-directory-background.h>
#include <libcaja-private/caja-dnd.h>
#include <libcaja-private/caja-file-dnd.h>
#include <libcaja-private/caja-file-utilities.h>
#include <libcaja-private/caja-ui-utilities.h>
#include <libcaja-private/caja-global-preferences.h>
#include <libcaja-private/caja-icon-dnd.h>
#include <libcaja-private/caja-metadata.h>
#include <libcaja-private/caja-module.h>
#include <libcaja-private/caja-tree-view-drag-dest.h>
#include <libcaja-private/caja-view-factory.h>
#include <libcaja-private/caja-clipboard.h>
#include <libcaja-private/caja-cell-renderer-text-ellipsized.h>

struct FMListViewDetails
{
    GtkTreeView *tree_view;
    FMListModel *model;
    GtkActionGroup *list_action_group;
    guint list_merge_id;

    GtkTreeViewColumn   *file_name_column;
    int file_name_column_num;

    GtkCellRendererPixbuf *pixbuf_cell;
    GtkCellRendererText   *file_name_cell;
    GList *cells;
    GtkCellEditable *editable_widget;

    CajaZoomLevel zoom_level;

    CajaTreeViewDragDest *drag_dest;

    GtkTreePath *double_click_path[2]; /* Both clicks in a double click need to be on the same row */

    GtkTreePath *new_selection_path;   /* Path of the new selection after removing a file */

    GtkTreePath *hover_path;

    guint drag_button;
    int drag_x;
    int drag_y;

    gboolean drag_started;

    gboolean ignore_button_release;

    gboolean row_selected_on_button_down;

    gboolean menus_ready;

    GHashTable *columns;
    GtkWidget *column_editor;

    char *original_name;

    CajaFile *renaming_file;
    gboolean rename_done;
    guint renaming_file_activate_timeout;

    gulong clipboard_handler_id;

    GQuark last_sort_attr;
};

struct SelectionForeachData
{
    GList *list;
    GtkTreeSelection *selection;
};

/*
 * The row height should be large enough to not clip emblems.
 * Computing this would be costly, so we just choose a number
 * that works well with the set of emblems we've designed.
 */
#define LIST_VIEW_MINIMUM_ROW_HEIGHT	28

/* We wait two seconds after row is collapsed to unload the subdirectory */
#define COLLAPSE_TO_UNLOAD_DELAY 2

/* Wait for the rename to end when activating a file being renamed */
#define WAIT_FOR_RENAME_ON_ACTIVATE 200

static int                      click_policy_auto_value;
static CajaFileSortType         default_sort_order_auto_value;
static gboolean			default_sort_reversed_auto_value;
static CajaZoomLevel        default_zoom_level_auto_value;
static char **                  default_visible_columns_auto_value;
static char **                  default_column_order_auto_value;
static GdkCursor *              hand_cursor = NULL;

static GtkTargetList *          source_target_list = NULL;

static GList *fm_list_view_get_selection                   (FMDirectoryView   *view);
static GList *fm_list_view_get_selection_for_file_transfer (FMDirectoryView   *view);
static void   fm_list_view_set_zoom_level                  (FMListView        *view,
        CajaZoomLevel  new_level,
        gboolean           always_set_level);
static void   fm_list_view_scale_font_size                 (FMListView        *view,
        CajaZoomLevel  new_level);
static void   fm_list_view_scroll_to_file                  (FMListView        *view,
        CajaFile      *file);
static void   fm_list_view_iface_init                      (CajaViewIface *iface);
static void   fm_list_view_rename_callback                 (CajaFile      *file,
        GFile             *result_location,
        GError            *error,
        gpointer           callback_data);


G_DEFINE_TYPE_WITH_CODE (FMListView, fm_list_view, FM_TYPE_DIRECTORY_VIEW,
                         G_IMPLEMENT_INTERFACE (CAJA_TYPE_VIEW,
                                 fm_list_view_iface_init));

static const char * default_trash_visible_columns[] =
{
    "name", "size", "type", "trashed_on", "trash_orig_path", NULL
};

static const char * default_trash_columns_order[] =
{
    "name", "size", "type", "trashed_on", "trash_orig_path", NULL
};

/* for EEL_CALL_PARENT */
#define parent_class fm_list_view_parent_class

static const gchar*
get_default_sort_order (CajaFile *file, gboolean *reversed)
{
    const gchar *retval;
    const char *attributes[] = {
        "name", /* is really "manually" which doesn't apply to lists */
        "name",
        "uri",
        "size",
        "type",
        "date_modified",
        "date_accessed",
        "emblems",
        "trashed_on",
        NULL
    };

    retval = caja_file_get_default_sort_attribute (file, reversed);

    if (retval == NULL)
    {
        retval = attributes[default_sort_order_auto_value];
        *reversed = default_sort_reversed_auto_value;
    }

    return retval;
}

static void
list_selection_changed_callback (GtkTreeSelection *selection, gpointer user_data)
{
    FMDirectoryView *view;

    view = FM_DIRECTORY_VIEW (user_data);

    fm_directory_view_notify_selection_changed (view);
}

/* Move these to eel? */

static void
tree_selection_foreach_set_boolean (GtkTreeModel *model,
                                    GtkTreePath *path,
                                    GtkTreeIter *iter,
                                    gpointer callback_data)
{
    * (gboolean *) callback_data = TRUE;
}

static gboolean
tree_selection_not_empty (GtkTreeSelection *selection)
{
    gboolean not_empty;

    not_empty = FALSE;
    gtk_tree_selection_selected_foreach (selection,
                                         tree_selection_foreach_set_boolean,
                                         &not_empty);
    return not_empty;
}

static gboolean
tree_view_has_selection (GtkTreeView *view)
{
    return tree_selection_not_empty (gtk_tree_view_get_selection (view));
}

static void
activate_selected_items (FMListView *view)
{
    GList *file_list;

    file_list = fm_list_view_get_selection (FM_DIRECTORY_VIEW (view));


    if (view->details->renaming_file)
    {
        /* We're currently renaming a file, wait until the rename is
           finished, or the activation uri will be wrong */
        if (view->details->renaming_file_activate_timeout == 0)
        {
            view->details->renaming_file_activate_timeout =
                g_timeout_add (WAIT_FOR_RENAME_ON_ACTIVATE, (GSourceFunc) activate_selected_items, view);
        }
        return;
    }

    if (view->details->renaming_file_activate_timeout != 0)
    {
        g_source_remove (view->details->renaming_file_activate_timeout);
        view->details->renaming_file_activate_timeout = 0;
    }

    fm_directory_view_activate_files (FM_DIRECTORY_VIEW (view),
                                      file_list,
                                      CAJA_WINDOW_OPEN_ACCORDING_TO_MODE,
                                      0,
                                      TRUE);
    caja_file_list_free (file_list);

}

static void
activate_selected_items_alternate (FMListView *view,
                                   CajaFile *file,
                                   gboolean open_in_tab)
{
    GList *file_list;
    CajaWindowOpenFlags flags;

    flags = CAJA_WINDOW_OPEN_FLAG_CLOSE_BEHIND;

    if (open_in_tab)
    {
        flags |= CAJA_WINDOW_OPEN_FLAG_NEW_TAB;
    }
    else
    {
        flags |= CAJA_WINDOW_OPEN_FLAG_NEW_WINDOW;
    }

    if (file != NULL)
    {
        caja_file_ref (file);
        file_list = g_list_prepend (NULL, file);
    }
    else
    {
        file_list = fm_list_view_get_selection (FM_DIRECTORY_VIEW (view));
    }
    fm_directory_view_activate_files (FM_DIRECTORY_VIEW (view),
                                      file_list,
                                      CAJA_WINDOW_OPEN_ACCORDING_TO_MODE,
                                      flags,
                                      TRUE);
    caja_file_list_free (file_list);

}

static gboolean
button_event_modifies_selection (GdkEventButton *event)
{
    return (event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) != 0;
}

static void
fm_list_view_did_not_drag (FMListView *view,
                           GdkEventButton *event)
{
    GtkTreeView *tree_view;
    GtkTreeSelection *selection;
    GtkTreePath *path;

    tree_view = view->details->tree_view;
    selection = gtk_tree_view_get_selection (tree_view);

    if (gtk_tree_view_get_path_at_pos (tree_view, event->x, event->y,
                                       &path, NULL, NULL, NULL))
    {
        if ((event->button == 1 || event->button == 2)
                && ((event->state & GDK_CONTROL_MASK) != 0 ||
                    (event->state & GDK_SHIFT_MASK) == 0)
                && view->details->row_selected_on_button_down)
        {
            if (!button_event_modifies_selection (event))
            {
                gtk_tree_selection_unselect_all (selection);
                gtk_tree_selection_select_path (selection, path);
            }
            else
            {
                gtk_tree_selection_unselect_path (selection, path);
            }
        }

        if ((click_policy_auto_value == CAJA_CLICK_POLICY_SINGLE)
                && !button_event_modifies_selection(event))
        {
            if (event->button == 1)
            {
                activate_selected_items (view);
            }
            else if (event->button == 2)
            {
                activate_selected_items_alternate (view, NULL, TRUE);
            }
        }
        gtk_tree_path_free (path);
    }

}

static void
drag_data_get_callback (GtkWidget *widget,
                        GdkDragContext *context,
                        GtkSelectionData *selection_data,
                        guint info,
                        guint time)
{
    GtkTreeView *tree_view;
    GtkTreeModel *model;
    GList *ref_list;

    tree_view = GTK_TREE_VIEW (widget);

    model = gtk_tree_view_get_model (tree_view);

    if (model == NULL)
    {
        return;
    }

    ref_list = g_object_get_data (G_OBJECT (context), "drag-info");

    if (ref_list == NULL)
    {
        return;
    }

    if (EGG_IS_TREE_MULTI_DRAG_SOURCE (model))
    {
        egg_tree_multi_drag_source_drag_data_get (EGG_TREE_MULTI_DRAG_SOURCE (model),
                ref_list,
                selection_data);
    }
}

static void
filtered_selection_foreach (GtkTreeModel *model,
                            GtkTreePath *path,
                            GtkTreeIter *iter,
                            gpointer data)
{
    struct SelectionForeachData *selection_data;
    GtkTreeIter parent;
    GtkTreeIter child;

    selection_data = data;

    /* If the parent folder is also selected, don't include this file in the
     * file operation, since that would copy it to the toplevel target instead
     * of keeping it as a child of the copied folder
     */
    child = *iter;
    while (gtk_tree_model_iter_parent (model, &parent, &child))
    {
        if (gtk_tree_selection_iter_is_selected (selection_data->selection,
                &parent))
        {
            return;
        }
        child = parent;
    }

    selection_data->list = g_list_prepend (selection_data->list,
                                           gtk_tree_row_reference_new (model, path));
}

static GList *
get_filtered_selection_refs (GtkTreeView *tree_view)
{
    struct SelectionForeachData selection_data;

    selection_data.list = NULL;
    selection_data.selection = gtk_tree_view_get_selection (tree_view);

    gtk_tree_selection_selected_foreach (selection_data.selection,
                                         filtered_selection_foreach,
                                         &selection_data);
    return g_list_reverse (selection_data.list);
}

static void
ref_list_free (GList *ref_list)
{
    g_list_foreach (ref_list, (GFunc) gtk_tree_row_reference_free, NULL);
    g_list_free (ref_list);
}

static void
stop_drag_check (FMListView *view)
{
    view->details->drag_button = 0;
}

static GdkPixbuf *
get_drag_pixbuf (FMListView *view)
{
    GtkTreeModel *model;
    GtkTreePath *path;
    GtkTreeIter iter;
    GdkPixbuf *ret;
    GdkRectangle cell_area;

    ret = NULL;

    if (gtk_tree_view_get_path_at_pos (view->details->tree_view,
                                       view->details->drag_x,
                                       view->details->drag_y,
                                       &path, NULL, NULL, NULL))
    {
        model = gtk_tree_view_get_model (view->details->tree_view);
        gtk_tree_model_get_iter (model, &iter, path);
        gtk_tree_model_get (model, &iter,
                            fm_list_model_get_column_id_from_zoom_level (view->details->zoom_level),
                            &ret,
                            -1);

        gtk_tree_view_get_cell_area (view->details->tree_view,
                                     path,
                                     view->details->file_name_column,
                                     &cell_area);

        gtk_tree_path_free (path);
    }

    return ret;
}

static void
drag_begin_callback (GtkWidget *widget,
                     GdkDragContext *context,
                     FMListView *view)
{
    GList *ref_list;
    GdkPixbuf *pixbuf;

    pixbuf = get_drag_pixbuf (view);
    if (pixbuf)
    {
        gtk_drag_set_icon_pixbuf (context,
                                  pixbuf,
                                  0, 0);
        g_object_unref (pixbuf);
    }
    else
    {
        gtk_drag_set_icon_default (context);
    }

    stop_drag_check (view);
    view->details->drag_started = TRUE;

    ref_list = get_filtered_selection_refs (GTK_TREE_VIEW (widget));
    g_object_set_data_full (G_OBJECT (context),
                            "drag-info",
                            ref_list,
                            (GDestroyNotify)ref_list_free);
}

static gboolean
motion_notify_callback (GtkWidget *widget,
                        GdkEventMotion *event,
                        gpointer callback_data)
{
    FMListView *view;
    GdkDragContext *context;

    view = FM_LIST_VIEW (callback_data);

    if (event->window != gtk_tree_view_get_bin_window (GTK_TREE_VIEW (widget)))
    {
        return FALSE;
    }

    if (click_policy_auto_value == CAJA_CLICK_POLICY_SINGLE)
    {
        GtkTreePath *old_hover_path;

        old_hover_path = view->details->hover_path;
        gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (widget),
                                       event->x, event->y,
                                       &view->details->hover_path,
                                       NULL, NULL, NULL);

        if ((old_hover_path != NULL) != (view->details->hover_path != NULL))
        {
            if (view->details->hover_path != NULL)
            {
                gdk_window_set_cursor (gtk_widget_get_window (widget), hand_cursor);
            }
            else
            {
                gdk_window_set_cursor (gtk_widget_get_window (widget), NULL);
            }
        }

        if (old_hover_path != NULL)
        {
            gtk_tree_path_free (old_hover_path);
        }
    }

    if (view->details->drag_button != 0)
    {
        if (!source_target_list)
        {
            source_target_list = fm_list_model_get_drag_target_list ();
        }

        if (gtk_drag_check_threshold (widget,
                                      view->details->drag_x,
                                      view->details->drag_y,
                                      event->x,
                                      event->y))
        {
            context = gtk_drag_begin
                      (widget,
                       source_target_list,
                       GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK | GDK_ACTION_ASK,
                       view->details->drag_button,
                       (GdkEvent*)event);
        }
        return TRUE;
    }

    return FALSE;
}

static gboolean
leave_notify_callback (GtkWidget *widget,
                       GdkEventCrossing *event,
                       gpointer callback_data)
{
    FMListView *view;

    view = FM_LIST_VIEW (callback_data);

    if (click_policy_auto_value == CAJA_CLICK_POLICY_SINGLE &&
            view->details->hover_path != NULL)
    {
        gtk_tree_path_free (view->details->hover_path);
        view->details->hover_path = NULL;
    }

    return FALSE;
}

static gboolean
enter_notify_callback (GtkWidget *widget,
                       GdkEventCrossing *event,
                       gpointer callback_data)
{
    FMListView *view;

    view = FM_LIST_VIEW (callback_data);

    if (click_policy_auto_value == CAJA_CLICK_POLICY_SINGLE)
    {
        if (view->details->hover_path != NULL)
        {
            gtk_tree_path_free (view->details->hover_path);
        }

        gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (widget),
                                       event->x, event->y,
                                       &view->details->hover_path,
                                       NULL, NULL, NULL);

        if (view->details->hover_path != NULL)
        {
            gdk_window_set_cursor (gtk_widget_get_window (widget), hand_cursor);
        }
    }

    return FALSE;
}

static void
do_popup_menu (GtkWidget *widget, FMListView *view, GdkEventButton *event)
{
    if (tree_view_has_selection (GTK_TREE_VIEW (widget)))
    {
        fm_directory_view_pop_up_selection_context_menu (FM_DIRECTORY_VIEW (view), event);
    }
    else
    {
        fm_directory_view_pop_up_background_context_menu (FM_DIRECTORY_VIEW (view), event);
    }
}

static gboolean
button_press_callback (GtkWidget *widget, GdkEventButton *event, gpointer callback_data)
{
    FMListView *view;
    GtkTreeView *tree_view;
    GtkTreePath *path;
    gboolean call_parent;
    gboolean allow_drag;
    GtkTreeSelection *selection;
    GtkWidgetClass *tree_view_class;
    gint64 current_time;
    static gint64 last_click_time = 0;
    static int click_count = 0;
    int double_click_time;
    int expander_size, horizontal_separator;
    gboolean on_expander;

    view = FM_LIST_VIEW (callback_data);
    tree_view = GTK_TREE_VIEW (widget);
    tree_view_class = GTK_WIDGET_GET_CLASS (tree_view);
    selection = gtk_tree_view_get_selection (tree_view);

    if (event->window != gtk_tree_view_get_bin_window (tree_view))
    {
        return FALSE;
    }

    fm_list_model_set_drag_view
    (FM_LIST_MODEL (gtk_tree_view_get_model (tree_view)),
     tree_view,
     event->x, event->y);

    g_object_get (G_OBJECT (gtk_widget_get_settings (widget)),
                  "gtk-double-click-time", &double_click_time,
                  NULL);

    /* Determine click count */
    current_time = eel_get_system_time ();
    if (current_time - last_click_time < double_click_time * 1000)
    {
        click_count++;
    }
    else
    {
        click_count = 0;
    }

    /* Stash time for next compare */
    last_click_time = current_time;

    /* Ignore double click if we are in single click mode */
    if (click_policy_auto_value == CAJA_CLICK_POLICY_SINGLE && click_count >= 2)
    {
        return TRUE;
    }

    view->details->ignore_button_release = FALSE;

    call_parent = TRUE;
    allow_drag = FALSE;
    if (gtk_tree_view_get_path_at_pos (tree_view, event->x, event->y,
                                       &path, NULL, NULL, NULL))
    {
        gtk_widget_style_get (widget,
                              "expander-size", &expander_size,
                              "horizontal-separator", &horizontal_separator,
                              NULL);
        /* TODO we should not hardcode this extra padding. It is
         * EXPANDER_EXTRA_PADDING from GtkTreeView.
         */
        expander_size += 4;
        on_expander = (event->x <= horizontal_separator / 2 +
                       gtk_tree_path_get_depth (path) * expander_size);

        /* Keep track of path of last click so double clicks only happen
         * on the same item */
        if ((event->button == 1 || event->button == 2)  &&
                event->type == GDK_BUTTON_PRESS)
        {
            if (view->details->double_click_path[1])
            {
                gtk_tree_path_free (view->details->double_click_path[1]);
            }
            view->details->double_click_path[1] = view->details->double_click_path[0];
            view->details->double_click_path[0] = gtk_tree_path_copy (path);
        }
        if (event->type == GDK_2BUTTON_PRESS)
        {
            /* Double clicking does not trigger a D&D action. */
            view->details->drag_button = 0;
            if (view->details->double_click_path[1] &&
                    gtk_tree_path_compare (view->details->double_click_path[0], view->details->double_click_path[1]) == 0 &&
                    !on_expander)
            {
                /* NOTE: Activation can actually destroy the view if we're switching */
                if (!button_event_modifies_selection (event))
                {
                    if ((event->button == 1 || event->button == 3))
                    {
                        activate_selected_items (view);
                    }
                    else if (event->button == 2)
                    {
                        activate_selected_items_alternate (view, NULL, TRUE);
                    }
                }
                else if (event->button == 1 &&
                         (event->state & GDK_SHIFT_MASK) != 0)
                {
                    CajaFile *file;
                    file = fm_list_model_file_for_path (view->details->model, path);
                    if (file != NULL)
                    {
                        activate_selected_items_alternate (view, file, TRUE);
                        caja_file_unref (file);
                    }
                }
            }
            else
            {
                tree_view_class->button_press_event (widget, event);
            }
        }
        else
        {

            /* We're going to filter out some situations where
             * we can't let the default code run because all
             * but one row would be would be deselected. We don't
             * want that; we want the right click menu or single
             * click to apply to everything that's currently selected. */

            if (event->button == 3 && gtk_tree_selection_path_is_selected (selection, path))
            {
                call_parent = FALSE;
            }

            if ((event->button == 1 || event->button == 2) &&
                    ((event->state & GDK_CONTROL_MASK) != 0 ||
                     (event->state & GDK_SHIFT_MASK) == 0))
            {
                view->details->row_selected_on_button_down = gtk_tree_selection_path_is_selected (selection, path);
                if (view->details->row_selected_on_button_down)
                {
                    call_parent = on_expander;
                    view->details->ignore_button_release = call_parent;
                }
                else if ((event->state & GDK_CONTROL_MASK) != 0)
                {
                    GList *selected_rows;
                    GList *l;

                    call_parent = FALSE;
                    if ((event->state & GDK_SHIFT_MASK) != 0)
                    {
                        GtkTreePath *cursor;
                        gtk_tree_view_get_cursor (tree_view, &cursor, NULL);
                        if (cursor != NULL)
                        {
                            gtk_tree_selection_select_range (selection, cursor, path);
                        }
                        else
                        {
                            gtk_tree_selection_select_path (selection, path);
                        }
                    }
                    else
                    {
                        gtk_tree_selection_select_path (selection, path);
                    }
                    selected_rows = gtk_tree_selection_get_selected_rows (selection, NULL);

                    /* This unselects everything */
                    gtk_tree_view_set_cursor (tree_view, path, NULL, FALSE);

                    /* So select it again */
                    l = selected_rows;
                    while (l != NULL)
                    {
                        GtkTreePath *p = l->data;
                        l = l->next;
                        gtk_tree_selection_select_path (selection, p);
                        gtk_tree_path_free (p);
                    }
                    g_list_free (selected_rows);
                }
                else
                {
                    view->details->ignore_button_release = on_expander;
                }
            }

            if (call_parent)
            {
                tree_view_class->button_press_event (widget, event);
            }
            else if (gtk_tree_selection_path_is_selected (selection, path))
            {
                gtk_widget_grab_focus (widget);
            }

            if ((event->button == 1 || event->button == 2) &&
                    event->type == GDK_BUTTON_PRESS)
            {
                view->details->drag_started = FALSE;
                view->details->drag_button = event->button;
                view->details->drag_x = event->x;
                view->details->drag_y = event->y;
            }

            if (event->button == 3)
            {
                do_popup_menu (widget, view, event);
            }
        }

        gtk_tree_path_free (path);
    }
    else
    {
        if ((event->button == 1 || event->button == 2)  &&
                event->type == GDK_BUTTON_PRESS)
        {
            if (view->details->double_click_path[1])
            {
                gtk_tree_path_free (view->details->double_click_path[1]);
            }
            view->details->double_click_path[1] = view->details->double_click_path[0];
            view->details->double_click_path[0] = NULL;
        }
        /* Deselect if people click outside any row. It's OK to
           let default code run; it won't reselect anything. */
        gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (tree_view));
        tree_view_class->button_press_event (widget, event);

        if (event->button == 3)
        {
            do_popup_menu (widget, view, event);
        }
    }

    /* We chained to the default handler in this method, so never
     * let the default handler run */
    return TRUE;
}

static gboolean
button_release_callback (GtkWidget *widget,
                         GdkEventButton *event,
                         gpointer callback_data)
{
    FMListView *view;

    view = FM_LIST_VIEW (callback_data);

    if (event->button == view->details->drag_button)
    {
        stop_drag_check (view);
        if (!view->details->drag_started &&
                !view->details->ignore_button_release)
        {
            fm_list_view_did_not_drag (view, event);
        }
    }
    return FALSE;
}

static gboolean
popup_menu_callback (GtkWidget *widget, gpointer callback_data)
{
    FMListView *view;

    view = FM_LIST_VIEW (callback_data);

    do_popup_menu (widget, view, NULL);

    return TRUE;
}

static void
subdirectory_done_loading_callback (CajaDirectory *directory, FMListView *view)
{
    fm_list_model_subdirectory_done_loading (view->details->model, directory);
}

static void
row_expanded_callback (GtkTreeView *treeview, GtkTreeIter *iter, GtkTreePath *path, gpointer callback_data)
{
    FMListView *view;
    CajaDirectory *directory;

    view = FM_LIST_VIEW (callback_data);

    if (fm_list_model_load_subdirectory (view->details->model, path, &directory))
    {
        char *uri;

        uri = caja_directory_get_uri (directory);
        caja_debug_log (FALSE, CAJA_DEBUG_LOG_DOMAIN_USER,
                        "list view row expanded window=%p: %s",
                        fm_directory_view_get_containing_window (FM_DIRECTORY_VIEW (view)),
                        uri);
        g_free (uri);

        fm_directory_view_add_subdirectory (FM_DIRECTORY_VIEW (view), directory);

        if (caja_directory_are_all_files_seen (directory))
        {
            fm_list_model_subdirectory_done_loading (view->details->model,
                    directory);
        }
        else
        {
            g_signal_connect_object (directory, "done_loading",
                                     G_CALLBACK (subdirectory_done_loading_callback),
                                     view, 0);
        }

        caja_directory_unref (directory);
    }
}

struct UnloadDelayData
{
    CajaFile *file;
    CajaDirectory *directory;
    FMListView *view;
};

static gboolean
unload_file_timeout (gpointer data)
{
    struct UnloadDelayData *unload_data = data;
    GtkTreeIter iter;
    FMListModel *model;
    GtkTreePath *path;

    if (unload_data->view != NULL)
    {
        model = unload_data->view->details->model;
        if (fm_list_model_get_tree_iter_from_file (model,
                unload_data->file,
                unload_data->directory,
                &iter))
        {
            path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
            if (!gtk_tree_view_row_expanded (unload_data->view->details->tree_view,
                                             path))
            {
                fm_list_model_unload_subdirectory (model, &iter);
            }
            gtk_tree_path_free (path);
        }
    }

    eel_remove_weak_pointer (&unload_data->view);

    if (unload_data->directory)
    {
        caja_directory_unref (unload_data->directory);
    }
    caja_file_unref (unload_data->file);
    g_free (unload_data);
    return FALSE;
}

static void
row_collapsed_callback (GtkTreeView *treeview, GtkTreeIter *iter, GtkTreePath *path, gpointer callback_data)
{
    FMListView *view;
    CajaFile *file;
    CajaDirectory *directory;
    GtkTreeIter parent;
    struct UnloadDelayData *unload_data;
    GtkTreeModel *model;
    char *uri;

    view = FM_LIST_VIEW (callback_data);
    model = GTK_TREE_MODEL (view->details->model);

    gtk_tree_model_get (model, iter,
                        FM_LIST_MODEL_FILE_COLUMN, &file,
                        -1);

    directory = NULL;
    if (gtk_tree_model_iter_parent (model, &parent, iter))
    {
        gtk_tree_model_get (model, &parent,
                            FM_LIST_MODEL_SUBDIRECTORY_COLUMN, &directory,
                            -1);
    }


    uri = caja_file_get_uri (file);
    caja_debug_log (FALSE, CAJA_DEBUG_LOG_DOMAIN_USER,
                    "list view row collapsed window=%p: %s",
                    fm_directory_view_get_containing_window (FM_DIRECTORY_VIEW (view)),
                    uri);
    g_free (uri);

    unload_data = g_new (struct UnloadDelayData, 1);
    unload_data->view = view;
    unload_data->file = file;
    unload_data->directory = directory;

    eel_add_weak_pointer (&unload_data->view);

    g_timeout_add_seconds (COLLAPSE_TO_UNLOAD_DELAY,
                           unload_file_timeout,
                           unload_data);
}

static void
row_activated_callback (GtkTreeView *treeview, GtkTreePath *path,
                        GtkTreeViewColumn *column, FMListView *view)
{
    activate_selected_items (view);
}

static void
subdirectory_unloaded_callback (FMListModel *model,
                                CajaDirectory *directory,
                                gpointer callback_data)
{
    FMListView *view;

    g_return_if_fail (FM_IS_LIST_MODEL (model));
    g_return_if_fail (CAJA_IS_DIRECTORY (directory));

    view = FM_LIST_VIEW(callback_data);

    g_signal_handlers_disconnect_by_func (directory,
                                          G_CALLBACK (subdirectory_done_loading_callback),
                                          view);
    fm_directory_view_remove_subdirectory (FM_DIRECTORY_VIEW (view), directory);
}

static gboolean
key_press_callback (GtkWidget *widget, GdkEventKey *event, gpointer callback_data)
{
    FMDirectoryView *view;
    GdkEventButton button_event = { 0 };
    gboolean handled;
    GtkTreeView *tree_view;
    GtkTreePath *path;

    tree_view = GTK_TREE_VIEW (widget);

    view = FM_DIRECTORY_VIEW (callback_data);
    handled = FALSE;

    switch (event->keyval)
    {
    case GDK_KEY_F10:
        if (event->state & GDK_CONTROL_MASK)
        {
            fm_directory_view_pop_up_background_context_menu (view, &button_event);
            handled = TRUE;
        }
        break;
    case GDK_KEY_Right:
        gtk_tree_view_get_cursor (tree_view, &path, NULL);
        if (path)
        {
            gtk_tree_view_expand_row (tree_view, path, FALSE);
            gtk_tree_path_free (path);
        }
        handled = TRUE;
        break;
	case GDK_KEY_Left:
        gtk_tree_view_get_cursor (tree_view, &path, NULL);
        if (path)
        {
            gtk_tree_view_collapse_row (tree_view, path);
            gtk_tree_path_free (path);
        }
        handled = TRUE;
        break;
    case GDK_KEY_space:
        if (event->state & GDK_CONTROL_MASK)
        {
            handled = FALSE;
            break;
        }
        if (!gtk_widget_has_focus (GTK_WIDGET (FM_LIST_VIEW (view)->details->tree_view)))
        {
            handled = FALSE;
            break;
        }
        if ((event->state & GDK_SHIFT_MASK) != 0)
        {
            activate_selected_items_alternate (FM_LIST_VIEW (view), NULL, TRUE);
        }
        else
        {
            activate_selected_items (FM_LIST_VIEW (view));
        }
        handled = TRUE;
        break;
    case GDK_KEY_Return:
    case GDK_KEY_KP_Enter:
        if ((event->state & GDK_SHIFT_MASK) != 0)
        {
            activate_selected_items_alternate (FM_LIST_VIEW (view), NULL, TRUE);
        }
        else
        {
            activate_selected_items (FM_LIST_VIEW (view));
        }
        handled = TRUE;
        break;
	case GDK_KEY_v:
        /* Eat Control + v to not enable type ahead */
        if ((event->state & GDK_CONTROL_MASK) != 0)
        {
            handled = TRUE;
        }
        break;

    default:
        handled = FALSE;
    }

    return handled;
}

static void
fm_list_view_reveal_selection (FMDirectoryView *view)
{
    GList *selection;

    g_return_if_fail (FM_IS_LIST_VIEW (view));

    selection = fm_directory_view_get_selection (view);

    /* Make sure at least one of the selected items is scrolled into view */
    if (selection != NULL)
    {
        FMListView *list_view;
        CajaFile *file;
        GtkTreeIter iter;
        GtkTreePath *path;

        list_view = FM_LIST_VIEW (view);
        file = selection->data;
        if (fm_list_model_get_first_iter_for_file (list_view->details->model, file, &iter))
        {
            path = gtk_tree_model_get_path (GTK_TREE_MODEL (list_view->details->model), &iter);

            gtk_tree_view_scroll_to_cell (list_view->details->tree_view, path, NULL, FALSE, 0.0, 0.0);

            gtk_tree_path_free (path);
        }
    }

    caja_file_list_free (selection);
}

static gboolean
sort_criterion_changes_due_to_user (GtkTreeView *tree_view)
{
    GList *columns, *p;
    GtkTreeViewColumn *column;
    GSignalInvocationHint *ihint;
    unsigned int sort_signal_id;
    gboolean ret;

    sort_signal_id = g_signal_lookup ("clicked", gtk_tree_view_column_get_type ());

    ret = FALSE;

    columns = gtk_tree_view_get_columns (tree_view);
    for (p = columns; p != NULL; p = p->next)
    {
        column = p->data;
        ihint = g_signal_get_invocation_hint (column);
        if (ihint != NULL)
        {
            ret = TRUE;
            break;
        }
    }
    g_list_free (columns);

    return ret;
}

static void
sort_column_changed_callback (GtkTreeSortable *sortable,
                              FMListView *view)
{
    CajaFile *file;
    gint sort_column_id, default_sort_column_id;
    GtkSortType reversed;
    GQuark sort_attr, default_sort_attr;
    char *reversed_attr, *default_reversed_attr;
    gboolean default_sort_reversed;

    file = fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (view));

    gtk_tree_sortable_get_sort_column_id (sortable, &sort_column_id, &reversed);
    sort_attr = fm_list_model_get_attribute_from_sort_column_id (view->details->model, sort_column_id);

    default_sort_column_id = fm_list_model_get_sort_column_id_from_attribute (view->details->model,
                             g_quark_from_string (get_default_sort_order (file, &default_sort_reversed)));
    default_sort_attr = fm_list_model_get_attribute_from_sort_column_id (view->details->model, default_sort_column_id);
    caja_file_set_metadata (file, CAJA_METADATA_KEY_LIST_VIEW_SORT_COLUMN,
                            g_quark_to_string (default_sort_attr), g_quark_to_string (sort_attr));

    default_reversed_attr = (default_sort_reversed ? "true" : "false");

    if (view->details->last_sort_attr != sort_attr &&
            sort_criterion_changes_due_to_user (view->details->tree_view))
    {
        /* at this point, the sort order is always GTK_SORT_ASCENDING, if the sort column ID
         * switched. Invert the sort order, if it's the default criterion with a reversed preference,
         * or if it makes sense for the attribute (i.e. date). */
        if (sort_attr == default_sort_attr)
        {
            /* use value from preferences */
            reversed = g_settings_get_boolean (caja_preferences, CAJA_PREFERENCES_DEFAULT_SORT_IN_REVERSE_ORDER);
        }
        else
        {
            reversed = caja_file_is_date_sort_attribute_q (sort_attr);
        }

        if (reversed)
        {
            g_signal_handlers_block_by_func (sortable, sort_column_changed_callback, view);
            gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (view->details->model),
                                                  sort_column_id,
                                                  GTK_SORT_DESCENDING);
            g_signal_handlers_unblock_by_func (sortable, sort_column_changed_callback, view);
        }
    }


    reversed_attr = (reversed ? "true" : "false");
    caja_file_set_metadata (file, CAJA_METADATA_KEY_LIST_VIEW_SORT_REVERSED,
                            default_reversed_attr, reversed_attr);

    /* Make sure selected item(s) is visible after sort */
    fm_list_view_reveal_selection (FM_DIRECTORY_VIEW (view));

    view->details->last_sort_attr = sort_attr;
}

static void
cell_renderer_editing_started_cb (GtkCellRenderer *renderer,
                                  GtkCellEditable *editable,
                                  const gchar *path_str,
                                  FMListView *list_view)
{
    GtkEntry *entry;

    entry = GTK_ENTRY (editable);
    list_view->details->editable_widget = editable;

    /* Free a previously allocated original_name */
    g_free (list_view->details->original_name);

    list_view->details->original_name = g_strdup (gtk_entry_get_text (entry));

    caja_clipboard_set_up_editable
    (GTK_EDITABLE (entry),
     fm_directory_view_get_ui_manager (FM_DIRECTORY_VIEW (list_view)),
     FALSE);
}

static void
cell_renderer_editing_canceled (GtkCellRendererText *cell,
                                FMListView          *view)
{
    view->details->editable_widget = NULL;

    fm_directory_view_unfreeze_updates (FM_DIRECTORY_VIEW (view));
}

static void
cell_renderer_edited (GtkCellRendererText *cell,
                      const char          *path_str,
                      const char          *new_text,
                      FMListView          *view)
{
    GtkTreePath *path;
    CajaFile *file;
    GtkTreeIter iter;

    view->details->editable_widget = NULL;

    /* Don't allow a rename with an empty string. Revert to original
     * without notifying the user.
     */
    if (new_text[0] == '\0')
    {
        g_object_set (G_OBJECT (view->details->file_name_cell),
                      "editable", FALSE,
                      NULL);
        fm_directory_view_unfreeze_updates (FM_DIRECTORY_VIEW (view));
        return;
    }

    path = gtk_tree_path_new_from_string (path_str);

    gtk_tree_model_get_iter (GTK_TREE_MODEL (view->details->model),
                             &iter, path);

    gtk_tree_path_free (path);

    gtk_tree_model_get (GTK_TREE_MODEL (view->details->model),
                        &iter,
                        FM_LIST_MODEL_FILE_COLUMN, &file,
                        -1);

    /* Only rename if name actually changed */
    if (strcmp (new_text, view->details->original_name) != 0)
    {
        view->details->renaming_file = caja_file_ref (file);
        view->details->rename_done = FALSE;
        fm_rename_file (file, new_text, fm_list_view_rename_callback, g_object_ref (view));
        g_free (view->details->original_name);
        view->details->original_name = g_strdup (new_text);
    }

    caja_file_unref (file);

    /*We're done editing - make the filename-cells readonly again.*/
    g_object_set (G_OBJECT (view->details->file_name_cell),
                  "editable", FALSE,
                  NULL);

    fm_directory_view_unfreeze_updates (FM_DIRECTORY_VIEW (view));
}

static char *
get_root_uri_callback (CajaTreeViewDragDest *dest,
                       gpointer user_data)
{
    FMListView *view;

    view = FM_LIST_VIEW (user_data);

    return fm_directory_view_get_uri (FM_DIRECTORY_VIEW (view));
}

static CajaFile *
get_file_for_path_callback (CajaTreeViewDragDest *dest,
                            GtkTreePath *path,
                            gpointer user_data)
{
    FMListView *view;

    view = FM_LIST_VIEW (user_data);

    return fm_list_model_file_for_path (view->details->model, path);
}

/* Handles an URL received from Mozilla */
static void
list_view_handle_netscape_url (CajaTreeViewDragDest *dest, const char *encoded_url,
                               const char *target_uri, GdkDragAction action, int x, int y, FMListView *view)
{
    fm_directory_view_handle_netscape_url_drop (FM_DIRECTORY_VIEW (view),
            encoded_url, target_uri, action, x, y);
}

static void
list_view_handle_uri_list (CajaTreeViewDragDest *dest, const char *item_uris,
                           const char *target_uri,
                           GdkDragAction action, int x, int y, FMListView *view)
{
    fm_directory_view_handle_uri_list_drop (FM_DIRECTORY_VIEW (view),
                                            item_uris, target_uri, action, x, y);
}

static void
list_view_handle_text (CajaTreeViewDragDest *dest, const char *text,
                       const char *target_uri,
                       GdkDragAction action, int x, int y, FMListView *view)
{
    fm_directory_view_handle_text_drop (FM_DIRECTORY_VIEW (view),
                                        text, target_uri, action, x, y);
}

static void
list_view_handle_raw (CajaTreeViewDragDest *dest, const char *raw_data,
                      int length, const char *target_uri, const char *direct_save_uri,
                      GdkDragAction action, int x, int y, FMListView *view)
{
    fm_directory_view_handle_raw_drop (FM_DIRECTORY_VIEW (view),
                                       raw_data, length, target_uri, direct_save_uri,
                                       action, x, y);
}

static void
move_copy_items_callback (CajaTreeViewDragDest *dest,
                          const GList *item_uris,
                          const char *target_uri,
                          guint action,
                          int x,
                          int y,
                          gpointer user_data)

{
    FMDirectoryView *view = user_data;

    caja_clipboard_clear_if_colliding_uris (GTK_WIDGET (view),
                                            item_uris,
                                            fm_directory_view_get_copied_files_atom (view));
    fm_directory_view_move_copy_items (item_uris,
                                       NULL,
                                       target_uri,
                                       action,
                                       x, y,
                                       view);
}

static void
apply_columns_settings (FMListView *list_view,
                        char **column_order,
                        char **visible_columns)
{
    GList *all_columns;
    CajaFile *file;
    GList *old_view_columns, *view_columns;
    GHashTable *visible_columns_hash;
    GtkTreeViewColumn *prev_view_column;
    GList *l;
    int i;

    file = fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (list_view));

    /* prepare ordered list of view columns using column_order and visible_columns */
    view_columns = NULL;

    all_columns = caja_get_columns_for_file (file);
    all_columns = caja_sort_columns (all_columns, column_order);

    /* hash table to lookup if a given column should be visible */
    visible_columns_hash = g_hash_table_new_full (g_str_hash,
                           g_str_equal,
                           (GDestroyNotify) g_free,
                           (GDestroyNotify) g_free);
    for (i = 0; visible_columns[i] != NULL; ++i)
    {
        g_hash_table_insert (visible_columns_hash,
                             g_ascii_strdown (visible_columns[i], -1),
                             g_ascii_strdown (visible_columns[i], -1));
    }

    for (l = all_columns; l != NULL; l = l->next)
    {
        char *name;
        char *lowercase;

        g_object_get (G_OBJECT (l->data), "name", &name, NULL);
        lowercase = g_ascii_strdown (name, -1);

        if (g_hash_table_lookup (visible_columns_hash, lowercase) != NULL)
        {
            GtkTreeViewColumn *view_column;

            view_column = g_hash_table_lookup (list_view->details->columns, name);
            if (view_column != NULL)
            {
                view_columns = g_list_prepend (view_columns, view_column);
            }
        }

        g_free (name);
        g_free (lowercase);
    }

    g_hash_table_destroy (visible_columns_hash);
    caja_column_list_free (all_columns);

    view_columns = g_list_reverse (view_columns);

    /* remove columns that are not present in the configuration */
    old_view_columns = gtk_tree_view_get_columns (list_view->details->tree_view);
    for (l = old_view_columns; l != NULL; l = l->next)
    {
        if (g_list_find (view_columns, l->data) == NULL)
        {
            gtk_tree_view_remove_column (list_view->details->tree_view, l->data);
        }
    }
    g_list_free (old_view_columns);

    /* append new columns from the configuration */
    old_view_columns = gtk_tree_view_get_columns (list_view->details->tree_view);
    for (l = view_columns; l != NULL; l = l->next)
    {
        if (g_list_find (old_view_columns, l->data) == NULL)
        {
            gtk_tree_view_append_column (list_view->details->tree_view, l->data);
        }
    }
    g_list_free (old_view_columns);

    /* place columns in the correct order */
    prev_view_column = NULL;
    for (l = view_columns; l != NULL; l = l->next)
    {
        gtk_tree_view_move_column_after (list_view->details->tree_view, l->data, prev_view_column);
        prev_view_column = l->data;
    }
    g_list_free (view_columns);
}

static void
filename_cell_data_func (GtkTreeViewColumn *column,
                         GtkCellRenderer   *renderer,
                         GtkTreeModel      *model,
                         GtkTreeIter       *iter,
                         FMListView        *view)
{
    char *text;
    GtkTreePath *path;
    PangoUnderline underline;

    gtk_tree_model_get (model, iter,
                        view->details->file_name_column_num, &text,
                        -1);

    if (click_policy_auto_value == CAJA_CLICK_POLICY_SINGLE)
    {
        path = gtk_tree_model_get_path (model, iter);

        if (view->details->hover_path == NULL ||
                gtk_tree_path_compare (path, view->details->hover_path))
        {
            underline = PANGO_UNDERLINE_NONE;
        }
        else
        {
            underline = PANGO_UNDERLINE_SINGLE;
        }

        gtk_tree_path_free (path);
    }
    else
    {
        underline = PANGO_UNDERLINE_NONE;
    }

    g_object_set (G_OBJECT (renderer),
                  "text", text,
                  "underline", underline,
                  NULL);
    g_free (text);
}

static gboolean
focus_in_event_callback (GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
{
    CajaWindowSlotInfo *slot_info;
    FMListView *list_view = FM_LIST_VIEW (user_data);

    /* make the corresponding slot (and the pane that contains it) active */
    slot_info = fm_directory_view_get_caja_window_slot (FM_DIRECTORY_VIEW (list_view));
    caja_window_slot_info_make_hosting_pane_active (slot_info);

    return FALSE;
}

static void
create_and_set_up_tree_view (FMListView *view)
{
    GtkCellRenderer *cell;
    GtkTreeViewColumn *column;
    GtkBindingSet *binding_set;
    AtkObject *atk_obj;
    GList *caja_columns;
    GList *l;

    view->details->tree_view = GTK_TREE_VIEW (gtk_tree_view_new ());
    view->details->columns = g_hash_table_new_full (g_str_hash,
                             g_str_equal,
                             (GDestroyNotify)g_free,
                             (GDestroyNotify) g_object_unref);
    gtk_tree_view_set_enable_search (view->details->tree_view, TRUE);

    /* Don't handle backspace key. It's used to open the parent folder. */
    binding_set = gtk_binding_set_by_class (GTK_WIDGET_GET_CLASS (view->details->tree_view));
	gtk_binding_entry_remove (binding_set, GDK_KEY_BackSpace, 0);

    view->details->drag_dest =
        caja_tree_view_drag_dest_new (view->details->tree_view);

    g_signal_connect_object (view->details->drag_dest,
                             "get_root_uri",
                             G_CALLBACK (get_root_uri_callback),
                             view, 0);
    g_signal_connect_object (view->details->drag_dest,
                             "get_file_for_path",
                             G_CALLBACK (get_file_for_path_callback),
                             view, 0);
    g_signal_connect_object (view->details->drag_dest,
                             "move_copy_items",
                             G_CALLBACK (move_copy_items_callback),
                             view, 0);
    g_signal_connect_object (view->details->drag_dest, "handle_netscape_url",
                             G_CALLBACK (list_view_handle_netscape_url), view, 0);
    g_signal_connect_object (view->details->drag_dest, "handle_uri_list",
                             G_CALLBACK (list_view_handle_uri_list), view, 0);
    g_signal_connect_object (view->details->drag_dest, "handle_text",
                             G_CALLBACK (list_view_handle_text), view, 0);
    g_signal_connect_object (view->details->drag_dest, "handle_raw",
                             G_CALLBACK (list_view_handle_raw), view, 0);

    g_signal_connect_object (gtk_tree_view_get_selection (view->details->tree_view),
                             "changed",
                             G_CALLBACK (list_selection_changed_callback), view, 0);

    g_signal_connect_object (view->details->tree_view, "drag_begin",
                             G_CALLBACK (drag_begin_callback), view, 0);
    g_signal_connect_object (view->details->tree_view, "drag_data_get",
                             G_CALLBACK (drag_data_get_callback), view, 0);
    g_signal_connect_object (view->details->tree_view, "motion_notify_event",
                             G_CALLBACK (motion_notify_callback), view, 0);
    g_signal_connect_object (view->details->tree_view, "enter_notify_event",
                             G_CALLBACK (enter_notify_callback), view, 0);
    g_signal_connect_object (view->details->tree_view, "leave_notify_event",
                             G_CALLBACK (leave_notify_callback), view, 0);
    g_signal_connect_object (view->details->tree_view, "button_press_event",
                             G_CALLBACK (button_press_callback), view, 0);
    g_signal_connect_object (view->details->tree_view, "button_release_event",
                             G_CALLBACK (button_release_callback), view, 0);
    g_signal_connect_object (view->details->tree_view, "key_press_event",
                             G_CALLBACK (key_press_callback), view, 0);
    g_signal_connect_object (view->details->tree_view, "popup_menu",
                             G_CALLBACK (popup_menu_callback), view, 0);
    g_signal_connect_object (view->details->tree_view, "row_expanded",
                             G_CALLBACK (row_expanded_callback), view, 0);
    g_signal_connect_object (view->details->tree_view, "row_collapsed",
                             G_CALLBACK (row_collapsed_callback), view, 0);
    g_signal_connect_object (view->details->tree_view, "row-activated",
                             G_CALLBACK (row_activated_callback), view, 0);

    g_signal_connect_object (view->details->tree_view, "focus_in_event",
                             G_CALLBACK(focus_in_event_callback), view, 0);

    view->details->model = g_object_new (FM_TYPE_LIST_MODEL, NULL);
    gtk_tree_view_set_model (view->details->tree_view, GTK_TREE_MODEL (view->details->model));
    /* Need the model for the dnd drop icon "accept" change */
    fm_list_model_set_drag_view (FM_LIST_MODEL (view->details->model),
                                 view->details->tree_view,  0, 0);

    g_signal_connect_object (view->details->model, "sort_column_changed",
                             G_CALLBACK (sort_column_changed_callback), view, 0);

    g_signal_connect_object (view->details->model, "subdirectory_unloaded",
                             G_CALLBACK (subdirectory_unloaded_callback), view, 0);

    gtk_tree_selection_set_mode (gtk_tree_view_get_selection (view->details->tree_view), GTK_SELECTION_MULTIPLE);
    gtk_tree_view_set_rules_hint (view->details->tree_view, TRUE);

    caja_columns = caja_get_all_columns ();

    for (l = caja_columns; l != NULL; l = l->next)
    {
        CajaColumn *caja_column;
        int column_num, font_size;
        char *name;
        char *label;
        float xalign;

        caja_column = CAJA_COLUMN (l->data);

        g_object_get (caja_column,
                      "name", &name,
                      "label", &label,
                      "xalign", &xalign, NULL);

        column_num = fm_list_model_add_column (view->details->model,
                                               caja_column);

        /* Created the name column specially, because it
         * has the icon in it.*/
        if (!strcmp (name, "name"))
        {
            /* Create the file name column */
            cell = gtk_cell_renderer_pixbuf_new ();
            view->details->pixbuf_cell = (GtkCellRendererPixbuf *)cell;

            view->details->file_name_column = gtk_tree_view_column_new ();
            gtk_tree_view_column_set_expand (view->details->file_name_column, TRUE);
            font_size = PANGO_PIXELS (pango_font_description_get_size (GTK_WIDGET(view)->style->font_desc));
            gtk_tree_view_column_set_min_width (view->details->file_name_column, 20*font_size);
            g_object_ref_sink (view->details->file_name_column);
            view->details->file_name_column_num = column_num;

            g_hash_table_insert (view->details->columns,
                                 g_strdup ("name"),
                                 view->details->file_name_column);

            gtk_tree_view_set_search_column (view->details->tree_view, column_num);

            gtk_tree_view_column_set_sort_column_id (view->details->file_name_column, column_num);
            gtk_tree_view_column_set_title (view->details->file_name_column, _("Name"));
            gtk_tree_view_column_set_resizable (view->details->file_name_column, TRUE);

            gtk_tree_view_column_pack_start (view->details->file_name_column, cell, FALSE);
            gtk_tree_view_column_set_attributes (view->details->file_name_column,
                                                 cell,
                                                 "pixbuf", FM_LIST_MODEL_SMALLEST_ICON_COLUMN,
                                                 NULL);

            cell = gtk_cell_renderer_text_new ();
            g_object_set (cell,
                        "ellipsize", PANGO_ELLIPSIZE_END,
                        "ellipsize-set", TRUE,
                        NULL);
            view->details->file_name_cell = (GtkCellRendererText *)cell;
            g_signal_connect (cell, "edited", G_CALLBACK (cell_renderer_edited), view);
            g_signal_connect (cell, "editing-canceled", G_CALLBACK (cell_renderer_editing_canceled), view);
            g_signal_connect (cell, "editing-started", G_CALLBACK (cell_renderer_editing_started_cb), view);

            gtk_tree_view_column_pack_start (view->details->file_name_column, cell, TRUE);
            gtk_tree_view_column_set_cell_data_func (view->details->file_name_column, cell,
                    (GtkTreeCellDataFunc) filename_cell_data_func,
                    view, NULL);
        }
        else
        {
            cell = gtk_cell_renderer_text_new ();
            g_object_set (cell, "xalign", xalign, NULL);
            view->details->cells = g_list_append (view->details->cells,
                                                  cell);
            column = gtk_tree_view_column_new_with_attributes (label,
                     cell,
                     "text", column_num,
                     NULL);
            g_object_ref_sink (column);
            gtk_tree_view_column_set_sort_column_id (column, column_num);
            g_hash_table_insert (view->details->columns,
                                 g_strdup (name),
                                 column);

            gtk_tree_view_column_set_resizable (column, TRUE);
            gtk_tree_view_column_set_visible (column, TRUE);
        }
        g_free (name);
        g_free (label);
    }
    caja_column_list_free (caja_columns);

    /* Apply the default column order and visible columns, to get it
     * right most of the time. The metadata will be checked when a
     * folder is loaded */
    apply_columns_settings (view,
                            default_column_order_auto_value,
                            default_visible_columns_auto_value);

    gtk_widget_show (GTK_WIDGET (view->details->tree_view));
    gtk_container_add (GTK_CONTAINER (view), GTK_WIDGET (view->details->tree_view));


    atk_obj = gtk_widget_get_accessible (GTK_WIDGET (view->details->tree_view));
    atk_object_set_name (atk_obj, _("List View"));
}

static void
fm_list_view_add_file (FMDirectoryView *view, CajaFile *file, CajaDirectory *directory)
{
    FMListModel *model;

    model = FM_LIST_VIEW (view)->details->model;
    fm_list_model_add_file (model, file, directory);
}

static char **
get_visible_columns (FMListView *list_view)
{
    CajaFile *file;
    GList *visible_columns;
    char **ret;

    ret = NULL;

    file = fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (list_view));

    visible_columns = caja_file_get_metadata_list
                      (file,
                       CAJA_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS);

    if (visible_columns)
    {
        GPtrArray *res;
        GList *l;

        res = g_ptr_array_new ();
        for (l = visible_columns; l != NULL; l = l->next)
        {
            g_ptr_array_add (res, l->data);
        }
        g_ptr_array_add (res, NULL);

        ret = (char **) g_ptr_array_free (res, FALSE);
        g_list_free (visible_columns);
    }

    if (ret != NULL)
    {
        return ret;
    }

    return caja_file_is_in_trash (file) ?
           g_strdupv ((gchar **) default_trash_visible_columns) :
           g_strdupv (default_visible_columns_auto_value);
}

static char **
get_column_order (FMListView *list_view)
{
    CajaFile *file;
    GList *column_order;
    char **ret;

    ret = NULL;

    file = fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (list_view));

    column_order = caja_file_get_metadata_list
                   (file,
                    CAJA_METADATA_KEY_LIST_VIEW_COLUMN_ORDER);

    if (column_order)
    {
        GPtrArray *res;
        GList *l;

        res = g_ptr_array_new ();
        for (l = column_order; l != NULL; l = l->next)
        {
            g_ptr_array_add (res, l->data);
        }
        g_ptr_array_add (res, NULL);

        ret = (char **) g_ptr_array_free (res, FALSE);
        g_list_free (column_order);
    }

    if (ret != NULL)
    {
        return ret;
    }

    return caja_file_is_in_trash (file) ?
           g_strdupv ((gchar **) default_trash_columns_order) :
           g_strdupv (default_column_order_auto_value);
}

static void
set_columns_settings_from_metadata_and_preferences (FMListView *list_view)
{
    char **column_order;
    char **visible_columns;

    column_order = get_column_order (list_view);
    visible_columns = get_visible_columns (list_view);

    apply_columns_settings (list_view, column_order, visible_columns);

    g_strfreev (column_order);
    g_strfreev (visible_columns);
}

static void
set_sort_order_from_metadata_and_preferences (FMListView *list_view)
{
    char *sort_attribute;
    int sort_column_id;
    CajaFile *file;
    gboolean sort_reversed, default_sort_reversed;
    const gchar *default_sort_order;

    file = fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (list_view));
    sort_attribute = caja_file_get_metadata (file,
                     CAJA_METADATA_KEY_LIST_VIEW_SORT_COLUMN,
                     NULL);
    sort_column_id = fm_list_model_get_sort_column_id_from_attribute (list_view->details->model,
                     g_quark_from_string (sort_attribute));
    g_free (sort_attribute);

    default_sort_order = get_default_sort_order (file, &default_sort_reversed);

    if (sort_column_id == -1)
    {
        sort_column_id =
            fm_list_model_get_sort_column_id_from_attribute (list_view->details->model,
                    g_quark_from_string (default_sort_order));
    }

    sort_reversed = caja_file_get_boolean_metadata (file,
                    CAJA_METADATA_KEY_LIST_VIEW_SORT_REVERSED,
                    default_sort_reversed);

    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (list_view->details->model),
                                          sort_column_id,
                                          sort_reversed ? GTK_SORT_DESCENDING : GTK_SORT_ASCENDING);
}

static gboolean
list_view_changed_foreach (GtkTreeModel *model,
                           GtkTreePath  *path,
                           GtkTreeIter  *iter,
                           gpointer      data)
{
    gtk_tree_model_row_changed (model, path, iter);
    return FALSE;
}

static CajaZoomLevel
get_default_zoom_level (void)
{
    CajaZoomLevel default_zoom_level;

    default_zoom_level = default_zoom_level_auto_value;

    if (default_zoom_level <  CAJA_ZOOM_LEVEL_SMALLEST
            || CAJA_ZOOM_LEVEL_LARGEST < default_zoom_level)
    {
        default_zoom_level = CAJA_ZOOM_LEVEL_SMALL;
    }

    return default_zoom_level;
}

static void
set_zoom_level_from_metadata_and_preferences (FMListView *list_view)
{
    CajaFile *file;
    int level;

    if (fm_directory_view_supports_zooming (FM_DIRECTORY_VIEW (list_view)))
    {
        file = fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (list_view));
        level = caja_file_get_integer_metadata (file,
                                                CAJA_METADATA_KEY_LIST_VIEW_ZOOM_LEVEL,
                                                get_default_zoom_level ());
        fm_list_view_set_zoom_level (list_view, level, TRUE);

        /* updated the rows after updating the font size */
        gtk_tree_model_foreach (GTK_TREE_MODEL (list_view->details->model),
                                list_view_changed_foreach, NULL);
    }
}

static void
fm_list_view_begin_loading (FMDirectoryView *view)
{
    FMListView *list_view;

    list_view = FM_LIST_VIEW (view);

    set_sort_order_from_metadata_and_preferences (list_view);
    set_zoom_level_from_metadata_and_preferences (list_view);
    set_columns_settings_from_metadata_and_preferences (list_view);
}

static void
stop_cell_editing (FMListView *list_view)
{
    GtkTreeViewColumn *column;

    /* Stop an ongoing rename to commit the name changes when the user
     * changes directories without exiting cell edit mode. It also prevents
     * the edited handler from being called on the cleared list model.
     */
    column = list_view->details->file_name_column;
    if (column != NULL && list_view->details->editable_widget != NULL &&
            GTK_IS_CELL_EDITABLE (list_view->details->editable_widget))
    {
        gtk_cell_editable_editing_done (list_view->details->editable_widget);
    }
}

static void
fm_list_view_clear (FMDirectoryView *view)
{
    FMListView *list_view;

    list_view = FM_LIST_VIEW (view);

    if (list_view->details->model != NULL)
    {
        stop_cell_editing (list_view);
        fm_list_model_clear (list_view->details->model);
    }
}

static void
fm_list_view_rename_callback (CajaFile *file,
                              GFile *result_location,
                              GError *error,
                              gpointer callback_data)
{
    FMListView *view;

    view = FM_LIST_VIEW (callback_data);

    if (view->details->renaming_file)
    {
        view->details->rename_done = TRUE;

        if (error != NULL)
        {
            /* If the rename failed (or was cancelled), kill renaming_file.
             * We won't get a change event for the rename, so otherwise
             * it would stay around forever.
             */
            caja_file_unref (view->details->renaming_file);
            view->details->renaming_file = NULL;
        }
    }

    g_object_unref (view);
}


static void
fm_list_view_file_changed (FMDirectoryView *view, CajaFile *file, CajaDirectory *directory)
{
    FMListView *listview;
    GtkTreeIter iter;
    GtkTreePath *file_path;

    listview = FM_LIST_VIEW (view);

    fm_list_model_file_changed (listview->details->model, file, directory);

    if (listview->details->renaming_file != NULL &&
            file == listview->details->renaming_file &&
            listview->details->rename_done)
    {
        /* This is (probably) the result of the rename operation, and
         * the tree-view changes above could have resorted the list, so
         * scroll to the new position
         */
        if (fm_list_model_get_tree_iter_from_file (listview->details->model, file, directory, &iter))
        {
            file_path = gtk_tree_model_get_path (GTK_TREE_MODEL (listview->details->model), &iter);
            gtk_tree_view_scroll_to_cell (listview->details->tree_view,
                                          file_path, NULL,
                                          FALSE, 0.0, 0.0);
            gtk_tree_path_free (file_path);
        }

        caja_file_unref (listview->details->renaming_file);
        listview->details->renaming_file = NULL;
    }
}

static GtkWidget *
fm_list_view_get_background_widget (FMDirectoryView *view)
{
    return GTK_WIDGET (view);
}

static void
fm_list_view_get_selection_foreach_func (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
    GList **list;
    CajaFile *file;

    list = data;

    gtk_tree_model_get (model, iter,
                        FM_LIST_MODEL_FILE_COLUMN, &file,
                        -1);

    if (file != NULL)
    {
        (* list) = g_list_prepend ((* list), file);
    }
}

static GList *
fm_list_view_get_selection (FMDirectoryView *view)
{
    GList *list;

    list = NULL;

    gtk_tree_selection_selected_foreach (gtk_tree_view_get_selection (FM_LIST_VIEW (view)->details->tree_view),
                                         fm_list_view_get_selection_foreach_func, &list);

    return g_list_reverse (list);
}

static void
fm_list_view_get_selection_for_file_transfer_foreach_func (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
    CajaFile *file;
    struct SelectionForeachData *selection_data;
    GtkTreeIter parent, child;

    selection_data = data;

    gtk_tree_model_get (model, iter,
                        FM_LIST_MODEL_FILE_COLUMN, &file,
                        -1);

    if (file != NULL)
    {
        /* If the parent folder is also selected, don't include this file in the
         * file operation, since that would copy it to the toplevel target instead
         * of keeping it as a child of the copied folder
         */
        child = *iter;
        while (gtk_tree_model_iter_parent (model, &parent, &child))
        {
            if (gtk_tree_selection_iter_is_selected (selection_data->selection,
                    &parent))
            {
                return;
            }
            child = parent;
        }

        caja_file_ref (file);
        selection_data->list = g_list_prepend (selection_data->list, file);
    }
}


static GList *
fm_list_view_get_selection_for_file_transfer (FMDirectoryView *view)
{
    struct SelectionForeachData selection_data;

    selection_data.list = NULL;
    selection_data.selection = gtk_tree_view_get_selection (FM_LIST_VIEW (view)->details->tree_view);

    gtk_tree_selection_selected_foreach (selection_data.selection,
                                         fm_list_view_get_selection_for_file_transfer_foreach_func, &selection_data);

    return g_list_reverse (selection_data.list);
}




static guint
fm_list_view_get_item_count (FMDirectoryView *view)
{
    g_return_val_if_fail (FM_IS_LIST_VIEW (view), 0);

    return fm_list_model_get_length (FM_LIST_VIEW (view)->details->model);
}

static gboolean
fm_list_view_is_empty (FMDirectoryView *view)
{
    return fm_list_model_is_empty (FM_LIST_VIEW (view)->details->model);
}

static void
fm_list_view_end_file_changes (FMDirectoryView *view)
{
    FMListView *list_view;

    list_view = FM_LIST_VIEW (view);

    if (list_view->details->new_selection_path)
    {
        gtk_tree_view_set_cursor (list_view->details->tree_view,
                                  list_view->details->new_selection_path,
                                  NULL, FALSE);
        gtk_tree_path_free (list_view->details->new_selection_path);
        list_view->details->new_selection_path = NULL;
    }
}

static void
fm_list_view_remove_file (FMDirectoryView *view, CajaFile *file, CajaDirectory *directory)
{
    GtkTreePath *path;
    GtkTreePath *file_path;
    GtkTreeIter iter;
    GtkTreeIter temp_iter;
    GtkTreeRowReference* row_reference;
    FMListView *list_view;
    GtkTreeModel* tree_model;
    GtkTreeSelection *selection;

    path = NULL;
    row_reference = NULL;
    list_view = FM_LIST_VIEW (view);
    tree_model = GTK_TREE_MODEL(list_view->details->model);

    if (fm_list_model_get_tree_iter_from_file (list_view->details->model, file, directory, &iter))
    {
        selection = gtk_tree_view_get_selection (list_view->details->tree_view);
        file_path = gtk_tree_model_get_path (tree_model, &iter);

        if (gtk_tree_selection_path_is_selected (selection, file_path))
        {
            /* get reference for next element in the list view. If the element to be deleted is the
             * last one, get reference to previous element. If there is only one element in view
             * no need to select anything.
            */
            temp_iter = iter;

            if (gtk_tree_model_iter_next (tree_model, &iter))
            {
                path = gtk_tree_model_get_path (tree_model, &iter);
                row_reference = gtk_tree_row_reference_new (tree_model, path);
            }
            else
            {
                path = gtk_tree_model_get_path (tree_model, &temp_iter);
                if (gtk_tree_path_prev (path))
                {
                    row_reference = gtk_tree_row_reference_new (tree_model, path);
                }
            }
            gtk_tree_path_free (path);
        }

        gtk_tree_path_free (file_path);

        fm_list_model_remove_file (list_view->details->model, file, directory);

        if (gtk_tree_row_reference_valid (row_reference))
        {
            if (list_view->details->new_selection_path)
            {
                gtk_tree_path_free (list_view->details->new_selection_path);
            }
            list_view->details->new_selection_path = gtk_tree_row_reference_get_path (row_reference);
        }

        if (row_reference)
        {
            gtk_tree_row_reference_free (row_reference);
        }
    }


}

static void
fm_list_view_set_selection (FMDirectoryView *view, GList *selection)
{
    FMListView *list_view;
    GtkTreeSelection *tree_selection;
    GList *node;
    GList *iters, *l;
    CajaFile *file;

    list_view = FM_LIST_VIEW (view);
    tree_selection = gtk_tree_view_get_selection (list_view->details->tree_view);

    g_signal_handlers_block_by_func (tree_selection, list_selection_changed_callback, view);

    gtk_tree_selection_unselect_all (tree_selection);
    for (node = selection; node != NULL; node = node->next)
    {
        file = node->data;
        iters = fm_list_model_get_all_iters_for_file (list_view->details->model, file);

        for (l = iters; l != NULL; l = l->next)
        {
            gtk_tree_selection_select_iter (tree_selection,
                                            (GtkTreeIter *)l->data);
        }
    	g_list_foreach(iters, (GFunc) g_free, NULL);
    	g_list_free(iters);
    }

    g_signal_handlers_unblock_by_func (tree_selection, list_selection_changed_callback, view);
    fm_directory_view_notify_selection_changed (view);
}

static void
fm_list_view_invert_selection (FMDirectoryView *view)
{
    FMListView *list_view;
    GtkTreeSelection *tree_selection;
    GList *node;
    GList *iters, *l;
    CajaFile *file;
    GList *selection = NULL;

    list_view = FM_LIST_VIEW (view);
    tree_selection = gtk_tree_view_get_selection (list_view->details->tree_view);

    g_signal_handlers_block_by_func (tree_selection, list_selection_changed_callback, view);

    gtk_tree_selection_selected_foreach (tree_selection,
                                         fm_list_view_get_selection_foreach_func, &selection);

    gtk_tree_selection_select_all (tree_selection);

    for (node = selection; node != NULL; node = node->next)
    {
        file = node->data;
        iters = fm_list_model_get_all_iters_for_file (list_view->details->model, file);

        for (l = iters; l != NULL; l = l->next)
        {
            gtk_tree_selection_unselect_iter (tree_selection,
                                              (GtkTreeIter *)l->data);
        }
    	g_list_foreach(iters, (GFunc) g_free, NULL);
    	g_list_free(iters);
    }

    g_list_free (selection);

    g_signal_handlers_unblock_by_func (tree_selection, list_selection_changed_callback, view);
    fm_directory_view_notify_selection_changed (view);
}

static void
fm_list_view_select_all (FMDirectoryView *view)
{
    gtk_tree_selection_select_all (gtk_tree_view_get_selection (FM_LIST_VIEW (view)->details->tree_view));
}

static void
column_editor_response_callback (GtkWidget *dialog,
                                 int response_id,
                                 gpointer user_data)
{
    gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
column_chooser_changed_callback (CajaColumnChooser *chooser,
                                 FMListView *view)
{
    CajaFile *file;
    char **visible_columns;
    char **column_order;
    GList *list;
    int i;

    file = fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (view));

    caja_column_chooser_get_settings (chooser,
                                      &visible_columns,
                                      &column_order);

    list = NULL;
    for (i = 0; visible_columns[i] != NULL; ++i)
    {
        list = g_list_prepend (list, visible_columns[i]);
    }
    list = g_list_reverse (list);
    caja_file_set_metadata_list (file,
                                 CAJA_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS,
                                 list);
    g_list_free (list);

    list = NULL;
    for (i = 0; column_order[i] != NULL; ++i)
    {
        list = g_list_prepend (list, column_order[i]);
    }
    list = g_list_reverse (list);
    caja_file_set_metadata_list (file,
                                 CAJA_METADATA_KEY_LIST_VIEW_COLUMN_ORDER,
                                 list);
    g_list_free (list);

    apply_columns_settings (view, column_order, visible_columns);

    g_strfreev (visible_columns);
    g_strfreev (column_order);
}

static void
column_chooser_set_from_arrays (CajaColumnChooser *chooser,
                                FMListView *view,
                                char **visible_columns,
                                char **column_order)
{
    g_signal_handlers_block_by_func
    (chooser, G_CALLBACK (column_chooser_changed_callback), view);

    caja_column_chooser_set_settings (chooser,
                                      visible_columns,
                                      column_order);

    g_signal_handlers_unblock_by_func
    (chooser, G_CALLBACK (column_chooser_changed_callback), view);
}

static void
column_chooser_set_from_settings (CajaColumnChooser *chooser,
                                  FMListView *view)
{
    char **visible_columns;
    char **column_order;

    visible_columns = get_visible_columns (view);
    column_order = get_column_order (view);

    column_chooser_set_from_arrays (chooser, view,
                                    visible_columns, column_order);

    g_strfreev (visible_columns);
    g_strfreev (column_order);
}

static void
column_chooser_use_default_callback (CajaColumnChooser *chooser,
                                     FMListView *view)
{
    CajaFile *file;
    char **default_columns;
    char **default_order;

    file = fm_directory_view_get_directory_as_file
           (FM_DIRECTORY_VIEW (view));

    caja_file_set_metadata_list (file, CAJA_METADATA_KEY_LIST_VIEW_COLUMN_ORDER, NULL);
    caja_file_set_metadata_list (file, CAJA_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS, NULL);

    /* set view values ourselves, as new metadata could not have been
     * updated yet.
     */
    default_columns = caja_file_is_in_trash (file) ?
                      g_strdupv ((gchar **) default_trash_visible_columns) :
                      g_strdupv (default_visible_columns_auto_value);

    default_order = caja_file_is_in_trash (file) ?
                    g_strdupv ((gchar **) default_trash_columns_order) :
                    g_strdupv (default_column_order_auto_value);

    apply_columns_settings (view, default_order, default_columns);
    column_chooser_set_from_arrays (chooser, view,
                                    default_columns, default_order);

    g_strfreev (default_columns);
    g_strfreev (default_order);
}

static GtkWidget *
create_column_editor (FMListView *view)
{
    GtkWidget *window;
    GtkWidget *label;
    GtkWidget *box;
    GtkWidget *column_chooser;
    GtkWidget *alignment;
    CajaFile *file;
    char *str;
    char *name;
    const char *label_text;

    file = fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (view));
    name = caja_file_get_display_name (file);
    str = g_strdup_printf (_("%s Visible Columns"), name);
    g_free (name);

    window = gtk_dialog_new_with_buttons (str,
                                          GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view))),
                                          GTK_DIALOG_DESTROY_WITH_PARENT,
                                          GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                          NULL);
    g_free (str);
    g_signal_connect (window, "response",
                      G_CALLBACK (column_editor_response_callback), NULL);

    gtk_window_set_default_size (GTK_WINDOW (window), 300, 400);

    box = gtk_vbox_new (FALSE, 12);
    gtk_container_set_border_width (GTK_CONTAINER (box), 12);
    gtk_widget_show (box);
    gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (window))), box);

    label_text = _("Choose the order of information to appear in this folder:");
    str = g_strconcat ("<b>", label_text, "</b>", NULL);
    label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (label), str);
    gtk_label_set_line_wrap (GTK_LABEL (label), FALSE);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
    gtk_widget_show (label);
    gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

    g_free (str);

    alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
    gtk_alignment_set_padding (GTK_ALIGNMENT (alignment),
                               0, 0, 12, 0);
    gtk_widget_show (alignment);
    gtk_box_pack_start (GTK_BOX (box), alignment, TRUE, TRUE, 0);

    column_chooser = caja_column_chooser_new (file);
    gtk_widget_show (column_chooser);
    gtk_container_add (GTK_CONTAINER (alignment), column_chooser);

    g_signal_connect (column_chooser, "changed",
                      G_CALLBACK (column_chooser_changed_callback),
                      view);
    g_signal_connect (column_chooser, "use_default",
                      G_CALLBACK (column_chooser_use_default_callback),
                      view);

    column_chooser_set_from_settings
    (CAJA_COLUMN_CHOOSER (column_chooser), view);

    return window;
}

static void
action_visible_columns_callback (GtkAction *action,
                                 gpointer callback_data)
{
    FMListView *list_view;

    list_view = FM_LIST_VIEW (callback_data);

    if (list_view->details->column_editor)
    {
        gtk_window_present (GTK_WINDOW (list_view->details->column_editor));
    }
    else
    {
        list_view->details->column_editor = create_column_editor (list_view);
        eel_add_weak_pointer (&list_view->details->column_editor);

        gtk_widget_show (list_view->details->column_editor);
    }
}

static const GtkActionEntry list_view_entries[] =
{
    /* name, stock id */     { "Visible Columns", NULL,
        /* label, accelerator */   N_("Visible _Columns..."), NULL,
        /* tooltip */              N_("Select the columns visible in this folder"),
        G_CALLBACK (action_visible_columns_callback)
    },
};

static void
fm_list_view_merge_menus (FMDirectoryView *view)
{
    FMListView *list_view;
    GtkUIManager *ui_manager;
    GtkActionGroup *action_group;
    const char *ui;

    EEL_CALL_PARENT (FM_DIRECTORY_VIEW_CLASS, merge_menus, (view));

    list_view = FM_LIST_VIEW (view);

    ui_manager = fm_directory_view_get_ui_manager (view);

    action_group = gtk_action_group_new ("ListViewActions");
    gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
    list_view->details->list_action_group = action_group;
    gtk_action_group_add_actions (action_group,
                                  list_view_entries, G_N_ELEMENTS (list_view_entries),
                                  list_view);

    gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
    g_object_unref (action_group); /* owned by ui manager */

    ui = caja_ui_string_get ("caja-list-view-ui.xml");
    list_view->details->list_merge_id = gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, NULL);

    list_view->details->menus_ready = TRUE;
}

static void
fm_list_view_unmerge_menus (FMDirectoryView *view)
{
    FMListView *list_view;
    GtkUIManager *ui_manager;

    list_view = FM_LIST_VIEW (view);

    FM_DIRECTORY_VIEW_CLASS (fm_list_view_parent_class)->unmerge_menus (view);

    ui_manager = fm_directory_view_get_ui_manager (view);
    if (ui_manager != NULL)
    {
        caja_ui_unmerge_ui (ui_manager,
                            &list_view->details->list_merge_id,
                            &list_view->details->list_action_group);
    }
}

static void
fm_list_view_update_menus (FMDirectoryView *view)
{
    FMListView *list_view;

    list_view = FM_LIST_VIEW (view);

    /* don't update if the menus aren't ready */
    if (!list_view->details->menus_ready)
    {
        return;
    }

    EEL_CALL_PARENT (FM_DIRECTORY_VIEW_CLASS, update_menus, (view));
}

/* Reset sort criteria and zoom level to match defaults */
static void
fm_list_view_reset_to_defaults (FMDirectoryView *view)
{
    CajaFile *file;
    const gchar *default_sort_order;
    gboolean default_sort_reversed;

    file = fm_directory_view_get_directory_as_file (view);

    caja_file_set_metadata (file, CAJA_METADATA_KEY_LIST_VIEW_SORT_COLUMN, NULL, NULL);
    caja_file_set_metadata (file, CAJA_METADATA_KEY_LIST_VIEW_SORT_REVERSED, NULL, NULL);
    caja_file_set_metadata (file, CAJA_METADATA_KEY_LIST_VIEW_ZOOM_LEVEL, NULL, NULL);
    caja_file_set_metadata_list (file, CAJA_METADATA_KEY_LIST_VIEW_COLUMN_ORDER, NULL);
    caja_file_set_metadata_list (file, CAJA_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS, NULL);

    default_sort_order = get_default_sort_order (file, &default_sort_reversed);

    gtk_tree_sortable_set_sort_column_id
    (GTK_TREE_SORTABLE (FM_LIST_VIEW (view)->details->model),
     fm_list_model_get_sort_column_id_from_attribute (FM_LIST_VIEW (view)->details->model,
             g_quark_from_string (default_sort_order)),
     default_sort_reversed ? GTK_SORT_DESCENDING : GTK_SORT_ASCENDING);

    fm_list_view_set_zoom_level (FM_LIST_VIEW (view), get_default_zoom_level (), FALSE);
    set_columns_settings_from_metadata_and_preferences (FM_LIST_VIEW (view));
}

static void
fm_list_view_scale_font_size (FMListView *view,
                              CajaZoomLevel new_level)
{
    GList *l;
    static gboolean first_time = TRUE;
    static double pango_scale[7];
    int medium;
    int i;

    g_return_if_fail (new_level >= CAJA_ZOOM_LEVEL_SMALLEST &&
                      new_level <= CAJA_ZOOM_LEVEL_LARGEST);

    if (first_time)
    {
        first_time = FALSE;
        medium = CAJA_ZOOM_LEVEL_SMALLER;
        pango_scale[medium] = PANGO_SCALE_MEDIUM;
        for (i = medium; i > CAJA_ZOOM_LEVEL_SMALLEST; i--)
        {
            pango_scale[i - 1] = (1 / 1.2) * pango_scale[i];
        }
        for (i = medium; i < CAJA_ZOOM_LEVEL_LARGEST; i++)
        {
            pango_scale[i + 1] = 1.2 * pango_scale[i];
        }
    }

    g_object_set (G_OBJECT (view->details->file_name_cell),
                  "scale", pango_scale[new_level],
                  NULL);
    for (l = view->details->cells; l != NULL; l = l->next)
    {
        g_object_set (G_OBJECT (l->data),
                      "scale", pango_scale[new_level],
                      NULL);
    }
}

static void
fm_list_view_set_zoom_level (FMListView *view,
                             CajaZoomLevel new_level,
                             gboolean always_emit)
{
    int icon_size;
    int column;

    g_return_if_fail (FM_IS_LIST_VIEW (view));
    g_return_if_fail (new_level >= CAJA_ZOOM_LEVEL_SMALLEST &&
                      new_level <= CAJA_ZOOM_LEVEL_LARGEST);

    if (view->details->zoom_level == new_level)
    {
        if (always_emit)
        {
            g_signal_emit_by_name (FM_DIRECTORY_VIEW(view), "zoom_level_changed");
        }
        return;
    }

    view->details->zoom_level = new_level;
    g_signal_emit_by_name (FM_DIRECTORY_VIEW(view), "zoom_level_changed");

    caja_file_set_integer_metadata
    (fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (view)),
     CAJA_METADATA_KEY_LIST_VIEW_ZOOM_LEVEL,
     get_default_zoom_level (),
     new_level);

    /* Select correctly scaled icons. */
    column = fm_list_model_get_column_id_from_zoom_level (new_level);
    gtk_tree_view_column_set_attributes (view->details->file_name_column,
                                         GTK_CELL_RENDERER (view->details->pixbuf_cell),
                                         "pixbuf", column,
                                         NULL);

    /* Scale text. */
    fm_list_view_scale_font_size (view, new_level);

    /* Make all rows the same size. */
    icon_size = caja_get_icon_size_for_zoom_level (new_level);
    gtk_cell_renderer_set_fixed_size (GTK_CELL_RENDERER (view->details->pixbuf_cell),
                                      -1, icon_size);

    fm_directory_view_update_menus (FM_DIRECTORY_VIEW (view));
}

static void
fm_list_view_bump_zoom_level (FMDirectoryView *view, int zoom_increment)
{
    FMListView *list_view;
    gint new_level;

    g_return_if_fail (FM_IS_LIST_VIEW (view));

    list_view = FM_LIST_VIEW (view);
    new_level = list_view->details->zoom_level + zoom_increment;

    if (new_level >= CAJA_ZOOM_LEVEL_SMALLEST &&
            new_level <= CAJA_ZOOM_LEVEL_LARGEST)
    {
        fm_list_view_set_zoom_level (list_view, new_level, FALSE);
    }
}

static CajaZoomLevel
fm_list_view_get_zoom_level (FMDirectoryView *view)
{
    FMListView *list_view;

    g_return_val_if_fail (FM_IS_LIST_VIEW (view), CAJA_ZOOM_LEVEL_STANDARD);

    list_view = FM_LIST_VIEW (view);

    return list_view->details->zoom_level;
}

static void
fm_list_view_zoom_to_level (FMDirectoryView *view,
                            CajaZoomLevel zoom_level)
{
    FMListView *list_view;

    g_return_if_fail (FM_IS_LIST_VIEW (view));

    list_view = FM_LIST_VIEW (view);

    fm_list_view_set_zoom_level (list_view, zoom_level, FALSE);
}

static void
fm_list_view_restore_default_zoom_level (FMDirectoryView *view)
{
    FMListView *list_view;

    g_return_if_fail (FM_IS_LIST_VIEW (view));

    list_view = FM_LIST_VIEW (view);

    fm_list_view_set_zoom_level (list_view, get_default_zoom_level (), FALSE);
}

static gboolean
fm_list_view_can_zoom_in (FMDirectoryView *view)
{
    g_return_val_if_fail (FM_IS_LIST_VIEW (view), FALSE);

    return FM_LIST_VIEW (view)->details->zoom_level	< CAJA_ZOOM_LEVEL_LARGEST;
}

static gboolean
fm_list_view_can_zoom_out (FMDirectoryView *view)
{
    g_return_val_if_fail (FM_IS_LIST_VIEW (view), FALSE);

    return FM_LIST_VIEW (view)->details->zoom_level > CAJA_ZOOM_LEVEL_SMALLEST;
}

static void
fm_list_view_start_renaming_file (FMDirectoryView *view,
                                  CajaFile *file,
                                  gboolean select_all)
{
    FMListView *list_view;
    GtkTreeIter iter;
    GtkTreePath *path;
    gint start_offset, end_offset;

    list_view = FM_LIST_VIEW (view);

    /* Select all if we are in renaming mode already */
    if (list_view->details->file_name_column && list_view->details->editable_widget)
    {
        gtk_editable_select_region (
            GTK_EDITABLE (list_view->details->editable_widget),
            0,
            -1);
        return;
    }

    if (!fm_list_model_get_first_iter_for_file (list_view->details->model, file, &iter))
    {
        return;
    }

    /* Freeze updates to the view to prevent losing rename focus when the tree view updates */
    fm_directory_view_freeze_updates (FM_DIRECTORY_VIEW (view));

    path = gtk_tree_model_get_path (GTK_TREE_MODEL (list_view->details->model), &iter);

    /* Make filename-cells editable. */
    g_object_set (G_OBJECT (list_view->details->file_name_cell),
                  "editable", TRUE,
                  NULL);

    gtk_tree_view_scroll_to_cell (list_view->details->tree_view,
                                  NULL,
                                  list_view->details->file_name_column,
                                  TRUE, 0.0, 0.0);
    gtk_tree_view_set_cursor (list_view->details->tree_view,
                              path,
                              list_view->details->file_name_column,
                              TRUE);

    /* set cursor also triggers editing-started, where we save the editable widget */
    if (list_view->details->editable_widget != NULL) {
            eel_filename_get_rename_region (list_view->details->original_name,
                                            &start_offset, &end_offset);

            gtk_editable_select_region (GTK_EDITABLE (list_view->details->editable_widget),
                                        start_offset, end_offset);
    }

    gtk_tree_path_free (path);
}

static void
fm_list_view_click_policy_changed (FMDirectoryView *directory_view)
{
    GdkWindow *win;
    GdkDisplay *display;
    FMListView *view;
    GtkTreeIter iter;
    GtkTreeView *tree;

    view = FM_LIST_VIEW (directory_view);

    /* ensure that we unset the hand cursor and refresh underlined rows */
    if (click_policy_auto_value == CAJA_CLICK_POLICY_DOUBLE)
    {
        if (view->details->hover_path != NULL)
        {
            if (gtk_tree_model_get_iter (GTK_TREE_MODEL (view->details->model),
                                         &iter, view->details->hover_path))
            {
                gtk_tree_model_row_changed (GTK_TREE_MODEL (view->details->model),
                                            view->details->hover_path, &iter);
            }

            gtk_tree_path_free (view->details->hover_path);
            view->details->hover_path = NULL;
        }

        tree = view->details->tree_view;
        if (gtk_widget_get_realized (GTK_WIDGET (tree)))
        {
            win = gtk_widget_get_window (GTK_WIDGET (tree));
            gdk_window_set_cursor (win, NULL);

            display = gtk_widget_get_display (GTK_WIDGET (view));
            if (display != NULL)
            {
                gdk_display_flush (display);
            }
        }

        if (hand_cursor != NULL)
        {
            gdk_cursor_unref (hand_cursor);
            hand_cursor = NULL;
        }
    }
    else if (click_policy_auto_value == CAJA_CLICK_POLICY_SINGLE)
    {
        if (hand_cursor == NULL)
        {
            hand_cursor = gdk_cursor_new(GDK_HAND2);
        }
    }
}

static void
default_sort_order_changed_callback (gpointer callback_data)
{
    FMListView *list_view;

    list_view = FM_LIST_VIEW (callback_data);

    set_sort_order_from_metadata_and_preferences (list_view);
}

static void
default_zoom_level_changed_callback (gpointer callback_data)
{
    FMListView *list_view;

    list_view = FM_LIST_VIEW (callback_data);

    set_zoom_level_from_metadata_and_preferences (list_view);
}

static void
default_visible_columns_changed_callback (gpointer callback_data)
{
    FMListView *list_view;

    list_view = FM_LIST_VIEW (callback_data);

    set_columns_settings_from_metadata_and_preferences (list_view);
}

static void
default_column_order_changed_callback (gpointer callback_data)
{
    FMListView *list_view;

    list_view = FM_LIST_VIEW (callback_data);

    set_columns_settings_from_metadata_and_preferences (list_view);
}

static void
fm_list_view_sort_directories_first_changed (FMDirectoryView *view)
{
    FMListView *list_view;

    list_view = FM_LIST_VIEW (view);

    fm_list_model_set_should_sort_directories_first (list_view->details->model,
            fm_directory_view_should_sort_directories_first (view));
}

static int
fm_list_view_compare_files (FMDirectoryView *view, CajaFile *file1, CajaFile *file2)
{
    FMListView *list_view;

    list_view = FM_LIST_VIEW (view);
    return fm_list_model_compare_func (list_view->details->model, file1, file2);
}

static gboolean
fm_list_view_using_manual_layout (FMDirectoryView *view)
{
    g_return_val_if_fail (FM_IS_LIST_VIEW (view), FALSE);

    return FALSE;
}

static void
fm_list_view_dispose (GObject *object)
{
    FMListView *list_view;

    list_view = FM_LIST_VIEW (object);

    if (list_view->details->model)
    {
        stop_cell_editing (list_view);
        g_object_unref (list_view->details->model);
        list_view->details->model = NULL;
    }

    if (list_view->details->drag_dest)
    {
        g_object_unref (list_view->details->drag_dest);
        list_view->details->drag_dest = NULL;
    }

    if (list_view->details->renaming_file_activate_timeout != 0)
    {
        g_source_remove (list_view->details->renaming_file_activate_timeout);
        list_view->details->renaming_file_activate_timeout = 0;
    }

    if (list_view->details->clipboard_handler_id != 0)
    {
        g_signal_handler_disconnect (caja_clipboard_monitor_get (),
                                     list_view->details->clipboard_handler_id);
        list_view->details->clipboard_handler_id = 0;
    }

    G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
fm_list_view_finalize (GObject *object)
{
    FMListView *list_view;

    list_view = FM_LIST_VIEW (object);

    g_free (list_view->details->original_name);
    list_view->details->original_name = NULL;

    if (list_view->details->double_click_path[0])
    {
        gtk_tree_path_free (list_view->details->double_click_path[0]);
    }
    if (list_view->details->double_click_path[1])
    {
        gtk_tree_path_free (list_view->details->double_click_path[1]);
    }
    if (list_view->details->new_selection_path)
    {
        gtk_tree_path_free (list_view->details->new_selection_path);
    }

    g_list_free (list_view->details->cells);
    g_hash_table_destroy (list_view->details->columns);

    if (list_view->details->hover_path != NULL)
    {
        gtk_tree_path_free (list_view->details->hover_path);
    }

    if (list_view->details->column_editor != NULL)
    {
        gtk_widget_destroy (list_view->details->column_editor);
    }

    g_free (list_view->details);

    g_signal_handlers_disconnect_by_func (caja_preferences,
                                          default_sort_order_changed_callback,
                                          list_view);
    g_signal_handlers_disconnect_by_func (caja_list_view_preferences,
                                          default_zoom_level_changed_callback,
                                          list_view);
    g_signal_handlers_disconnect_by_func (caja_list_view_preferences,
                                          default_visible_columns_changed_callback,
                                          list_view);
    g_signal_handlers_disconnect_by_func (caja_list_view_preferences,
                                          default_column_order_changed_callback,
                                          list_view);

    G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
fm_list_view_emblems_changed (FMDirectoryView *directory_view)
{
    g_assert (FM_IS_LIST_VIEW (directory_view));

    /* FIXME: This needs to update the emblems of the icons, since
     * relative emblems may have changed.
     */
}

static char *
fm_list_view_get_first_visible_file (CajaView *view)
{
    CajaFile *file;
    GtkTreePath *path;
    GtkTreeIter iter;
    FMListView *list_view;

    list_view = FM_LIST_VIEW (view);

    if (gtk_tree_view_get_path_at_pos (list_view->details->tree_view,
                                       0, 0,
                                       &path, NULL, NULL, NULL))
    {
        gtk_tree_model_get_iter (GTK_TREE_MODEL (list_view->details->model),
                                 &iter, path);

        gtk_tree_path_free (path);

        gtk_tree_model_get (GTK_TREE_MODEL (list_view->details->model),
                            &iter,
                            FM_LIST_MODEL_FILE_COLUMN, &file,
                            -1);
        if (file)
        {
            char *uri;

            uri = caja_file_get_uri (file);

            caja_file_unref (file);

            return uri;
        }
    }

    return NULL;
}

static void
fm_list_view_scroll_to_file (FMListView *view,
                             CajaFile *file)
{
    GtkTreePath *path;
    GtkTreeIter iter;

    if (!fm_list_model_get_first_iter_for_file (view->details->model, file, &iter))
    {
        return;
    }

    path = gtk_tree_model_get_path (GTK_TREE_MODEL (view->details->model), &iter);

    gtk_tree_view_scroll_to_cell (view->details->tree_view,
                                  path, NULL,
                                  TRUE, 0.0, 0.0);

    gtk_tree_path_free (path);
}

static void
list_view_scroll_to_file (CajaView *view,
                          const char *uri)
{
    CajaFile *file;

    if (uri != NULL)
    {
        /* Only if existing, since we don't want to add the file to
           the directory if it has been removed since then */
        file = caja_file_get_existing_by_uri (uri);
        if (file != NULL)
        {
            fm_list_view_scroll_to_file (FM_LIST_VIEW (view), file);
            caja_file_unref (file);
        }
    }
}

static void
list_view_notify_clipboard_info (CajaClipboardMonitor *monitor,
                                 CajaClipboardInfo *info,
                                 FMListView *view)
{
    /* this could be called as a result of _end_loading() being
     * called after _dispose(), where the model is cleared.
     */
    if (view->details->model == NULL)
    {
        return;
    }

    if (info != NULL && info->cut)
    {
        fm_list_model_set_highlight_for_files (view->details->model, info->files);
    }
    else
    {
        fm_list_model_set_highlight_for_files (view->details->model, NULL);
    }
}

static void
fm_list_view_end_loading (FMDirectoryView *view,
                          gboolean all_files_seen)
{
    CajaClipboardMonitor *monitor;
    CajaClipboardInfo *info;

    monitor = caja_clipboard_monitor_get ();
    info = caja_clipboard_monitor_get_clipboard_info (monitor);

    list_view_notify_clipboard_info (monitor, info, FM_LIST_VIEW (view));
}

static void
real_set_is_active (FMDirectoryView *view,
                    gboolean is_active)
{
    GtkWidget *tree_view;
    GtkStyle *style;
    GdkColor color;

    tree_view = GTK_WIDGET (fm_list_view_get_tree_view (FM_LIST_VIEW (view)));

    if (is_active)
    {
        gtk_widget_modify_base (tree_view, GTK_STATE_NORMAL, NULL);
    }
    else
    {
        style = gtk_widget_get_style (tree_view);
        color = style->base[GTK_STATE_INSENSITIVE];
        gtk_widget_modify_base (tree_view, GTK_STATE_NORMAL, &color);
    }

    EEL_CALL_PARENT (FM_DIRECTORY_VIEW_CLASS,
                     set_is_active, (view, is_active));
}

static void
fm_list_view_class_init (FMListViewClass *class)
{
    FMDirectoryViewClass *fm_directory_view_class;

    fm_directory_view_class = FM_DIRECTORY_VIEW_CLASS (class);

    G_OBJECT_CLASS (class)->dispose = fm_list_view_dispose;
    G_OBJECT_CLASS (class)->finalize = fm_list_view_finalize;

    fm_directory_view_class->add_file = fm_list_view_add_file;
    fm_directory_view_class->begin_loading = fm_list_view_begin_loading;
    fm_directory_view_class->end_loading = fm_list_view_end_loading;
    fm_directory_view_class->bump_zoom_level = fm_list_view_bump_zoom_level;
    fm_directory_view_class->can_zoom_in = fm_list_view_can_zoom_in;
    fm_directory_view_class->can_zoom_out = fm_list_view_can_zoom_out;
    fm_directory_view_class->click_policy_changed = fm_list_view_click_policy_changed;
    fm_directory_view_class->clear = fm_list_view_clear;
    fm_directory_view_class->file_changed = fm_list_view_file_changed;
    fm_directory_view_class->get_background_widget = fm_list_view_get_background_widget;
    fm_directory_view_class->get_selection = fm_list_view_get_selection;
    fm_directory_view_class->get_selection_for_file_transfer = fm_list_view_get_selection_for_file_transfer;
    fm_directory_view_class->get_item_count = fm_list_view_get_item_count;
    fm_directory_view_class->is_empty = fm_list_view_is_empty;
    fm_directory_view_class->remove_file = fm_list_view_remove_file;
    fm_directory_view_class->merge_menus = fm_list_view_merge_menus;
    fm_directory_view_class->unmerge_menus = fm_list_view_unmerge_menus;
    fm_directory_view_class->update_menus = fm_list_view_update_menus;
    fm_directory_view_class->reset_to_defaults = fm_list_view_reset_to_defaults;
    fm_directory_view_class->restore_default_zoom_level = fm_list_view_restore_default_zoom_level;
    fm_directory_view_class->reveal_selection = fm_list_view_reveal_selection;
    fm_directory_view_class->select_all = fm_list_view_select_all;
    fm_directory_view_class->set_selection = fm_list_view_set_selection;
    fm_directory_view_class->invert_selection = fm_list_view_invert_selection;
    fm_directory_view_class->compare_files = fm_list_view_compare_files;
    fm_directory_view_class->sort_directories_first_changed = fm_list_view_sort_directories_first_changed;
    fm_directory_view_class->start_renaming_file = fm_list_view_start_renaming_file;
    fm_directory_view_class->get_zoom_level = fm_list_view_get_zoom_level;
    fm_directory_view_class->zoom_to_level = fm_list_view_zoom_to_level;
    fm_directory_view_class->emblems_changed = fm_list_view_emblems_changed;
    fm_directory_view_class->end_file_changes = fm_list_view_end_file_changes;
    fm_directory_view_class->using_manual_layout = fm_list_view_using_manual_layout;
    fm_directory_view_class->set_is_active = real_set_is_active;

    eel_g_settings_add_auto_enum (caja_preferences,
                                  CAJA_PREFERENCES_CLICK_POLICY,
                                  &click_policy_auto_value);
    eel_g_settings_add_auto_enum (caja_preferences,
                                  CAJA_PREFERENCES_DEFAULT_SORT_ORDER,
                                  (int *) &default_sort_order_auto_value);
    eel_g_settings_add_auto_boolean (caja_preferences,
                                     CAJA_PREFERENCES_DEFAULT_SORT_IN_REVERSE_ORDER,
                                     &default_sort_reversed_auto_value);
    eel_g_settings_add_auto_enum (caja_list_view_preferences,
                                  CAJA_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL,
                                  (int *) &default_zoom_level_auto_value);
    eel_g_settings_add_auto_strv (caja_list_view_preferences,
                                  CAJA_PREFERENCES_LIST_VIEW_DEFAULT_VISIBLE_COLUMNS,
                                  &default_visible_columns_auto_value);
    eel_g_settings_add_auto_strv (caja_list_view_preferences,
                                  CAJA_PREFERENCES_LIST_VIEW_DEFAULT_COLUMN_ORDER,
                                  &default_column_order_auto_value);
}

static const char *
fm_list_view_get_id (CajaView *view)
{
    return FM_LIST_VIEW_ID;
}


static void
fm_list_view_iface_init (CajaViewIface *iface)
{
    fm_directory_view_init_view_iface (iface);

    iface->get_view_id = fm_list_view_get_id;
    iface->get_first_visible_file = fm_list_view_get_first_visible_file;
    iface->scroll_to_file = list_view_scroll_to_file;
    iface->get_title = NULL;
}


static void
fm_list_view_init (FMListView *list_view)
{
    list_view->details = g_new0 (FMListViewDetails, 1);

    create_and_set_up_tree_view (list_view);

    g_signal_connect_swapped (caja_preferences,
                              "changed::" CAJA_PREFERENCES_DEFAULT_SORT_ORDER,
                              G_CALLBACK (default_sort_order_changed_callback),
                              list_view);
    g_signal_connect_swapped (caja_preferences,
                              "changed::" CAJA_PREFERENCES_DEFAULT_SORT_IN_REVERSE_ORDER,
                              G_CALLBACK (default_sort_order_changed_callback),
                              list_view);
    g_signal_connect_swapped (caja_list_view_preferences,
                              "changed::" CAJA_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL,
                              G_CALLBACK (default_zoom_level_changed_callback),
                              list_view);
    g_signal_connect_swapped (caja_list_view_preferences,
                              "changed::" CAJA_PREFERENCES_LIST_VIEW_DEFAULT_VISIBLE_COLUMNS,
                              G_CALLBACK (default_visible_columns_changed_callback),
                              list_view);
    g_signal_connect_swapped (caja_list_view_preferences,
                              "changed::" CAJA_PREFERENCES_LIST_VIEW_DEFAULT_COLUMN_ORDER,
                              G_CALLBACK (default_column_order_changed_callback),
                              list_view);

    fm_list_view_click_policy_changed (FM_DIRECTORY_VIEW (list_view));

    fm_list_view_sort_directories_first_changed (FM_DIRECTORY_VIEW (list_view));

    /* ensure that the zoom level is always set in begin_loading */
    list_view->details->zoom_level = CAJA_ZOOM_LEVEL_SMALLEST - 1;

    list_view->details->hover_path = NULL;
    list_view->details->clipboard_handler_id =
        g_signal_connect (caja_clipboard_monitor_get (),
                          "clipboard_info",
                          G_CALLBACK (list_view_notify_clipboard_info), list_view);
}

static CajaView *
fm_list_view_create (CajaWindowSlotInfo *slot)
{
    FMListView *view;

    view = g_object_new (FM_TYPE_LIST_VIEW,
                         "window-slot", slot,
                         NULL);
    return CAJA_VIEW (view);
}

static gboolean
fm_list_view_supports_uri (const char *uri,
                           GFileType file_type,
                           const char *mime_type)
{
    if (file_type == G_FILE_TYPE_DIRECTORY)
    {
        return TRUE;
    }
    if (strcmp (mime_type, CAJA_SAVED_SEARCH_MIMETYPE) == 0)
    {
        return TRUE;
    }
    if (g_str_has_prefix (uri, "trash:"))
    {
        return TRUE;
    }
    if (g_str_has_prefix (uri, EEL_SEARCH_URI))
    {
        return TRUE;
    }

    return FALSE;
}

static CajaViewInfo fm_list_view =
{
    FM_LIST_VIEW_ID,
    /* translators: this is used in the view selection dropdown
     * of navigation windows and in the preferences dialog */
    N_("List View"),
    /* translators: this is used in the view menu */
    N_("_List"),
    N_("The list view encountered an error."),
    N_("The list view encountered an error while starting up."),
    N_("Display this location with the list view."),
    fm_list_view_create,
    fm_list_view_supports_uri
};

void
fm_list_view_register (void)
{
    fm_list_view.view_combo_label = _(fm_list_view.view_combo_label);
    fm_list_view.view_menu_label_with_mnemonic = _(fm_list_view.view_menu_label_with_mnemonic);
    fm_list_view.error_label = _(fm_list_view.error_label);
    fm_list_view.startup_error_label = _(fm_list_view.startup_error_label);
    fm_list_view.display_location_label = _(fm_list_view.display_location_label);

    caja_view_factory_register (&fm_list_view);
}

GtkTreeView*
fm_list_view_get_tree_view (FMListView *list_view)
{
    return list_view->details->tree_view;
}
