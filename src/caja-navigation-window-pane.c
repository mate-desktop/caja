/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-navigation-window-pane.c: Caja navigation window pane

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

   Author: Holger Berndt <berndth@gmx.de>
*/

#include <eel/eel-gtk-extensions.h>

#include <libcaja-private/caja-global-preferences.h>
#include <libcaja-private/caja-window-slot-info.h>
#include <libcaja-private/caja-view-factory.h>
#include <libcaja-private/caja-entry.h>

#include "caja-navigation-window-pane.h"
#include "caja-window-private.h"
#include "caja-window-manage-views.h"
#include "caja-pathbar.h"
#include "caja-location-bar.h"
#include "caja-notebook.h"
#include "caja-window-slot.h"

static void caja_navigation_window_pane_dispose    (GObject *object);

G_DEFINE_TYPE (CajaNavigationWindowPane,
               caja_navigation_window_pane,
               CAJA_TYPE_WINDOW_PANE)
#define parent_class caja_navigation_window_pane_parent_class

static void
real_set_active (CajaWindowPane *pane, gboolean is_active)
{
    CajaNavigationWindowPane *nav_pane;
    GList *l;

    nav_pane = CAJA_NAVIGATION_WINDOW_PANE (pane);

    /* path bar */
    for (l = CAJA_PATH_BAR (nav_pane->path_bar)->button_list; l; l = l->next)
    {
        gtk_widget_set_sensitive (gtk_bin_get_child (GTK_BIN (caja_path_bar_get_button_from_button_list_entry (l->data))), is_active);
    }

    /* navigation bar (manual entry) */
    caja_location_bar_set_active (CAJA_LOCATION_BAR (nav_pane->navigation_bar), is_active);

    /* location button */
    gtk_widget_set_sensitive (gtk_bin_get_child (GTK_BIN (nav_pane->location_button)), is_active);
}

