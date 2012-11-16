/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Caja
 *
 * Copyright (C) 2002 Sun Microsystems, Inc.
 *
 * Caja is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Caja is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Author: Dave Camp <dave@ximian.com>
 * XDS support: Benedikt Meurer <benny@xfce.org> (adapted by Amos Brocco <amos.brocco@unifr.ch>)
 */

/* caja-tree-view-drag-dest.c: Handles drag and drop for treeviews which
 *                                 contain a hierarchy of files
 */

#include <config.h>
#include "caja-tree-view-drag-dest.h"

#include <eel/eel-gtk-macros.h>
#include <gtk/gtk.h>
#include "caja-file-dnd.h"
#include "caja-file-changes-queue.h"
#include "caja-icon-dnd.h"
#include "caja-link.h"
#include "caja-marshal.h"
#include "caja-debug-log.h"
#include <stdio.h>
#include <string.h>

#define AUTO_SCROLL_MARGIN 20

#define HOVER_EXPAND_TIMEOUT 1

struct _CajaTreeViewDragDestDetails
{
    GtkTreeView *tree_view;

    gboolean drop_occurred;

    gboolean have_drag_data;
    guint drag_type;
    GtkSelectionData *drag_data;
    GList *drag_list;

    guint highlight_id;
    guint scroll_id;
    guint expand_id;

    char *direct_save_uri;
};

enum
{
    GET_ROOT_URI,
    GET_FILE_FOR_PATH,
    MOVE_COPY_ITEMS,
    HANDLE_NETSCAPE_URL,
    HANDLE_URI_LIST,
    HANDLE_TEXT,
    HANDLE_RAW,
    LAST_SIGNAL
};

static void caja_tree_view_drag_dest_init (CajaTreeViewDragDest      *dest);
static void caja_tree_view_drag_dest_class_init    (CajaTreeViewDragDestClass *class);

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (CajaTreeViewDragDest, caja_tree_view_drag_dest,
               G_TYPE_OBJECT);
#define parent_class caja_tree_view_drag_dest_parent_class

static const GtkTargetEntry drag_types [] =
{
    { CAJA_ICON_DND_MATE_ICON_LIST_TYPE, 0, CAJA_ICON_DND_MATE_ICON_LIST },
    /* prefer "_NETSCAPE_URL" over "text/uri-list" to satisfy web browsers. */
    { CAJA_ICON_DND_NETSCAPE_URL_TYPE, 0, CAJA_ICON_DND_NETSCAPE_URL },
    { CAJA_ICON_DND_URI_LIST_TYPE, 0, CAJA_ICON_DND_URI_LIST },
    { CAJA_ICON_DND_KEYWORD_TYPE, 0, CAJA_ICON_DND_KEYWORD },
    { CAJA_ICON_DND_XDNDDIRECTSAVE_TYPE, 0, CAJA_ICON_DND_XDNDDIRECTSAVE }, /* XDS Protocol Type */
    { CAJA_ICON_DND_RAW_TYPE, 0, CAJA_ICON_DND_RAW }
};


static void
gtk_tree_view_vertical_autoscroll (GtkTreeView *tree_view)
{
    GdkRectangle visible_rect;
    GtkAdjustment *vadjustment;
    GdkWindow *window;
    int y;
    int offset;
    float value;

    window = gtk_tree_view_get_bin_window (tree_view);

#if GTK_CHECK_VERSION(3, 0, 0)
    vadjustment = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE(tree_view));
#else
    vadjustment = gtk_tree_view_get_vadjustment (tree_view);
#endif

    gdk_window_get_pointer (window, NULL, &y, NULL);

    y += gtk_adjustment_get_value (vadjustment);

    gtk_tree_view_get_visible_rect (tree_view, &visible_rect);

    offset = y - (visible_rect.y + 2 * AUTO_SCROLL_MARGIN);
    if (offset > 0)
    {
        offset = y - (visible_rect.y + visible_rect.height - 2 * AUTO_SCROLL_MARGIN);
        if (offset < 0)
        {
            return;
        }
    }

    value = CLAMP (gtk_adjustment_get_value (vadjustment) + offset, 0.0,
                   gtk_adjustment_get_upper (vadjustment) - gtk_adjustment_get_page_size (vadjustment));
    gtk_adjustment_set_value (vadjustment, value);
}

static int
scroll_timeout (gpointer data)
{
    GtkTreeView *tree_view = GTK_TREE_VIEW (data);

    gtk_tree_view_vertical_autoscroll (tree_view);

    return TRUE;
}

