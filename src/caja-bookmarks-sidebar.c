/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 *  Caja:  Bookmarks sidebar
 *
 *  Copyright (C) 2020 Gordon N. Squash.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  Authors: Gordon N. Squash
 *
 *  Based largely on the Caja Places sidebar and the Caja History sidebar.
 *
 */

#include <config.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <cairo-gobject.h>

#include <eel/eel-gtk-extensions.h>

#include <libcaja-private/caja-file.h>
#include <libcaja-private/caja-file-utilities.h>
#include <libcaja-private/caja-file-operations.h>

#include <libcaja-private/caja-bookmark.h>
#include <libcaja-private/caja-global-preferences.h>
#include <libcaja-private/caja-sidebar-provider.h>
#include <libcaja-private/caja-module.h>
#include <libcaja-private/caja-signaller.h>
#include <libcaja-private/caja-window-info.h>
#include <libcaja-private/caja-window-slot-info.h>

#include "caja-bookmark-list.h"
#include "caja-window.h"

#include "caja-bookmarks-sidebar.h"


typedef struct
{
    GtkScrolledWindowClass parent;
} CajaBookmarksSidebarClass;

typedef struct
{
    GObject parent;
} CajaBookmarksSidebarProvider;

typedef struct
{
    GObjectClass parent;
} CajaBookmarksSidebarProviderClass;

enum
{
    BOOKMARKS_SIDEBAR_COLUMN_ICON,
    BOOKMARKS_SIDEBAR_COLUMN_NAME,
    BOOKMARKS_SIDEBAR_COLUMN_STYLE,
    BOOKMARKS_SIDEBAR_COLUMN_ICON_VISIBLE,
    BOOKMARKS_SIDEBAR_COLUMN_BOOKMARK,
    BOOKMARKS_SIDEBAR_COLUMN_COUNT
};

static void  caja_bookmarks_sidebar_iface_init        (CajaSidebarIface         *iface);
static void  sidebar_provider_iface_init              (CajaSidebarProviderIface *iface);
static GType caja_bookmarks_sidebar_provider_get_type (void);
static void  caja_bookmarks_sidebar_style_updated     (GtkWidget *widget);

G_DEFINE_TYPE_WITH_CODE (CajaBookmarksSidebar, caja_bookmarks_sidebar, GTK_TYPE_SCROLLED_WINDOW,
                         G_IMPLEMENT_INTERFACE (CAJA_TYPE_SIDEBAR,
                                 caja_bookmarks_sidebar_iface_init));

G_DEFINE_TYPE_WITH_CODE (CajaBookmarksSidebarProvider, caja_bookmarks_sidebar_provider, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CAJA_TYPE_SIDEBAR_PROVIDER,
                                 sidebar_provider_iface_init));

static gboolean
is_built_in_bookmark (CajaFile *file)
{
    gboolean built_in;
    gint idx;

    built_in = FALSE;

    for (idx = 0; idx < G_USER_N_DIRECTORIES; idx++) {
        /* PUBLIC_SHARE and TEMPLATES are not in our built-in list */
        if (caja_file_is_user_special_directory (file, idx)) {
            if (idx != G_USER_DIRECTORY_PUBLIC_SHARE &&  idx != G_USER_DIRECTORY_TEMPLATES) {
                built_in = TRUE;
            }

            break;
        }
    }

    return built_in;
}