static gboolean
navigation_bar_focus_in_callback (GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
{
    CajaWindowPane *pane;
    pane = CAJA_WINDOW_PANE (user_data);
    caja_window_set_active_pane (pane->window, pane);
    return FALSE;
}

static int
bookmark_list_get_uri_index (GList *list, GFile *location)
{
    GList *l;
    int i;
    CajaBookmark *bookmark = NULL;
    GFile *tmp = NULL;

    g_return_val_if_fail (location != NULL, -1);

    for (i = 0, l = list; l != NULL; i++, l = l->next)
    {
        bookmark = CAJA_BOOKMARK (l->data);

        tmp = caja_bookmark_get_location (bookmark);
        if (g_file_equal (location, tmp))
        {
            g_object_unref (tmp);
            return i;
        }
        g_object_unref (tmp);
    }

    return -1;
}

static void
search_bar_focus_in_callback (CajaSearchBar *bar,
                              CajaWindowPane *pane)
{
    caja_window_set_active_pane (pane->window, pane);
}


static void
search_bar_activate_callback (CajaSearchBar *bar,
                              CajaNavigationWindowPane *pane)
{
    char *uri;
    CajaDirectory *directory;
    CajaSearchDirectory *search_directory;
    CajaQuery *query;
    GFile *location;

    uri = caja_search_directory_generate_new_uri ();
    location = g_file_new_for_uri (uri);
    g_free (uri);

    directory = caja_directory_get (location);

    g_assert (CAJA_IS_SEARCH_DIRECTORY (directory));

    search_directory = CAJA_SEARCH_DIRECTORY (directory);

    query = caja_search_bar_get_query (CAJA_SEARCH_BAR (pane->search_bar));
    if (query != NULL)
    {
        CajaWindowSlot *slot = CAJA_WINDOW_PANE (pane)->active_slot;
        if (!caja_search_directory_is_indexed (search_directory))
        {
            char *current_uri;

            current_uri = caja_window_slot_get_location_uri (slot);
            caja_query_set_location (query, current_uri);
            g_free (current_uri);
        }
        caja_search_directory_set_query (search_directory, query);
        g_object_unref (query);
    }

    caja_window_slot_go_to (CAJA_WINDOW_PANE (pane)->active_slot, location, FALSE);

    caja_directory_unref (directory);
    g_object_unref (location);
}

static void
search_bar_cancel_callback (GtkWidget *widget,
                            CajaNavigationWindowPane *pane)
{
    if (caja_navigation_window_pane_hide_temporary_bars (pane))
    {
        caja_navigation_window_restore_focus_widget (CAJA_NAVIGATION_WINDOW (CAJA_WINDOW_PANE (pane)->window));
    }
}

static void
navigation_bar_cancel_callback (GtkWidget *widget,
                                CajaNavigationWindowPane *pane)
{
    if (caja_navigation_window_pane_hide_temporary_bars (pane))
    {
        caja_navigation_window_restore_focus_widget (CAJA_NAVIGATION_WINDOW (CAJA_WINDOW_PANE (pane)->window));
    }
}

static void
navigation_bar_location_changed_callback (GtkWidget *widget,
        const char *uri,
        CajaNavigationWindowPane *pane)
{
    GFile *location;

    if (caja_navigation_window_pane_hide_temporary_bars (pane))
    {
        caja_navigation_window_restore_focus_widget (CAJA_NAVIGATION_WINDOW (CAJA_WINDOW_PANE (pane)->window));
    }

    location = g_file_new_for_uri (uri);
    caja_window_slot_go_to (CAJA_WINDOW_PANE (pane)->active_slot, location, FALSE);
    g_object_unref (location);
}

static void
path_bar_location_changed_callback (GtkWidget *widget,
                                    GFile *location,
                                    CajaNavigationWindowPane *pane)
{
    CajaNavigationWindowSlot *slot;
    CajaWindowPane *win_pane;
    int i;

    g_assert (CAJA_IS_NAVIGATION_WINDOW_PANE (pane));

    win_pane = CAJA_WINDOW_PANE(pane);

    slot = CAJA_NAVIGATION_WINDOW_SLOT (win_pane->active_slot);

    /* Make sure we are changing the location on the correct pane */
    caja_window_set_active_pane (CAJA_WINDOW_PANE (pane)->window, CAJA_WINDOW_PANE (pane));

    /* check whether we already visited the target location */
    i = bookmark_list_get_uri_index (slot->back_list, location);
    if (i >= 0)
    {
        caja_navigation_window_back_or_forward (CAJA_NAVIGATION_WINDOW (win_pane->window), TRUE, i, FALSE);
    }
    else
    {
        caja_window_slot_go_to (win_pane->active_slot, location, FALSE);
    }
}

static gboolean
location_button_should_be_active (CajaNavigationWindowPane *pane)
{
    return g_settings_get_boolean (caja_preferences, CAJA_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY);
}

static void
location_button_toggled_cb (GtkToggleButton *toggle,
                            CajaNavigationWindowPane *pane)
{
    gboolean is_active;

    is_active = gtk_toggle_button_get_active (toggle);
    g_settings_set_boolean (caja_preferences, CAJA_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY, is_active);

    if (is_active) {
        caja_location_bar_activate (CAJA_LOCATION_BAR (pane->navigation_bar));
    }

    caja_window_set_active_pane (CAJA_WINDOW_PANE (pane)->window, CAJA_WINDOW_PANE (pane));
}

static GtkWidget *
location_button_create (CajaNavigationWindowPane *pane)
{
    GtkWidget *image;
    GtkWidget *button;

    image = gtk_image_new_from_icon_name ("gtk-edit", GTK_ICON_SIZE_MENU);
    gtk_widget_show (image);

    button = g_object_new (GTK_TYPE_TOGGLE_BUTTON,
                   "image", image,
                   "focus-on-click", FALSE,
                   "active", location_button_should_be_active (pane),
                   NULL);

    gtk_widget_set_tooltip_text (button,
                     _("Toggle between button and text-based location bar"));

    g_signal_connect (button, "toggled",
              G_CALLBACK (location_button_toggled_cb), pane);
    return button;
}

static gboolean
path_bar_path_event_callback (CajaPathBar *path_bar,
                   GFile *location,
                   GdkEventButton *event,
                   CajaWindowPane *pane)

{
    CajaWindowSlot *slot;
    CajaWindowOpenFlags flags;

    if (event->type == GDK_BUTTON_RELEASE) {
        int mask;

        mask = event->state & gtk_accelerator_get_default_mod_mask ();
        flags = 0;

        if (event->button == 2 && mask == 0)
        {
            flags = CAJA_WINDOW_OPEN_FLAG_NEW_TAB;
        }
        else if (event->button == 1 && mask == GDK_CONTROL_MASK)
        {
            flags = CAJA_WINDOW_OPEN_FLAG_NEW_WINDOW;
        }

        if (flags != 0)
        {
            slot = caja_window_get_active_slot (CAJA_WINDOW_PANE (pane)->window);
            caja_window_slot_info_open_location (slot, location,
                                                 CAJA_WINDOW_OPEN_ACCORDING_TO_MODE,
                                                 flags, NULL);
        }

         return FALSE;
    }

    if (event->button == 3) {
        CajaView *view;

        slot = caja_window_get_active_slot (pane->window);
        view = slot->content_view;

        if (view != NULL) {
            char *uri;

            uri = g_file_get_uri (location);
            caja_view_pop_up_location_context_menu (view, event, uri);
            g_free (uri);
        }
        return TRUE;
    }
    return FALSE;
}

static void
notebook_popup_menu_new_tab_cb (GtkMenuItem *menuitem,
    			gpointer user_data)
{
    CajaWindowPane *pane;

    pane = CAJA_WINDOW_PANE (user_data);
    caja_window_new_tab (pane->window);
}

static void
notebook_popup_menu_move_left_cb (GtkMenuItem *menuitem,
                                  gpointer user_data)
{
    CajaNavigationWindowPane *pane;

    pane = CAJA_NAVIGATION_WINDOW_PANE (user_data);
    caja_notebook_reorder_current_child_relative (CAJA_NOTEBOOK (pane->notebook), -1);
}

static void
notebook_popup_menu_move_right_cb (GtkMenuItem *menuitem,
                                   gpointer user_data)
{
    CajaNavigationWindowPane *pane;

    pane = CAJA_NAVIGATION_WINDOW_PANE (user_data);
    caja_notebook_reorder_current_child_relative (CAJA_NOTEBOOK (pane->notebook), 1);
}

static void
notebook_popup_menu_close_cb (GtkMenuItem *menuitem,
                              gpointer user_data)
{
    CajaWindowPane *pane;
    CajaWindowSlot *slot;

    pane = CAJA_WINDOW_PANE (user_data);
    slot = pane->active_slot;
    caja_window_slot_close (slot);
}

static void
notebook_popup_menu_show (CajaNavigationWindowPane *pane,
                          GdkEventButton *event)
{
    GtkWidget *popup;
    GtkWidget *item;
    gboolean can_move_left, can_move_right;
    CajaNotebook *notebook;

    notebook = CAJA_NOTEBOOK (pane->notebook);

    can_move_left = caja_notebook_can_reorder_current_child_relative (notebook, -1);
    can_move_right = caja_notebook_can_reorder_current_child_relative (notebook, 1);

    popup = gtk_menu_new();

    gtk_menu_set_reserve_toggle_size (GTK_MENU (popup), FALSE);

    item = eel_image_menu_item_new_from_icon (NULL, _("_New Tab"));
    g_signal_connect (item, "activate",
    		  G_CALLBACK (notebook_popup_menu_new_tab_cb),
    		  pane);
    gtk_menu_shell_append (GTK_MENU_SHELL (popup),
    		       item);

    gtk_menu_shell_append (GTK_MENU_SHELL (popup),
    		       gtk_separator_menu_item_new ());

    item = eel_image_menu_item_new_from_icon (NULL, _("Move Tab _Left"));
    g_signal_connect (item, "activate",
                      G_CALLBACK (notebook_popup_menu_move_left_cb),
                      pane);
    gtk_menu_shell_append (GTK_MENU_SHELL (popup),
                           item);
    gtk_widget_set_sensitive (item, can_move_left);

    item = eel_image_menu_item_new_from_icon (NULL, _("Move Tab _Right"));
    g_signal_connect (item, "activate",
                      G_CALLBACK (notebook_popup_menu_move_right_cb),
                      pane);
    gtk_menu_shell_append (GTK_MENU_SHELL (popup),
                           item);
    gtk_widget_set_sensitive (item, can_move_right);

    gtk_menu_shell_append (GTK_MENU_SHELL (popup),
                           gtk_separator_menu_item_new ());

    item = eel_image_menu_item_new_from_icon ("window-close", _("_Close Tab"));

    g_signal_connect (item, "activate",
                      G_CALLBACK (notebook_popup_menu_close_cb), pane);
    gtk_menu_shell_append (GTK_MENU_SHELL (popup),
                           item);

    gtk_widget_show_all (popup);

    /* TODO is this correct? */
    gtk_menu_attach_to_widget (GTK_MENU (popup),
                               pane->notebook,
                               NULL);

    gtk_menu_popup_at_pointer (GTK_MENU (popup),
                               (const GdkEvent*) event);
}

/* emitted when the user clicks the "close" button of tabs */
static void
notebook_tab_close_requested (CajaNotebook *notebook,
                              CajaWindowSlot *slot,
                              CajaWindowPane *pane)
{
    caja_window_pane_slot_close (pane, slot);
}

static gboolean
notebook_button_press_cb (GtkWidget *widget,
                          GdkEventButton *event,
                          gpointer user_data)
{
    CajaNavigationWindowPane *pane;

    pane = CAJA_NAVIGATION_WINDOW_PANE (user_data);
    if (GDK_BUTTON_PRESS == event->type && 3 == event->button)
    {
        notebook_popup_menu_show (pane, event);
        return TRUE;
    }
    else if (GDK_BUTTON_PRESS == event->type && 2 == event->button)
    {
        CajaWindowPane *wpane;
        CajaWindowSlot *slot;

        wpane = CAJA_WINDOW_PANE (pane);
        slot = wpane->active_slot;
        caja_window_slot_close (slot);

        return FALSE;
    }

    return FALSE;
}

static gboolean
notebook_popup_menu_cb (GtkWidget *widget,
                        gpointer user_data)
{
    CajaNavigationWindowPane *pane;

    pane = CAJA_NAVIGATION_WINDOW_PANE (user_data);
    notebook_popup_menu_show (pane, NULL);
    return TRUE;
}

static gboolean
notebook_switch_page_cb (GtkNotebook *notebook,
                         GtkWidget *page,
                         unsigned int page_num,
                         CajaNavigationWindowPane *pane)
{
    CajaWindowSlot *slot;
    GtkWidget *widget;

    widget = gtk_notebook_get_nth_page (GTK_NOTEBOOK (pane->notebook), page_num);
    g_assert (widget != NULL);

    /* find slot corresponding to the target page */
    slot = caja_window_pane_get_slot_for_content_box (CAJA_WINDOW_PANE (pane), widget);
    g_assert (slot != NULL);

    caja_window_set_active_slot (slot->pane->window, slot);

    caja_window_slot_update_icon (slot);

    return FALSE;
}

void
caja_navigation_window_pane_remove_page (CajaNavigationWindowPane *pane, int page_num)
{
    GtkNotebook *notebook;
    notebook = GTK_NOTEBOOK (pane->notebook);

    g_signal_handlers_block_by_func (notebook,
                                     G_CALLBACK (notebook_switch_page_cb),
                                     pane);
    gtk_notebook_remove_page (notebook, page_num);
    g_signal_handlers_unblock_by_func (notebook,
                                       G_CALLBACK (notebook_switch_page_cb),
                                       pane);
}

void
caja_navigation_window_pane_add_slot_in_tab (CajaNavigationWindowPane *pane, CajaWindowSlot *slot, CajaWindowOpenSlotFlags flags)
{
    CajaNotebook *notebook;

    notebook = CAJA_NOTEBOOK (pane->notebook);
    g_signal_handlers_block_by_func (notebook,
                                     G_CALLBACK (notebook_switch_page_cb),
                                     pane);
    caja_notebook_add_tab (notebook,
                           slot,
                           (flags & CAJA_WINDOW_OPEN_SLOT_APPEND) != 0 ?
                           -1 :
                           gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook)) + 1,
                           FALSE);
    g_signal_handlers_unblock_by_func (notebook,
                                       G_CALLBACK (notebook_switch_page_cb),
                                       pane);
}

