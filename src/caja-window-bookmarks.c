/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Caja
 *
 * Copyright (C) 2000, 2001 Eazel, Inc.
 * Copyright (C) 2005 Red Hat, Inc.
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
 * Author: John Sullivan <sullivan@eazel.com>
 *         Alexander Larsson <alexl@redhat.com>
 */

#include <config.h>

#include <locale.h>

#include "caja-actions.h"
#include "caja-bookmark-list.h"
#include "caja-bookmarks-window.h"
#include "caja-window-bookmarks.h"
#include "caja-window-private.h"
#include <eel/eel-debug.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <glib/gi18n.h>

#define MENU_ITEM_MAX_WIDTH_CHARS 32

static GtkWindow *bookmarks_window = NULL;

static void refresh_bookmarks_menu (CajaWindow *window);

static void
remove_bookmarks_for_uri_if_yes (GtkDialog *dialog, int response, gpointer callback_data)
{
    const char *uri;
    CajaWindow *window;

    g_assert (GTK_IS_DIALOG (dialog));
    g_assert (callback_data != NULL);

    window = callback_data;

    if (response == GTK_RESPONSE_YES)
    {
        uri = g_object_get_data (G_OBJECT (dialog), "uri");
        caja_bookmark_list_delete_items_with_uri (window->details->bookmark_list, uri);
    }

    gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
show_bogus_bookmark_window (CajaWindow *window,
                            CajaBookmark *bookmark)
{
    GtkDialog *dialog;
    GFile *location;
    char *uri_for_display;
    char *prompt;
    char *detail;

    location = caja_bookmark_get_location (bookmark);
    uri_for_display = g_file_get_parse_name (location);

    prompt = _("Do you want to remove any bookmarks with the "
               "non-existing location from your list?");
    detail = g_strdup_printf (_("The location \"%s\" does not exist."), uri_for_display);

    dialog = eel_show_yes_no_dialog (prompt, detail,
                                     _("Bookmark for Nonexistent Location"),
                                     GTK_STOCK_CANCEL,
                                     GTK_WINDOW (window));

    g_signal_connect (dialog, "response",
                      G_CALLBACK (remove_bookmarks_for_uri_if_yes), window);
    g_object_set_data_full (G_OBJECT (dialog), "uri", g_file_get_uri (location), g_free);

    gtk_dialog_set_default_response (dialog, GTK_RESPONSE_NO);

    g_object_unref (location);
    g_free (uri_for_display);
    g_free (detail);
}

static GtkWindow *
get_or_create_bookmarks_window (CajaWindow *window)
{
    if (bookmarks_window == NULL)
    {
        bookmarks_window = create_bookmarks_window (window->details->bookmark_list,
                           window);
    }
    else
    {
        edit_bookmarks_dialog_set_signals (window);
    }

    return bookmarks_window;
}

/**
 * caja_bookmarks_exiting:
 *
 * Last chance to save state before app exits.
 * Called when application exits; don't call from anywhere else.
 **/
void
caja_bookmarks_exiting (void)
{
    if (bookmarks_window != NULL)
    {
        caja_bookmarks_window_save_geometry (bookmarks_window);
        gtk_widget_destroy (GTK_WIDGET (bookmarks_window));
    }
}

/**
 * add_bookmark_for_current_location
 *
 * Add a bookmark for the displayed location to the bookmarks menu.
 * Does nothing if there's already a bookmark for the displayed location.
 */
void
caja_window_add_bookmark_for_current_location (CajaWindow *window)
{
    CajaBookmark *bookmark;
    CajaWindowSlot *slot;
    CajaBookmarkList *list;

    g_assert (CAJA_IS_WINDOW (window));

    slot = window->details->active_pane->active_slot;
    bookmark = slot->current_location_bookmark;
    list = window->details->bookmark_list;

    if (!caja_bookmark_list_contains (list, bookmark))
    {
        caja_bookmark_list_append (list, bookmark);
    }
}

void
caja_window_edit_bookmarks (CajaWindow *window)
{
    GtkWindow *dialog;

    dialog = get_or_create_bookmarks_window (window);

    gtk_window_set_screen (
        dialog, gtk_window_get_screen (GTK_WINDOW (window)));
    gtk_window_present (dialog);
}

static void
remove_bookmarks_menu_items (CajaWindow *window)
{
    GtkUIManager *ui_manager;

    ui_manager = caja_window_get_ui_manager (window);
    if (window->details->bookmarks_merge_id != 0)
    {
        gtk_ui_manager_remove_ui (ui_manager,
                                  window->details->bookmarks_merge_id);
        window->details->bookmarks_merge_id = 0;
    }
    if (window->details->bookmarks_action_group != NULL)
    {
        gtk_ui_manager_remove_action_group (ui_manager,
                                            window->details->bookmarks_action_group);
        window->details->bookmarks_action_group = NULL;
    }
}

static void
connect_proxy_cb (GtkActionGroup *action_group,
                  GtkAction *action,
                  GtkWidget *proxy,
                  gpointer dummy)
{
    GtkLabel *label;

    if (!GTK_IS_MENU_ITEM (proxy))
        return;

    label = GTK_LABEL (gtk_bin_get_child (GTK_BIN (proxy)));

    gtk_label_set_use_underline (label, FALSE);
    gtk_label_set_ellipsize (label, PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars (label, MENU_ITEM_MAX_WIDTH_CHARS);
}

static void
update_bookmarks (CajaWindow *window)
{
    CajaBookmarkList *bookmarks;
    CajaBookmark *bookmark;
    guint bookmark_count;
    guint index;
    GtkUIManager *ui_manager;

    g_assert (CAJA_IS_WINDOW (window));
    g_assert (window->details->bookmarks_merge_id == 0);
    g_assert (window->details->bookmarks_action_group == NULL);

    if (window->details->bookmark_list == NULL)
    {
        window->details->bookmark_list = caja_bookmark_list_new ();
    }

    bookmarks = window->details->bookmark_list;

    ui_manager = caja_window_get_ui_manager (CAJA_WINDOW (window));

    window->details->bookmarks_merge_id = gtk_ui_manager_new_merge_id (ui_manager);
    window->details->bookmarks_action_group = gtk_action_group_new ("BookmarksGroup");
    g_signal_connect (window->details->bookmarks_action_group, "connect-proxy",
                      G_CALLBACK (connect_proxy_cb), NULL);

    gtk_ui_manager_insert_action_group (ui_manager,
                                        window->details->bookmarks_action_group,
                                        -1);
    g_object_unref (window->details->bookmarks_action_group);

    /* append new set of bookmarks */
    bookmark_count = caja_bookmark_list_length (bookmarks);
    for (index = 0; index < bookmark_count; ++index)
    {
        bookmark = caja_bookmark_list_item_at (bookmarks, index);

        if (caja_bookmark_uri_known_not_to_exist (bookmark))
        {
            continue;
        }

        caja_menus_append_bookmark_to_menu
        (CAJA_WINDOW (window),
         bookmark,
         CAJA_WINDOW_GET_CLASS (window)->bookmarks_placeholder,
         "dynamic",
         index,
         window->details->bookmarks_action_group,
         window->details->bookmarks_merge_id,
         G_CALLBACK (refresh_bookmarks_menu),
         show_bogus_bookmark_window);
    }
}

static void
refresh_bookmarks_menu (CajaWindow *window)
{
    g_assert (CAJA_IS_WINDOW (window));

    remove_bookmarks_menu_items (window);
    update_bookmarks (window);
}

/**
 * caja_window_initialize_bookmarks_menu
 *
 * Fill in bookmarks menu with stored bookmarks, and wire up signals
 * so we'll be notified when bookmark list changes.
 */
void
caja_window_initialize_bookmarks_menu (CajaWindow *window)
{
    g_assert (CAJA_IS_WINDOW (window));

    refresh_bookmarks_menu (window);

    /* Recreate dynamic part of menu if bookmark list changes */
    g_signal_connect_object (window->details->bookmark_list, "contents_changed",
                             G_CALLBACK (refresh_bookmarks_menu),
                             window, G_CONNECT_SWAPPED);
}
