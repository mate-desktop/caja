/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-navigation-window-slot.c: Caja navigation window slot

   Copyright (C) 2008 Free Software Foundation, Inc.

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

   Author: Christian Neumair <cneumair@gnome.org>
*/

#include <eel/eel-gtk-macros.h>

#include <libcaja-private/caja-window-slot-info.h>
#include <libcaja-private/caja-file.h>

#include "caja-window-slot.h"
#include "caja-navigation-window-slot.h"
#include "caja-window-private.h"
#include "caja-search-bar.h"
#include "caja-navigation-window-pane.h"

G_DEFINE_TYPE (CajaNavigationWindowSlot, caja_navigation_window_slot, CAJA_TYPE_WINDOW_SLOT)

#define parent_class caja_navigation_window_slot_parent_class

gboolean
caja_navigation_window_slot_should_close_with_mount (CajaNavigationWindowSlot *slot,
        GMount *mount)
{
    CajaBookmark *bookmark;
    GFile *mount_location, *bookmark_location;
    GList *l;
    gboolean close_with_mount;

    if (slot->parent.pane->window->details->initiated_unmount)
    {
        return FALSE;
    }

    mount_location = g_mount_get_root (mount);

    close_with_mount = TRUE;

    for (l = slot->back_list; l != NULL; l = l->next)
    {
        bookmark = CAJA_BOOKMARK (l->data);

        bookmark_location = caja_bookmark_get_location (bookmark);
        close_with_mount &= g_file_has_prefix (bookmark_location, mount_location) ||
                            g_file_equal (bookmark_location, mount_location);
        g_object_unref (bookmark_location);

        if (!close_with_mount)
        {
            break;
        }
    }

    close_with_mount &= g_file_has_prefix (CAJA_WINDOW_SLOT (slot)->location, mount_location) ||
                        g_file_equal (CAJA_WINDOW_SLOT (slot)->location, mount_location);

    /* we could also consider the forward list here, but since the “go home” request
     * in caja-window-manager-views.c:mount_removed_callback() would discard those
     * anyway, we don't consider them.
     */

    g_object_unref (mount_location);

    return close_with_mount;
}

void
caja_navigation_window_slot_clear_forward_list (CajaNavigationWindowSlot *slot)
{
    g_assert (CAJA_IS_NAVIGATION_WINDOW_SLOT (slot));

    g_list_free_full (slot->forward_list, g_object_unref);
    slot->forward_list = NULL;
}

void
caja_navigation_window_slot_clear_back_list (CajaNavigationWindowSlot *slot)
{
    g_assert (CAJA_IS_NAVIGATION_WINDOW_SLOT (slot));

    g_list_free_full (slot->back_list, g_object_unref);
    slot->back_list = NULL;
}

static void
query_editor_changed_callback (CajaSearchBar *bar,
                               CajaQuery *query,
                               gboolean reload,
                               CajaWindowSlot *slot)
{
    CajaDirectory *directory;

    g_assert (CAJA_IS_FILE (slot->viewed_file));

    directory = caja_directory_get_for_file (slot->viewed_file);
    g_assert (CAJA_IS_SEARCH_DIRECTORY (directory));

    caja_search_directory_set_query (CAJA_SEARCH_DIRECTORY (directory),
                                     query);
    if (reload)
    {
        caja_window_slot_reload (slot);
    }

    caja_directory_unref (directory);
}


static void
caja_navigation_window_slot_update_query_editor (CajaWindowSlot *slot)
{
    CajaDirectory *directory;
    CajaSearchDirectory *search_directory;
    GtkWidget *query_editor;

    g_assert (slot->pane->window != NULL);

    query_editor = NULL;

    directory = caja_directory_get (slot->location);
    if (CAJA_IS_SEARCH_DIRECTORY (directory))
    {
        search_directory = CAJA_SEARCH_DIRECTORY (directory);

        if (caja_search_directory_is_saved_search (search_directory))
        {
            query_editor = caja_query_editor_new (TRUE,
                                                  caja_search_directory_is_indexed (search_directory));
        }
        else
        {
            query_editor = caja_query_editor_new_with_bar (FALSE,
                           caja_search_directory_is_indexed (search_directory),
                           slot->pane->window->details->active_pane->active_slot == slot,
                           CAJA_SEARCH_BAR (CAJA_NAVIGATION_WINDOW_PANE (slot->pane)->search_bar),
                           slot);
        }
    }

    slot->query_editor = CAJA_QUERY_EDITOR (query_editor);

    if (query_editor != NULL)
    {
        CajaQuery *query;

        g_signal_connect_object (query_editor, "changed",
                                 G_CALLBACK (query_editor_changed_callback), slot, 0);

        query = caja_search_directory_get_query (search_directory);
        if (query != NULL)
        {
            caja_query_editor_set_query (CAJA_QUERY_EDITOR (query_editor),
                                         query);
            g_object_unref (query);
        }
        else
        {
            caja_query_editor_set_default_query (CAJA_QUERY_EDITOR (query_editor));
        }

        caja_window_slot_add_extra_location_widget (slot, query_editor);
        gtk_widget_show (query_editor);
        caja_query_editor_grab_focus (CAJA_QUERY_EDITOR (query_editor));
    }

    caja_directory_unref (directory);
}

static void
caja_navigation_window_slot_active (CajaWindowSlot *slot)
{
    CajaNavigationWindow *window;
    CajaNavigationWindowPane *pane;
    int page_num;

    pane = CAJA_NAVIGATION_WINDOW_PANE (slot->pane);
    window = CAJA_NAVIGATION_WINDOW (slot->pane->window);

    page_num = gtk_notebook_page_num (GTK_NOTEBOOK (pane->notebook),
                                      slot->content_box);
    g_assert (page_num >= 0);

    gtk_notebook_set_current_page (GTK_NOTEBOOK (pane->notebook), page_num);

    EEL_CALL_PARENT (CAJA_WINDOW_SLOT_CLASS, active, (slot));

    if (slot->viewed_file != NULL)
    {
        caja_navigation_window_load_extension_toolbar_items (window);
    }
}

static void
caja_navigation_window_slot_dispose (GObject *object)
{
    CajaNavigationWindowSlot *slot;

    slot = CAJA_NAVIGATION_WINDOW_SLOT (object);

    caja_navigation_window_slot_clear_forward_list (slot);
    caja_navigation_window_slot_clear_back_list (slot);

    G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
caja_navigation_window_slot_init (CajaNavigationWindowSlot *slot)
{
}

static void
caja_navigation_window_slot_class_init (CajaNavigationWindowSlotClass *class)
{
    CAJA_WINDOW_SLOT_CLASS (class)->active = caja_navigation_window_slot_active;
    CAJA_WINDOW_SLOT_CLASS (class)->update_query_editor = caja_navigation_window_slot_update_query_editor;

    G_OBJECT_CLASS (class)->dispose = caja_navigation_window_slot_dispose;
}