static void
real_sync_location_widgets (CajaWindowPane *pane)
{
    CajaNavigationWindowPane *navigation_pane;
    CajaWindowSlot *slot;

    slot = pane->active_slot;
    navigation_pane = CAJA_NAVIGATION_WINDOW_PANE (pane);

    /* Change the location bar and path bar to match the current location. */
    if (slot->location != NULL)
    {
        char *uri;

        /* this may be NULL if we just created the slot */
        uri = caja_window_slot_get_location_uri (slot);
        caja_location_bar_set_location (CAJA_LOCATION_BAR (navigation_pane->navigation_bar), uri);
        g_free (uri);
        caja_path_bar_set_path (CAJA_PATH_BAR (navigation_pane->path_bar), slot->location);
    }

    /* Update window global UI if this is the active pane */
    if (pane == pane->window->details->active_pane)
    {
        CajaNavigationWindowSlot *navigation_slot;

        caja_window_update_up_button (pane->window);

        /* Check if the back and forward buttons need enabling or disabling. */
        navigation_slot = CAJA_NAVIGATION_WINDOW_SLOT (pane->window->details->active_pane->active_slot);
        caja_navigation_window_allow_back (CAJA_NAVIGATION_WINDOW (pane->window),
                                           navigation_slot->back_list != NULL);
        caja_navigation_window_allow_forward (CAJA_NAVIGATION_WINDOW (pane->window),
                                              navigation_slot->forward_list != NULL);
    }
}