static void
remove_scroll_timeout (CajaTreeViewDragDest *dest)
{
    if (dest->details->scroll_id)
    {
        g_source_remove (dest->details->scroll_id);
        dest->details->scroll_id = 0;
    }
}

static int
expand_timeout (gpointer data)
{
    GtkTreeView *tree_view;
    GtkTreePath *drop_path;

    tree_view = GTK_TREE_VIEW (data);

    gtk_tree_view_get_drag_dest_row (tree_view, &drop_path, NULL);

    if (drop_path)
    {
        gtk_tree_view_expand_row (tree_view, drop_path, FALSE);
        gtk_tree_path_free (drop_path);
    }

    return FALSE;
}

static void
remove_expand_timeout (CajaTreeViewDragDest *dest)
{
    if (dest->details->expand_id)
    {
        g_source_remove (dest->details->expand_id);
        dest->details->expand_id = 0;
    }
}

static gboolean
#if GTK_CHECK_VERSION(3,0,0)
highlight_draw (GtkWidget *widget,
		cairo_t   *cr,
                gpointer data)
#else
highlight_expose (GtkWidget *widget,
                  GdkEventExpose *event,
                  gpointer data)
#endif
{
    GdkWindow *bin_window;
    int width;
    int height;

    /* FIXMEchpe: is bin window right here??? */
    bin_window = gtk_tree_view_get_bin_window (GTK_TREE_VIEW (widget));

#if GTK_CHECK_VERSION(3, 0, 0)
    width = gdk_window_get_width(bin_window);
    height = gdk_window_get_height(bin_window);
#else
    gdk_drawable_get_size(bin_window, &width, &height);
#endif

    gtk_paint_focus (gtk_widget_get_style (widget),
#if GTK_CHECK_VERSION(3,0,0)
                     cr,
                     gtk_widget_get_state (widget),
#else
                     bin_window,
                     gtk_widget_get_state (widget),
                     NULL,
#endif
                     widget,
                     "treeview-drop-indicator",
                     0, 0, width, height);

    return FALSE;
}

static void
set_widget_highlight (CajaTreeViewDragDest *dest, gboolean highlight)
{
    if (!highlight && dest->details->highlight_id)
    {
        g_signal_handler_disconnect (dest->details->tree_view,
                                     dest->details->highlight_id);
        dest->details->highlight_id = 0;
        gtk_widget_queue_draw (GTK_WIDGET (dest->details->tree_view));
    }

    if (highlight && !dest->details->highlight_id)
    {
        dest->details->highlight_id =
            g_signal_connect_object (dest->details->tree_view,
#if GTK_CHECK_VERSION(3,0,0)
                                     "draw",
                                     G_CALLBACK (highlight_draw),
#else
                                     "expose_event",
                                     G_CALLBACK (highlight_expose),
#endif
                                     dest, 0);
        gtk_widget_queue_draw (GTK_WIDGET (dest->details->tree_view));
    }
}

static void
set_drag_dest_row (CajaTreeViewDragDest *dest,
                   GtkTreePath *path)
{
    if (path)
    {
        set_widget_highlight (dest, FALSE);
        gtk_tree_view_set_drag_dest_row
        (dest->details->tree_view,
         path,
         GTK_TREE_VIEW_DROP_INTO_OR_BEFORE);
    }
    else
    {
        set_widget_highlight (dest, TRUE);
        gtk_tree_view_set_drag_dest_row (dest->details->tree_view,
                                         NULL,
                                         0);
    }
}

static void
clear_drag_dest_row (CajaTreeViewDragDest *dest)
{
    gtk_tree_view_set_drag_dest_row (dest->details->tree_view, NULL, 0);
    set_widget_highlight (dest, FALSE);
}

static gboolean
get_drag_data (CajaTreeViewDragDest *dest,
               GdkDragContext *context,
               guint32 time)
{
    GdkAtom target;

    target = gtk_drag_dest_find_target (GTK_WIDGET (dest->details->tree_view),
                                        context,
                                        NULL);

    if (target == GDK_NONE)
    {
        return FALSE;
    }

    if (target == gdk_atom_intern (CAJA_ICON_DND_XDNDDIRECTSAVE_TYPE, FALSE) &&
            !dest->details->drop_occurred)
    {
        dest->details->drag_type = CAJA_ICON_DND_XDNDDIRECTSAVE;
        dest->details->have_drag_data = TRUE;
        return TRUE;
    }

    gtk_drag_get_data (GTK_WIDGET (dest->details->tree_view),
                       context, target, time);

    return TRUE;
}