static void
update_bookmarks (CajaBookmarksSidebar *sidebar)
{
    GtkListStore         *store;
    GtkTreeSelection     *selection;
    GtkTreeIter           iter;
    CajaBookmark         *bookmark = NULL;
    int                   bookmark_count = 0;
    int                   index = 0;
    GFile                *root;
    CajaFile             *file;
    char                 *bookmark_uri;
    cairo_surface_t      *surface = NULL;
    char                 *name;
    gboolean              bookmarks_list_empty = TRUE;

    store = GTK_LIST_STORE (gtk_tree_view_get_model (sidebar->tree_view));

    gtk_list_store_clear (store);

    selection = GTK_TREE_SELECTION (gtk_tree_view_get_selection (sidebar->tree_view));

    bookmark_count = caja_bookmark_list_length (sidebar->bookmarks);

    for (index = 0; index < bookmark_count; index++)
    {
        bookmark = caja_bookmark_list_item_at (sidebar->bookmarks, index);

        if (caja_bookmark_uri_known_not_to_exist (bookmark)) {
            continue;
        }

        bookmark_uri = caja_bookmark_get_uri (bookmark);

        root = caja_bookmark_get_location (bookmark);
        file = caja_file_get (root);

        if (is_built_in_bookmark (file)) {
            g_object_unref (root);
            caja_file_unref (file);
            continue;
        }

        bookmarks_list_empty = FALSE;
        surface = caja_bookmark_get_surface (bookmark, GTK_ICON_SIZE_MENU);
        name = caja_bookmark_get_name (bookmark);
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            BOOKMARKS_SIDEBAR_COLUMN_ICON,         surface,
                            BOOKMARKS_SIDEBAR_COLUMN_NAME,         name,
                            BOOKMARKS_SIDEBAR_COLUMN_STYLE,        PANGO_STYLE_NORMAL,
                            BOOKMARKS_SIDEBAR_COLUMN_ICON_VISIBLE, TRUE,
                            BOOKMARKS_SIDEBAR_COLUMN_BOOKMARK,     bookmark,
                            -1);

        /* Select the bookmark if we're in the directory the bookmark points to. */
        if (g_strcmp0 (bookmark_uri, sidebar->current_uri) == 0)
            gtk_tree_selection_select_iter (selection, &iter);

        g_object_unref (root);
        caja_file_unref (file);
        g_free (bookmark_uri);

        if (surface != NULL)
        {
            cairo_surface_destroy (surface);
        }
        g_free (name);
    }

    /* If the user has no user-defined bookmarks, then add one row to the list
     * which says that no bookmarks have been defined, rather than leaving the
     * user scratching their head wondering why the list is blank.
     */
    if (bookmarks_list_empty)
    {
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            BOOKMARKS_SIDEBAR_COLUMN_ICON,         NULL,
                            BOOKMARKS_SIDEBAR_COLUMN_NAME,         _("No bookmarks defined"),
                            BOOKMARKS_SIDEBAR_COLUMN_STYLE,        PANGO_STYLE_ITALIC,
                            BOOKMARKS_SIDEBAR_COLUMN_ICON_VISIBLE, FALSE,
                            BOOKMARKS_SIDEBAR_COLUMN_BOOKMARK,     NULL,
                            -1);
        gtk_tree_selection_select_iter (selection, &iter);
    }
}

static void
open_selected_item (CajaBookmarksSidebar *sidebar,
                    GtkTreePath          *path,
                    CajaWindowOpenFlags   flags)
{
    CajaWindowSlotInfo *slot;
    GtkTreeModel *model;
    GtkTreeIter iter;
    CajaBookmark *bookmark;
    GFile *location;

    model = gtk_tree_view_get_model (sidebar->tree_view);

    if (!gtk_tree_model_get_iter (model, &iter, path))
        return;

    gtk_tree_model_get (model, &iter, BOOKMARKS_SIDEBAR_COLUMN_BOOKMARK,
                        &bookmark, -1);

    if (bookmark == NULL)
        return;

    /* Navigate to the clicked location. */
    location = caja_bookmark_get_location (CAJA_BOOKMARK (bookmark));

    slot = caja_window_info_get_active_slot (sidebar->window);

    caja_window_slot_info_open_location (slot, location,
                                         CAJA_WINDOW_OPEN_ACCORDING_TO_MODE,
                                         flags, NULL);

    g_object_unref (location);
}

static void
row_activated_callback (GtkTreeView       *tree_view,
                        GtkTreePath       *path,
                        GtkTreeViewColumn *column,
                        gpointer           user_data)
{
    CajaBookmarksSidebar *sidebar;

    sidebar = CAJA_BOOKMARKS_SIDEBAR (user_data);
    g_assert (sidebar->tree_view == tree_view);

    open_selected_item (sidebar, path, 0);
}

static gboolean
button_press_event_callback (GtkWidget      *widget,
                             GdkEventButton *event,
                             gpointer        user_data)
{
    CajaBookmarksSidebar *sidebar;
    GtkTreePath          *path;

    int                   open_flags = 0;

    if (event->type == GDK_BUTTON_PRESS)
    {
        /**
         * If the middle button was pressed, open the bookmark in a new tab.
         */
        if (event->button == GDK_BUTTON_MIDDLE)
            open_flags = CAJA_WINDOW_OPEN_FLAG_NEW_TAB;

        /**
         * If the middle button was not pressed, don't do
         * anything, since the action will be handled in
         * row_activated_callback() (above).
         */
        else
            return FALSE;
    }

    sidebar = CAJA_BOOKMARKS_SIDEBAR (user_data);
    g_assert (sidebar->tree_view == GTK_TREE_VIEW (widget));

    if (gtk_tree_view_get_path_at_pos (sidebar->tree_view,
                                       event->x, event->y,
                                       &path, NULL, NULL, NULL))
    {
        open_selected_item (sidebar,
                            path,
                            open_flags);

        gtk_tree_path_free (path);
    }

    /*
     * If the middle button was pressed, don't let the event bubble up to the
     * row_activated_callback() (above), because we do not want the row to
     * be selected:  The location of the bookmark was opened in a new tab, not
     * the current tab.
     */
    return TRUE;
}