gboolean
caja_navigation_window_pane_hide_temporary_bars (CajaNavigationWindowPane *pane)
{
    CajaWindowSlot *slot;
    gboolean success;

    g_assert (CAJA_IS_NAVIGATION_WINDOW_PANE (pane));

    slot = CAJA_WINDOW_PANE(pane)->active_slot;
    success = FALSE;

    if (pane->temporary_location_bar)
    {
        if (caja_navigation_window_pane_location_bar_showing (pane))
        {
            caja_navigation_window_pane_hide_location_bar (pane, FALSE);
        }
        pane->temporary_location_bar = FALSE;
        success = TRUE;
    }
    if (pane->temporary_navigation_bar)
    {
        CajaDirectory *directory;

        directory = caja_directory_get (slot->location);

        if (CAJA_IS_SEARCH_DIRECTORY (directory))
        {
            caja_navigation_window_pane_set_bar_mode (pane, CAJA_BAR_SEARCH);
        }
        else
        {
            if (!g_settings_get_boolean (caja_preferences, CAJA_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY))
            {
                caja_navigation_window_pane_set_bar_mode (pane, CAJA_BAR_PATH);
            }
        }
        pane->temporary_navigation_bar = FALSE;
        success = TRUE;

        caja_directory_unref (directory);
    }
    if (pane->temporary_search_bar)
    {
        CajaNavigationWindow *window;

        if (!g_settings_get_boolean (caja_preferences, CAJA_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY))
        {
            caja_navigation_window_pane_set_bar_mode (pane, CAJA_BAR_PATH);
        }
        else
        {
            caja_navigation_window_pane_set_bar_mode (pane, CAJA_BAR_NAVIGATION);
        }
        window = CAJA_NAVIGATION_WINDOW (CAJA_WINDOW_PANE (pane)->window);
        caja_navigation_window_set_search_button (window, FALSE);
        pane->temporary_search_bar = FALSE;
        success = TRUE;
    }

    return success;
}