static void
free_drag_data (CajaTreeViewDragDest *dest)
{
    dest->details->have_drag_data = FALSE;

    if (dest->details->drag_data)
    {
        gtk_selection_data_free (dest->details->drag_data);
        dest->details->drag_data = NULL;
    }

    if (dest->details->drag_list)
    {
        caja_drag_destroy_selection_list (dest->details->drag_list);
        dest->details->drag_list = NULL;
    }

    g_free (dest->details->direct_save_uri);
    dest->details->direct_save_uri = NULL;
}

static char *
get_root_uri (CajaTreeViewDragDest *dest)
{
    char *uri;

    g_signal_emit (dest, signals[GET_ROOT_URI], 0, &uri);

    return uri;
}

static CajaFile *
file_for_path (CajaTreeViewDragDest *dest, GtkTreePath *path)
{
    CajaFile *file;
    char *uri;

    if (path)
    {
        g_signal_emit (dest, signals[GET_FILE_FOR_PATH], 0, path, &file);
    }
    else
    {
        uri = get_root_uri (dest);

        file = NULL;
        if (uri != NULL)
        {
            file = caja_file_get_by_uri (uri);
        }

        g_free (uri);
    }

    return file;
}

static GtkTreePath *
get_drop_path (CajaTreeViewDragDest *dest,
               GtkTreePath *path)
{
    CajaFile *file;
    GtkTreePath *ret;

    if (!path || !dest->details->have_drag_data)
    {
        return NULL;
    }

    ret = gtk_tree_path_copy (path);
    file = file_for_path (dest, ret);

    /* Go up the tree until we find a file that can accept a drop */
    while (file == NULL /* dummy row */ ||
            !caja_drag_can_accept_info (file,
                                        dest->details->drag_type,
                                        dest->details->drag_list))
    {
        if (gtk_tree_path_get_depth (ret) == 1)
        {
            gtk_tree_path_free (ret);
            ret = NULL;
            break;
        }
        else
        {
            gtk_tree_path_up (ret);

            caja_file_unref (file);
            file = file_for_path (dest, ret);
        }
    }
    caja_file_unref (file);

    return ret;
}

static char *
get_drop_target_uri_for_path (CajaTreeViewDragDest *dest,
                              GtkTreePath *path)
{
    CajaFile *file;
    char *target;

    file = file_for_path (dest, path);
    if (file == NULL)
    {
        return NULL;
    }

    target = caja_file_get_drop_target_uri (file);
    caja_file_unref (file);

    return target;
}

static guint
get_drop_action (CajaTreeViewDragDest *dest,
                 GdkDragContext *context,
                 GtkTreePath *path)
{
    char *drop_target;
    int action;

    if (!dest->details->have_drag_data ||
            (dest->details->drag_type == CAJA_ICON_DND_MATE_ICON_LIST &&
             dest->details->drag_list == NULL))
    {
        return 0;
    }

    switch (dest->details->drag_type)
    {
    case CAJA_ICON_DND_MATE_ICON_LIST :
        drop_target = get_drop_target_uri_for_path (dest, path);

        if (!drop_target)
        {
            return 0;
        }

        caja_drag_default_drop_action_for_icons
        (context,
         drop_target,
         dest->details->drag_list,
         &action);

        g_free (drop_target);

        return action;

    case CAJA_ICON_DND_NETSCAPE_URL:
        drop_target = get_drop_target_uri_for_path (dest, path);

        if (drop_target == NULL)
        {
            return 0;
        }

        action = caja_drag_default_drop_action_for_netscape_url (context);

        g_free (drop_target);

        return action;

    case CAJA_ICON_DND_URI_LIST :
        drop_target = get_drop_target_uri_for_path (dest, path);

        if (drop_target == NULL)
        {
            return 0;
        }

        g_free (drop_target);

        return gdk_drag_context_get_suggested_action (context);

    case CAJA_ICON_DND_TEXT:
    case CAJA_ICON_DND_RAW:
    case CAJA_ICON_DND_XDNDDIRECTSAVE:
        return GDK_ACTION_COPY;

    case CAJA_ICON_DND_KEYWORD:

        if (!path)
        {
            return 0;
        }

        return GDK_ACTION_COPY;
    }

    return 0;
}