static void
loading_uri_callback (CajaWindowInfo       *window,
                      char                 *location,
                      CajaBookmarksSidebar *sidebar)
{
    GtkTreeModel     *model;
    GtkTreeIter       iter;
    gboolean          valid;
    CajaBookmark     *bookmark;
    char             *uri;

    if (strcmp (sidebar->current_uri, location) != 0)
    {
        GtkTreeSelection *selection;

        g_free (sidebar->current_uri);
        sidebar->current_uri = g_strdup (location);

        /* set selection if any place matches location */
        selection = gtk_tree_view_get_selection (sidebar->tree_view);
        gtk_tree_selection_unselect_all (selection);

        model = gtk_tree_view_get_model (sidebar->tree_view);

        valid = gtk_tree_model_get_iter_first (model, &iter);

        while (valid)
        {
            gtk_tree_model_get (model, &iter,
                                BOOKMARKS_SIDEBAR_COLUMN_BOOKMARK, &bookmark,
                                -1);

            uri = caja_bookmark_get_uri (bookmark);

            if (uri != NULL)
            {
                if (strcmp (uri, location) == 0)
                {
                    g_free (uri);
                    gtk_tree_selection_select_iter (selection, &iter);
                    break;
                }
                g_free (uri);
            }
            valid = gtk_tree_model_iter_next (model, &iter);
        }
    }
}

/*
 * If no bookmarks have been defined by the user, then we create one row
 * in the bookmark list sidebar which says "No bookmarks defined".  We
 * do not want the user to be able to select the row, however, so if the
 * bookmark is NULL, then refuse selection; if the bookmark is not NULL,
 * allow selection.
*/
static gboolean
is_row_selectable (GtkTreeSelection *selection,
                   GtkTreeModel     *model,
                   GtkTreePath      *path,
                   gboolean          path_currently_selected,
                   gpointer          data)
{
    GtkTreeIter    iter;
    CajaBookmark  *bookmark;

    if (!gtk_tree_model_get_iter (model, &iter, path))
        return FALSE;

    gtk_tree_model_get (model, &iter, BOOKMARKS_SIDEBAR_COLUMN_BOOKMARK,
                        &bookmark, -1);

    if (bookmark == NULL)
        return FALSE;
    else
        return TRUE;
}

static void
caja_bookmarks_sidebar_init (CajaBookmarksSidebar *sidebar)
{
    GtkTreeView       *tree_view;
    GtkTreeViewColumn *col;
    GtkCellRenderer   *cell;
    GtkListStore      *store;
    GtkTreeSelection  *selection;

    tree_view = GTK_TREE_VIEW (gtk_tree_view_new ());
    gtk_tree_view_set_headers_visible (tree_view, FALSE);
    gtk_widget_show (GTK_WIDGET (tree_view));

    col = GTK_TREE_VIEW_COLUMN (gtk_tree_view_column_new ());

    cell = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (col, cell, FALSE);
    gtk_tree_view_column_set_attributes (col, cell,
                                         "surface", BOOKMARKS_SIDEBAR_COLUMN_ICON,
                                         "visible", BOOKMARKS_SIDEBAR_COLUMN_ICON_VISIBLE,
                                         NULL);

    cell = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (col, cell, TRUE);
    gtk_tree_view_column_set_attributes (col, cell,
                                         "text",      BOOKMARKS_SIDEBAR_COLUMN_NAME,
                                         "style",     BOOKMARKS_SIDEBAR_COLUMN_STYLE,
                                         NULL);

    gtk_tree_view_column_set_fixed_width (col, CAJA_ICON_SIZE_SMALLER);
    gtk_tree_view_append_column (tree_view, col);

    store = gtk_list_store_new (BOOKMARKS_SIDEBAR_COLUMN_COUNT,
                                CAIRO_GOBJECT_TYPE_SURFACE,
                                G_TYPE_STRING,
                                PANGO_TYPE_STYLE,
                                G_TYPE_BOOLEAN,
                                CAJA_TYPE_BOOKMARK);

    gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (store));
    g_object_unref (store);

    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sidebar),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);

    gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (sidebar), NULL);
    gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (sidebar), NULL);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sidebar), GTK_SHADOW_IN);
    gtk_scrolled_window_set_overlay_scrolling (GTK_SCROLLED_WINDOW (sidebar), FALSE);

    gtk_container_add (GTK_CONTAINER (sidebar), GTK_WIDGET (tree_view));
    gtk_widget_show (GTK_WIDGET (sidebar));

    sidebar->tree_view = tree_view;

    selection = gtk_tree_view_get_selection (tree_view);
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

    /* Please see the comment associated with the function is_row_selectable()
     * (above).
     */
    gtk_tree_selection_set_select_function (selection,
                                            is_row_selectable,
                                            NULL,
                                            NULL);

    g_signal_connect_object
    (tree_view, "row_activated",
     G_CALLBACK (row_activated_callback), sidebar, 0);

    g_signal_connect (tree_view, "button-press-event",
                      G_CALLBACK (button_press_event_callback), sidebar);

    eel_gtk_tree_view_set_activate_on_single_click (sidebar->tree_view, TRUE);
}