void
caja_navigation_window_pane_always_use_location_entry (CajaNavigationWindowPane *pane, gboolean use_entry)
{
    if (use_entry)
    {
        caja_navigation_window_pane_set_bar_mode (pane, CAJA_BAR_NAVIGATION);
    }
    else
    {
        caja_navigation_window_pane_set_bar_mode (pane, CAJA_BAR_PATH);
    }

    g_signal_handlers_block_by_func (pane->location_button,
                                     G_CALLBACK (location_button_toggled_cb),
                                     pane);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pane->location_button), use_entry);
    g_signal_handlers_unblock_by_func (pane->location_button,
                                       G_CALLBACK (location_button_toggled_cb),
                                       pane);
}

void
caja_navigation_window_pane_setup (CajaNavigationWindowPane *pane)
{
    GtkWidget *hbox;
    CajaEntry *entry;
    GtkSizeGroup *header_size_group;

    pane->widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
    pane->location_bar = hbox;
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 4);
    gtk_box_pack_start (GTK_BOX (pane->widget), hbox,
                        FALSE, FALSE, 0);
    gtk_widget_show (hbox);

    /* the header size group ensures that the location bar has the same height as the sidebar header */
    header_size_group = CAJA_NAVIGATION_WINDOW (CAJA_WINDOW_PANE (pane)->window)->details->header_size_group;
    gtk_size_group_add_widget (header_size_group, pane->location_bar);

    pane->location_button = location_button_create (pane);
    gtk_box_pack_start (GTK_BOX (hbox), pane->location_button, FALSE, FALSE, 0);
    gtk_widget_show (pane->location_button);

    pane->path_bar = g_object_new (CAJA_TYPE_PATH_BAR, NULL);
    gtk_widget_show (pane->path_bar);

    g_signal_connect_object (pane->path_bar, "path_clicked",
                             G_CALLBACK (path_bar_location_changed_callback), pane, 0);

    g_signal_connect_object (pane->path_bar, "path-event",
                             G_CALLBACK (path_bar_path_event_callback), pane, 0);

    gtk_box_pack_start (GTK_BOX (hbox),
                        pane->path_bar,
                        TRUE, TRUE, 0);

    pane->navigation_bar = caja_location_bar_new (pane);
    g_signal_connect_object (pane->navigation_bar, "location_changed",
                             G_CALLBACK (navigation_bar_location_changed_callback), pane, 0);
    g_signal_connect_object (pane->navigation_bar, "cancel",
                             G_CALLBACK (navigation_bar_cancel_callback), pane, 0);
    entry = caja_location_bar_get_entry (CAJA_LOCATION_BAR (pane->navigation_bar));
    g_signal_connect (entry, "focus-in-event",
                      G_CALLBACK (navigation_bar_focus_in_callback), pane);

    gtk_box_pack_start (GTK_BOX (hbox),
                        pane->navigation_bar,
                        TRUE, TRUE, 0);

    pane->search_bar = caja_search_bar_new (CAJA_WINDOW_PANE (pane)->window);
    g_signal_connect_object (pane->search_bar, "activate",
                             G_CALLBACK (search_bar_activate_callback), pane, 0);
    g_signal_connect_object (pane->search_bar, "cancel",
                             G_CALLBACK (search_bar_cancel_callback), pane, 0);
    g_signal_connect_object (pane->search_bar, "focus-in",
                             G_CALLBACK (search_bar_focus_in_callback), pane, 0);
    gtk_box_pack_start (GTK_BOX (hbox),
                        pane->search_bar,
                        TRUE, TRUE, 0);

    pane->notebook = g_object_new (CAJA_TYPE_NOTEBOOK, NULL);
    gtk_box_pack_start (GTK_BOX (pane->widget), pane->notebook,
                        TRUE, TRUE, 0);
    g_signal_connect (pane->notebook,
                      "tab-close-request",
                      G_CALLBACK (notebook_tab_close_requested),
                      pane);
    g_signal_connect_after (pane->notebook,
                            "button_press_event",
                            G_CALLBACK (notebook_button_press_cb),
                            pane);
    g_signal_connect (pane->notebook, "popup-menu",
                      G_CALLBACK (notebook_popup_menu_cb),
                      pane);
    g_signal_connect (pane->notebook,
                      "switch-page",
                      G_CALLBACK (notebook_switch_page_cb),
                      pane);

    gtk_notebook_set_show_tabs (GTK_NOTEBOOK (pane->notebook), FALSE);
    gtk_notebook_set_show_border (GTK_NOTEBOOK (pane->notebook), FALSE);
    gtk_widget_show (pane->notebook);
    gtk_container_set_border_width (GTK_CONTAINER (pane->notebook), 0);

    /* Ensure that the view has some minimal size and that other parts
     * of the UI (like location bar and tabs) don't request more and
     * thus affect the default position of the split view paned.
     */
    gtk_widget_set_size_request (pane->widget, 60, 60);
}