static gboolean
drag_motion_callback (GtkWidget *widget,
                      GdkDragContext *context,
                      int x,
                      int y,
                      guint32 time,
                      gpointer data)
{
    CajaTreeViewDragDest *dest;
    GtkTreePath *path;
    GtkTreePath *drop_path, *old_drop_path;
    GtkTreeModel *model;
    GtkTreeIter drop_iter;
    GtkTreeViewDropPosition pos;
    GdkWindow *bin_window;
    guint action;
    gboolean res = TRUE;

    dest = CAJA_TREE_VIEW_DRAG_DEST (data);

    gtk_tree_view_get_dest_row_at_pos (GTK_TREE_VIEW (widget),
                                       x, y, &path, &pos);


    if (!dest->details->have_drag_data)
    {
        res = get_drag_data (dest, context, time);
    }

    if (!res)
    {
        return FALSE;
    }

    drop_path = get_drop_path (dest, path);

    action = 0;
    bin_window = gtk_tree_view_get_bin_window (GTK_TREE_VIEW (widget));
    if (bin_window != NULL)
    {
        int bin_x, bin_y;
        gdk_window_get_position (bin_window, &bin_x, &bin_y);
        if (bin_y <= y)
        {
            /* ignore drags on the header */
            action = get_drop_action (dest, context, drop_path);
        }
    }

    gtk_tree_view_get_drag_dest_row (GTK_TREE_VIEW (widget), &old_drop_path,
                                     NULL);

    if (action)
    {
        set_drag_dest_row (dest, drop_path);
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
        if (drop_path == NULL || (old_drop_path != NULL &&
                                  gtk_tree_path_compare (old_drop_path, drop_path) != 0))
        {
            remove_expand_timeout (dest);
        }
        if (dest->details->expand_id == 0 && drop_path != NULL)
        {
            gtk_tree_model_get_iter (model, &drop_iter, drop_path);
            if (gtk_tree_model_iter_has_child (model, &drop_iter))
            {
                dest->details->expand_id = g_timeout_add_seconds (HOVER_EXPAND_TIMEOUT,
                                           expand_timeout,
                                           dest->details->tree_view);
            }
        }
    }
    else
    {
        clear_drag_dest_row (dest);
        remove_expand_timeout (dest);
    }

    if (path)
    {
        gtk_tree_path_free (path);
    }

    if (drop_path)
    {
        gtk_tree_path_free (drop_path);
    }

    if (old_drop_path)
    {
        gtk_tree_path_free (old_drop_path);
    }

    if (dest->details->scroll_id == 0)
    {
        dest->details->scroll_id =
            g_timeout_add (150,
                           scroll_timeout,
                           dest->details->tree_view);
    }

    gdk_drag_status (context, action, time);

    return TRUE;
}

static void
drag_leave_callback (GtkWidget *widget,
                     GdkDragContext *context,
                     guint32 time,
                     gpointer data)
{
    CajaTreeViewDragDest *dest;

    dest = CAJA_TREE_VIEW_DRAG_DEST (data);

    clear_drag_dest_row (dest);

    free_drag_data (dest);

    remove_scroll_timeout (dest);
    remove_expand_timeout (dest);
}

static char *
get_drop_target_uri_at_pos (CajaTreeViewDragDest *dest, int x, int y)
{
    char *drop_target;
    GtkTreePath *path;
    GtkTreePath *drop_path;
    GtkTreeViewDropPosition pos;

    gtk_tree_view_get_dest_row_at_pos (dest->details->tree_view, x, y,
                                       &path, &pos);

    drop_path = get_drop_path (dest, path);

    drop_target = get_drop_target_uri_for_path (dest, drop_path);

    if (path != NULL)
    {
        gtk_tree_path_free (path);
    }

    if (drop_path != NULL)
    {
        gtk_tree_path_free (drop_path);
    }

    return drop_target;
}

static void
receive_uris (CajaTreeViewDragDest *dest,
              GdkDragContext *context,
              GList *source_uris,
              int x, int y)
{
    char *drop_target;
    GdkDragAction action, real_action;

    drop_target = get_drop_target_uri_at_pos (dest, x, y);
    g_assert (drop_target != NULL);

    real_action = gdk_drag_context_get_selected_action (context);

    if (real_action == GDK_ACTION_ASK)
    {
        if (caja_drag_selection_includes_special_link (dest->details->drag_list))
        {
            /* We only want to move the trash */
            action = GDK_ACTION_MOVE;
        }
        else
        {
            action = GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK;
        }
        real_action = caja_drag_drop_action_ask
                      (GTK_WIDGET (dest->details->tree_view), action);
    }

    /* We only want to copy external uris */
    if (dest->details->drag_type == CAJA_ICON_DND_URI_LIST)
    {
        action = GDK_ACTION_COPY;
    }

    if (real_action > 0)
    {
        if (!caja_drag_uris_local (drop_target, source_uris)
                || real_action != GDK_ACTION_MOVE)
        {
            g_signal_emit (dest, signals[MOVE_COPY_ITEMS], 0,
                           source_uris,
                           drop_target,
                           real_action,
                           x, y);
        }
    }

    g_free (drop_target);
}