static void
caja_bookmarks_sidebar_finalize (GObject *object)
{
    CajaBookmarksSidebar *sidebar;

    sidebar = CAJA_BOOKMARKS_SIDEBAR (object);

    g_free (sidebar->current_uri);

    G_OBJECT_CLASS (caja_bookmarks_sidebar_parent_class)->finalize (object);
}

static void
caja_bookmarks_sidebar_class_init (CajaBookmarksSidebarClass *class)
{
    G_OBJECT_CLASS (class)->finalize = caja_bookmarks_sidebar_finalize;

    GTK_WIDGET_CLASS (class)->style_updated = caja_bookmarks_sidebar_style_updated;
}

static const char *
caja_bookmarks_sidebar_get_sidebar_id (CajaSidebar *sidebar)
{
    return CAJA_BOOKMARKS_SIDEBAR_ID;
}

static char *
caja_bookmarks_sidebar_get_tab_label (CajaSidebar *sidebar)
{
    return g_strdup (_("Bookmarks"));
}

static char *
caja_bookmarks_sidebar_get_tab_tooltip (CajaSidebar *sidebar)
{
    return g_strdup (_("Show a list of your personal bookmarks"));
}

static GdkPixbuf *
caja_bookmarks_sidebar_get_tab_icon (CajaSidebar *sidebar)
{
    return NULL;
}

static void
caja_bookmarks_sidebar_is_visible_changed (CajaSidebar     *sidebar,
                                           gboolean         is_visible)
{
    /* Do nothing */
}

static void
caja_bookmarks_sidebar_iface_init (CajaSidebarIface *iface)
{
    iface->get_sidebar_id = caja_bookmarks_sidebar_get_sidebar_id;
    iface->get_tab_label = caja_bookmarks_sidebar_get_tab_label;
    iface->get_tab_tooltip = caja_bookmarks_sidebar_get_tab_tooltip;
    iface->get_tab_icon = caja_bookmarks_sidebar_get_tab_icon;
    iface->is_visible_changed = caja_bookmarks_sidebar_is_visible_changed;
}

static void
caja_bookmarks_sidebar_set_parent_window (CajaBookmarksSidebar *sidebar,
                                          CajaWindowInfo       *window)
{
    CajaWindowSlotInfo *slot;

    sidebar->window = window;

    slot = caja_window_info_get_active_slot (window);

    sidebar->bookmarks = caja_bookmark_list_new ();
    sidebar->current_uri = caja_window_slot_info_get_current_location (slot);

    g_signal_connect_object (sidebar->bookmarks, "contents_changed",
                             G_CALLBACK (update_bookmarks),
                             sidebar, G_CONNECT_SWAPPED);

    g_signal_connect_object (window, "loading_uri",
                             G_CALLBACK (loading_uri_callback),
                             sidebar, 0);

    update_bookmarks (sidebar);
}

static void
caja_bookmarks_sidebar_style_updated (GtkWidget *widget)
{
    CajaBookmarksSidebar *sidebar;

    sidebar = CAJA_BOOKMARKS_SIDEBAR (widget);

    update_bookmarks (sidebar);
}

static CajaSidebar *
caja_bookmarks_sidebar_create (CajaSidebarProvider *provider,
                               CajaWindowInfo      *window)
{
    CajaBookmarksSidebar *sidebar;

    sidebar = g_object_new (caja_bookmarks_sidebar_get_type (), NULL);
    caja_bookmarks_sidebar_set_parent_window (sidebar, window);
    g_object_ref_sink (sidebar);

    return CAJA_SIDEBAR (sidebar);
}

static void
sidebar_provider_iface_init (CajaSidebarProviderIface *iface)
{
    iface->create = caja_bookmarks_sidebar_create;
}

static void
caja_bookmarks_sidebar_provider_init (CajaBookmarksSidebarProvider *sidebar)
{
}

static void
caja_bookmarks_sidebar_provider_class_init (CajaBookmarksSidebarProviderClass *class)
{
}

void
caja_bookmarks_sidebar_register (void)
{
    caja_module_add_type (caja_bookmarks_sidebar_provider_get_type ());
}