void
caja_navigation_window_pane_show_location_bar_temporarily (CajaNavigationWindowPane *pane)
{
    if (!caja_navigation_window_pane_location_bar_showing (pane))
    {
        caja_navigation_window_pane_show_location_bar (pane, FALSE);
        pane->temporary_location_bar = TRUE;
    }
}

void
caja_navigation_window_pane_show_navigation_bar_temporarily (CajaNavigationWindowPane *pane)
{
    if (caja_navigation_window_pane_path_bar_showing (pane)
            || caja_navigation_window_pane_search_bar_showing (pane))
    {
        caja_navigation_window_pane_set_bar_mode (pane, CAJA_BAR_NAVIGATION);
        pane->temporary_navigation_bar = TRUE;
    }
    caja_location_bar_activate
    (CAJA_LOCATION_BAR (pane->navigation_bar));
}

gboolean
caja_navigation_window_pane_path_bar_showing (CajaNavigationWindowPane *pane)
{
    if (pane->path_bar != NULL)
    {
        return gtk_widget_get_visible (pane->path_bar);
    }
    /* If we're not visible yet we haven't changed visibility, so its TRUE */
    return TRUE;
}

void
caja_navigation_window_pane_set_bar_mode (CajaNavigationWindowPane *pane,
        CajaBarMode mode)
{
    gboolean use_entry;
    GtkWidget *focus_widget;
    CajaNavigationWindow *window;

    switch (mode)
    {

    case CAJA_BAR_PATH:
        gtk_widget_show (pane->path_bar);
        gtk_widget_hide (pane->navigation_bar);
        gtk_widget_hide (pane->search_bar);
        break;

    case CAJA_BAR_NAVIGATION:
        gtk_widget_show (pane->navigation_bar);
        gtk_widget_hide (pane->path_bar);
        gtk_widget_hide (pane->search_bar);
        break;

    case CAJA_BAR_SEARCH:
        gtk_widget_show (pane->search_bar);
        gtk_widget_hide (pane->path_bar);
        gtk_widget_hide (pane->navigation_bar);
        break;
    }

    if (mode == CAJA_BAR_NAVIGATION || mode == CAJA_BAR_PATH) {
        use_entry = (mode == CAJA_BAR_NAVIGATION);

        g_signal_handlers_block_by_func (pane->location_button,
                         G_CALLBACK (location_button_toggled_cb),
                         pane);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pane->location_button),
                          use_entry);
        g_signal_handlers_unblock_by_func (pane->location_button,
                           G_CALLBACK (location_button_toggled_cb),
                           pane);
    }

    window = CAJA_NAVIGATION_WINDOW (CAJA_WINDOW_PANE (pane)->window);
    focus_widget = gtk_window_get_focus (GTK_WINDOW (window));
    if (focus_widget != NULL && !caja_navigation_window_is_in_temporary_navigation_bar (focus_widget, window) &&
            !caja_navigation_window_is_in_temporary_search_bar (focus_widget, window))
    {
        if (mode == CAJA_BAR_NAVIGATION || mode == CAJA_BAR_PATH)
        {
            caja_navigation_window_set_search_button (window, FALSE);
        }
        else
        {
            caja_navigation_window_set_search_button (window, TRUE);
        }
    }
}