static void
receive_dropped_icons (CajaTreeViewDragDest *dest,
                       GdkDragContext *context,
                       int x, int y)
{
    GList *source_uris;
    GList *l;

    /* FIXME: ignore local only moves */

    if (!dest->details->drag_list)
    {
        return;
    }

    source_uris = NULL;
    for (l = dest->details->drag_list; l != NULL; l = l->next)
    {
        source_uris = g_list_prepend (source_uris,
                                      ((CajaDragSelectionItem *)l->data)->uri);
    }

    source_uris = g_list_reverse (source_uris);

    receive_uris (dest, context, source_uris, x, y);

    g_list_free (source_uris);
}

static void
receive_dropped_uri_list (CajaTreeViewDragDest *dest,
                          GdkDragContext *context,
                          int x, int y)
{
    char *drop_target;

    if (!dest->details->drag_data)
    {
        return;
    }

    drop_target = get_drop_target_uri_at_pos (dest, x, y);
    g_assert (drop_target != NULL);

    g_signal_emit (dest, signals[HANDLE_URI_LIST], 0,
                   (char*) gtk_selection_data_get_data (dest->details->drag_data),
                   drop_target,
                   gdk_drag_context_get_selected_action (context),
                   x, y);

    g_free (drop_target);
}

static void
receive_dropped_text (CajaTreeViewDragDest *dest,
                      GdkDragContext *context,
                      int x, int y)
{
    char *drop_target;
    char *text;

    if (!dest->details->drag_data)
    {
        return;
    }

    drop_target = get_drop_target_uri_at_pos (dest, x, y);
    g_assert (drop_target != NULL);

    text = gtk_selection_data_get_text (dest->details->drag_data);
    g_signal_emit (dest, signals[HANDLE_TEXT], 0,
                   (char *) text, drop_target,
                   gdk_drag_context_get_selected_action (context),
                   x, y);

    g_free (text);
    g_free (drop_target);
}

static void
receive_dropped_raw (CajaTreeViewDragDest *dest,
                     const char *raw_data, int length,
                     GdkDragContext *context,
                     int x, int y)
{
    char *drop_target;

    if (!dest->details->drag_data)
    {
        return;
    }

    drop_target = get_drop_target_uri_at_pos (dest, x, y);
    g_assert (drop_target != NULL);

    g_signal_emit (dest, signals[HANDLE_RAW], 0,
                   raw_data, length, drop_target,
                   dest->details->direct_save_uri,
                   gdk_drag_context_get_selected_action (context),
                   x, y);

    g_free (drop_target);
}

static void
receive_dropped_netscape_url (CajaTreeViewDragDest *dest,
                              GdkDragContext *context,
                              int x, int y)
{
    char *drop_target;

    if (!dest->details->drag_data)
    {
        return;
    }

    drop_target = get_drop_target_uri_at_pos (dest, x, y);
    g_assert (drop_target != NULL);

    g_signal_emit (dest, signals[HANDLE_NETSCAPE_URL], 0,
                   (char*) gtk_selection_data_get_data (dest->details->drag_data),
                   drop_target,
                   gdk_drag_context_get_selected_action (context),
                   x, y);

    g_free (drop_target);
}

static void
receive_dropped_keyword (CajaTreeViewDragDest *dest,
                         GdkDragContext *context,
                         int x, int y)
{
    char *drop_target_uri;
    CajaFile *drop_target_file;

    if (!dest->details->drag_data)
    {
        return;
    }

    drop_target_uri = get_drop_target_uri_at_pos (dest, x, y);
    g_assert (drop_target_uri != NULL);

    drop_target_file = caja_file_get_by_uri (drop_target_uri);

    if (drop_target_file != NULL)
    {
        caja_drag_file_receive_dropped_keyword (drop_target_file,
                                                (char *) gtk_selection_data_get_data (dest->details->drag_data));
        caja_file_unref (drop_target_file);
    }

    g_free (drop_target_uri);
}