gboolean
caja_navigation_window_pane_search_bar_showing (CajaNavigationWindowPane *pane)
{
    if (pane->search_bar != NULL)
    {
        return gtk_widget_get_visible (pane->search_bar);
    }
    /* If we're not visible yet we haven't changed visibility, so its TRUE */
    return TRUE;
}

void
caja_navigation_window_pane_hide_location_bar (CajaNavigationWindowPane *pane, gboolean save_preference)
{
    pane->temporary_location_bar = FALSE;
    gtk_widget_hide(pane->location_bar);
    caja_navigation_window_update_show_hide_menu_items(
        CAJA_NAVIGATION_WINDOW (CAJA_WINDOW_PANE (pane)->window));
    if (save_preference)
    {
        g_settings_set_boolean (caja_window_state, CAJA_WINDOW_STATE_START_WITH_LOCATION_BAR, FALSE);
    }
}

void
caja_navigation_window_pane_show_location_bar (CajaNavigationWindowPane *pane, gboolean save_preference)
{
    gtk_widget_show(pane->location_bar);
    caja_navigation_window_update_show_hide_menu_items(CAJA_NAVIGATION_WINDOW (CAJA_WINDOW_PANE (pane)->window));
    if (save_preference)
    {
        g_settings_set_boolean (caja_window_state, CAJA_WINDOW_STATE_START_WITH_LOCATION_BAR, TRUE);
    }
}