static gboolean
receive_xds (CajaTreeViewDragDest *dest,
             GtkWidget *widget,
             guint32 time,
             GdkDragContext *context,
             int x, int y)
{
    GFile *location;
    const guchar *selection_data;
    gint selection_format;
    gint selection_length;

    selection_data = gtk_selection_data_get_data (dest->details->drag_data);
    selection_format = gtk_selection_data_get_format (dest->details->drag_data);
    selection_length = gtk_selection_data_get_length (dest->details->drag_data);

    if (selection_format == 8
            && selection_length == 1
            && selection_data[0] == 'F')
    {
        gtk_drag_get_data (widget, context,
                           gdk_atom_intern (CAJA_ICON_DND_RAW_TYPE,
                                            FALSE),
                           time);
        return FALSE;
    }
    else if (selection_format == 8
             && selection_length == 1
             && selection_data[0] == 'S')
    {
        g_assert (dest->details->direct_save_uri != NULL);
        location = g_file_new_for_uri (dest->details->direct_save_uri);

        caja_file_changes_queue_file_added (location);
        caja_file_changes_consume_changes (TRUE);

        g_object_unref (location);
    }
    return TRUE;
}


static gboolean
drag_data_received_callback (GtkWidget *widget,
                             GdkDragContext *context,
                             int x,
                             int y,
                             GtkSelectionData *selection_data,
                             guint info,
                             guint32 time,
                             gpointer data)
{
    CajaTreeViewDragDest *dest;
    const char *tmp;
    int length;
    gboolean success, finished;

    dest = CAJA_TREE_VIEW_DRAG_DEST (data);

    if (!dest->details->have_drag_data)
    {
        dest->details->have_drag_data = TRUE;
        dest->details->drag_type = info;
        dest->details->drag_data =
            gtk_selection_data_copy (selection_data);
        if (info == CAJA_ICON_DND_MATE_ICON_LIST)
        {
            dest->details->drag_list =
                caja_drag_build_selection_list (selection_data);
        }
    }

    if (dest->details->drop_occurred)
    {
        success = FALSE;
        finished = TRUE;
        switch (info)
        {
        case CAJA_ICON_DND_MATE_ICON_LIST :
            receive_dropped_icons (dest, context, x, y);
            success = TRUE;
            break;
        case CAJA_ICON_DND_NETSCAPE_URL :
            receive_dropped_netscape_url (dest, context, x, y);
            success = TRUE;
            break;
        case CAJA_ICON_DND_URI_LIST :
            receive_dropped_uri_list (dest, context, x, y);
            success = TRUE;
            break;
        case CAJA_ICON_DND_TEXT:
            receive_dropped_text (dest, context, x, y);
            success = TRUE;
            break;
        case CAJA_ICON_DND_RAW:
            length = gtk_selection_data_get_length (selection_data);
            tmp = gtk_selection_data_get_data (selection_data);
            receive_dropped_raw (dest, tmp, length, context, x, y);
            success = TRUE;
            break;
        case CAJA_ICON_DND_KEYWORD:
            receive_dropped_keyword (dest, context, x, y);
            success = TRUE;
            break;
        case CAJA_ICON_DND_XDNDDIRECTSAVE:
            finished = receive_xds (dest, widget, time, context, x, y);
            success = TRUE;
            break;
        }

        if (finished)
        {
            dest->details->drop_occurred = FALSE;
            free_drag_data (dest);
            gtk_drag_finish (context, success, FALSE, time);
        }
    }

    /* appease GtkTreeView by preventing its drag_data_receive
     * from being called */
    g_signal_stop_emission_by_name (dest->details->tree_view,
                                    "drag_data_received");

    return TRUE;
}

static char *
get_direct_save_filename (GdkDragContext *context)
{
    guchar *prop_text;
    gint prop_len;

    if (!gdk_property_get (gdk_drag_context_get_source_window (context), gdk_atom_intern (CAJA_ICON_DND_XDNDDIRECTSAVE_TYPE, FALSE),
                           gdk_atom_intern ("text/plain", FALSE), 0, 1024, FALSE, NULL, NULL,
                           &prop_len, &prop_text))
    {
        return NULL;
    }

    /* Zero-terminate the string */
    prop_text = g_realloc (prop_text, prop_len + 1);
    prop_text[prop_len] = '\0';

    /* Verify that the file name provided by the source is valid */
    if (*prop_text == '\0' ||
            strchr ((const gchar *) prop_text, G_DIR_SEPARATOR) != NULL)
    {
        caja_debug_log (FALSE, CAJA_DEBUG_LOG_DOMAIN_USER,
                        "Invalid filename provided by XDS drag site");
        g_free (prop_text);
        return NULL;
    }

    return prop_text;
}

static gboolean
set_direct_save_uri (CajaTreeViewDragDest *dest,
                     GdkDragContext *context,
                     int x, int y)
{
    GFile *base, *child;
    char *drop_uri;
    char *filename, *uri;

    g_assert (dest->details->direct_save_uri == NULL);

    uri = NULL;

    drop_uri = get_drop_target_uri_at_pos (dest, x, y);
    if (drop_uri != NULL)
    {
        filename = get_direct_save_filename (context);
        if (filename != NULL)
        {
            /* Resolve relative path */
            base = g_file_new_for_uri (drop_uri);
            child = g_file_get_child (base, filename);
            uri = g_file_get_uri (child);

            g_object_unref (base);
            g_object_unref (child);

            /* Change the property */
            gdk_property_change (GDK_DRAWABLE (gdk_drag_context_get_source_window (context)),
                                 gdk_atom_intern (CAJA_ICON_DND_XDNDDIRECTSAVE_TYPE, FALSE),
                                 gdk_atom_intern ("text/plain", FALSE), 8,
                                 GDK_PROP_MODE_REPLACE, (const guchar *) uri,
                                 strlen (uri));

            dest->details->direct_save_uri = uri;
        }
        else
        {
            caja_debug_log (FALSE, CAJA_DEBUG_LOG_DOMAIN_USER,
                            "Invalid filename provided by XDS drag site");
        }
    }
    else
    {
        caja_debug_log (FALSE, CAJA_DEBUG_LOG_DOMAIN_USER,
                        "Could not retrieve XDS drop destination");
    }

    return uri != NULL;
}

static gboolean
drag_drop_callback (GtkWidget *widget,
                    GdkDragContext *context,
                    int x,
                    int y,
                    guint32 time,
                    gpointer data)
{
    CajaTreeViewDragDest *dest;
    guint info;
    GdkAtom target;

    dest = CAJA_TREE_VIEW_DRAG_DEST (data);

    target = gtk_drag_dest_find_target (GTK_WIDGET (dest->details->tree_view),
                                        context,
                                        NULL);
    if (target == GDK_NONE)
    {
        return FALSE;
    }

    info = dest->details->drag_type;

    if (info == CAJA_ICON_DND_XDNDDIRECTSAVE)
    {
        /* We need to set this or get_drop_path will fail, and it
           was unset by drag_leave_callback */
        dest->details->have_drag_data = TRUE;
        if (!set_direct_save_uri (dest, context, x, y))
        {
            return FALSE;
        }
        dest->details->have_drag_data = FALSE;
    }

    dest->details->drop_occurred = TRUE;

    get_drag_data (dest, context, time);
    remove_scroll_timeout (dest);
    remove_expand_timeout (dest);
    clear_drag_dest_row (dest);

    return TRUE;
}

static void
tree_view_weak_notify (gpointer user_data,
                       GObject *object)
{
    CajaTreeViewDragDest *dest;

    dest = CAJA_TREE_VIEW_DRAG_DEST (user_data);

    remove_scroll_timeout (dest);
    remove_expand_timeout (dest);

    dest->details->tree_view = NULL;
}

static void
caja_tree_view_drag_dest_dispose (GObject *object)
{
    CajaTreeViewDragDest *dest;

    dest = CAJA_TREE_VIEW_DRAG_DEST (object);

    if (dest->details->tree_view)
    {
        g_object_weak_unref (G_OBJECT (dest->details->tree_view),
                             tree_view_weak_notify,
                             dest);
    }

    remove_scroll_timeout (dest);
    remove_expand_timeout (dest);

    EEL_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}