gboolean
caja_navigation_window_pane_location_bar_showing (CajaNavigationWindowPane *pane)
{
    if (!CAJA_IS_NAVIGATION_WINDOW_PANE (pane))
    {
        return FALSE;
    }
    if (pane->location_bar != NULL)
    {
        return gtk_widget_get_visible (pane->location_bar);
    }
    /* If we're not visible yet we haven't changed visibility, so its TRUE */
    return TRUE;
}

static void
caja_navigation_window_pane_init (CajaNavigationWindowPane *pane)
{
}

static void
caja_navigation_window_pane_show (CajaWindowPane *pane)
{
    CajaNavigationWindowPane *npane = CAJA_NAVIGATION_WINDOW_PANE (pane);

    gtk_widget_show (npane->widget);
}

/* either called due to slot change, or due to location change in the current slot. */
static void
real_sync_search_widgets (CajaWindowPane *window_pane)
{
    CajaWindowSlot *slot;
    CajaDirectory *directory;
    CajaSearchDirectory *search_directory;
    CajaNavigationWindowPane *pane;

    pane = CAJA_NAVIGATION_WINDOW_PANE (window_pane);
    slot = window_pane->active_slot;
    search_directory = NULL;

    directory = caja_directory_get (slot->location);
    if (CAJA_IS_SEARCH_DIRECTORY (directory))
    {
        search_directory = CAJA_SEARCH_DIRECTORY (directory);
    }

    if (search_directory != NULL &&
            !caja_search_directory_is_saved_search (search_directory))
    {
        caja_navigation_window_pane_show_location_bar_temporarily (pane);
        caja_navigation_window_pane_set_bar_mode (pane, CAJA_BAR_SEARCH);
        pane->temporary_search_bar = FALSE;
    }
    else
    {
        pane->temporary_search_bar = TRUE;
        caja_navigation_window_pane_hide_temporary_bars (pane);
    }
    caja_directory_unref (directory);
}

static void
caja_navigation_window_pane_class_init (CajaNavigationWindowPaneClass *class)
{
    G_OBJECT_CLASS (class)->dispose = caja_navigation_window_pane_dispose;
    CAJA_WINDOW_PANE_CLASS (class)->show = caja_navigation_window_pane_show;
    CAJA_WINDOW_PANE_CLASS (class)->set_active = real_set_active;
    CAJA_WINDOW_PANE_CLASS (class)->sync_search_widgets = real_sync_search_widgets;
    CAJA_WINDOW_PANE_CLASS (class)->sync_location_widgets = real_sync_location_widgets;
}

static void
caja_navigation_window_pane_dispose (GObject *object)
{
    CajaNavigationWindowPane *pane = CAJA_NAVIGATION_WINDOW_PANE (object);

    gtk_widget_destroy (pane->widget);

    G_OBJECT_CLASS (parent_class)->dispose (object);
}

CajaNavigationWindowPane *
caja_navigation_window_pane_new (CajaWindow *window)
{
    CajaNavigationWindowPane *pane;

    pane = g_object_new (CAJA_TYPE_NAVIGATION_WINDOW_PANE, NULL);
    CAJA_WINDOW_PANE(pane)->window = window;

    return pane;
}