static void
caja_tree_view_drag_dest_finalize (GObject *object)
{
    CajaTreeViewDragDest *dest;

    dest = CAJA_TREE_VIEW_DRAG_DEST (object);

    free_drag_data (dest);

    g_free (dest->details);

    EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
caja_tree_view_drag_dest_init (CajaTreeViewDragDest *dest)
{
    dest->details = g_new0 (CajaTreeViewDragDestDetails, 1);
}

static void
caja_tree_view_drag_dest_class_init (CajaTreeViewDragDestClass *class)
{
    GObjectClass *gobject_class;

    gobject_class = G_OBJECT_CLASS (class);

    gobject_class->dispose = caja_tree_view_drag_dest_dispose;
    gobject_class->finalize = caja_tree_view_drag_dest_finalize;

    signals[GET_ROOT_URI] =
        g_signal_new ("get_root_uri",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (CajaTreeViewDragDestClass,
                                       get_root_uri),
                      NULL, NULL,
                      caja_marshal_STRING__VOID,
                      G_TYPE_STRING, 0);
    signals[GET_FILE_FOR_PATH] =
        g_signal_new ("get_file_for_path",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (CajaTreeViewDragDestClass,
                                       get_file_for_path),
                      NULL, NULL,
                      caja_marshal_OBJECT__BOXED,
                      CAJA_TYPE_FILE, 1,
                      GTK_TYPE_TREE_PATH);
    signals[MOVE_COPY_ITEMS] =
        g_signal_new ("move_copy_items",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (CajaTreeViewDragDestClass,
                                       move_copy_items),
                      NULL, NULL,

                      caja_marshal_VOID__POINTER_STRING_ENUM_INT_INT,
                      G_TYPE_NONE, 5,
                      G_TYPE_POINTER,
                      G_TYPE_STRING,
                      GDK_TYPE_DRAG_ACTION,
                      G_TYPE_INT,
                      G_TYPE_INT);
    signals[HANDLE_NETSCAPE_URL] =
        g_signal_new ("handle_netscape_url",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (CajaTreeViewDragDestClass,
                                       handle_netscape_url),
                      NULL, NULL,
                      caja_marshal_VOID__STRING_STRING_ENUM_INT_INT,
                      G_TYPE_NONE, 5,
                      G_TYPE_STRING,
                      G_TYPE_STRING,
                      GDK_TYPE_DRAG_ACTION,
                      G_TYPE_INT,
                      G_TYPE_INT);
    signals[HANDLE_URI_LIST] =
        g_signal_new ("handle_uri_list",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (CajaTreeViewDragDestClass,
                                       handle_uri_list),
                      NULL, NULL,
                      caja_marshal_VOID__STRING_STRING_ENUM_INT_INT,
                      G_TYPE_NONE, 5,
                      G_TYPE_STRING,
                      G_TYPE_STRING,
                      GDK_TYPE_DRAG_ACTION,
                      G_TYPE_INT,
                      G_TYPE_INT);
    signals[HANDLE_TEXT] =
        g_signal_new ("handle_text",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (CajaTreeViewDragDestClass,
                                       handle_text),
                      NULL, NULL,
                      caja_marshal_VOID__STRING_STRING_ENUM_INT_INT,
                      G_TYPE_NONE, 5,
                      G_TYPE_STRING,
                      G_TYPE_STRING,
                      GDK_TYPE_DRAG_ACTION,
                      G_TYPE_INT,
                      G_TYPE_INT);
    signals[HANDLE_RAW] =
        g_signal_new ("handle_raw",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (CajaTreeViewDragDestClass,
                                       handle_raw),
                      NULL, NULL,
                      caja_marshal_VOID__POINTER_INT_STRING_STRING_ENUM_INT_INT,
                      G_TYPE_NONE, 7,
                      G_TYPE_POINTER,
                      G_TYPE_INT,
                      G_TYPE_STRING,
                      G_TYPE_STRING,
                      GDK_TYPE_DRAG_ACTION,
                      G_TYPE_INT,
                      G_TYPE_INT);
}



CajaTreeViewDragDest *
caja_tree_view_drag_dest_new (GtkTreeView *tree_view)
{
    CajaTreeViewDragDest *dest;
    GtkTargetList *targets;

    dest = g_object_new (CAJA_TYPE_TREE_VIEW_DRAG_DEST, NULL);

    dest->details->tree_view = tree_view;
    g_object_weak_ref (G_OBJECT (dest->details->tree_view),
                       tree_view_weak_notify, dest);

    gtk_drag_dest_set (GTK_WIDGET (tree_view),
                       0, drag_types, G_N_ELEMENTS (drag_types),
                       GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK | GDK_ACTION_ASK);

    targets = gtk_drag_dest_get_target_list (GTK_WIDGET (tree_view));
    gtk_target_list_add_text_targets (targets, CAJA_ICON_DND_TEXT);

    g_signal_connect_object (tree_view,
                             "drag_motion",
                             G_CALLBACK (drag_motion_callback),
                             dest, 0);
    g_signal_connect_object (tree_view,
                             "drag_leave",
                             G_CALLBACK (drag_leave_callback),
                             dest, 0);
    g_signal_connect_object (tree_view,
                             "drag_drop",
                             G_CALLBACK (drag_drop_callback),
                             dest, 0);
    g_signal_connect_object (tree_view,
                             "drag_data_received",
                             G_CALLBACK (drag_data_received_callback),
                             dest, 0);

    return dest;
}
