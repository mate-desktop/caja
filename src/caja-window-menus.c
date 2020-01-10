/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Caja
 *
 * Copyright (C) 2000, 2001 Eazel, Inc.
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
 */

/* caja-window-menus.h - implementation of caja window menu operations,
 *                           split into separate file just for convenience.
 */
#include <config.h>
#include <locale.h>
#include <string.h>

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib/gi18n.h>

#include <eel/eel-gtk-extensions.h>

#include <libcaja-extension/caja-menu-provider.h>
#include <libcaja-private/caja-extensions.h>
#include <libcaja-private/caja-file-utilities.h>
#include <libcaja-private/caja-global-preferences.h>
#include <libcaja-private/caja-icon-names.h>
#include <libcaja-private/caja-ui-utilities.h>
#include <libcaja-private/caja-module.h>
#include <libcaja-private/caja-search-directory.h>
#include <libcaja-private/caja-search-engine.h>
#include <libcaja-private/caja-signaller.h>
#include <libcaja-private/caja-trash-monitor.h>

#include "caja-actions.h"
#include "caja-application.h"
#include "caja-connect-server-dialog.h"
#include "caja-file-management-properties.h"
#include "caja-property-browser.h"
#include "caja-window-manage-views.h"
#include "caja-window-bookmarks.h"
#include "caja-window-private.h"
#include "caja-desktop-window.h"
#include "caja-search-bar.h"

#define MENU_PATH_EXTENSION_ACTIONS                     "/MenuBar/File/Extension Actions"
#define POPUP_PATH_EXTENSION_ACTIONS                     "/background/Before Zoom Items/Extension Actions"

#define NETWORK_URI          "network:"
#define COMPUTER_URI         "computer:"

/* Struct that stores all the info necessary to activate a bookmark. */
typedef struct
{
    CajaBookmark *bookmark;
    CajaWindow *window;
    guint changed_handler_id;
    CajaBookmarkFailedCallback failed_callback;
} BookmarkHolder;

static BookmarkHolder *
bookmark_holder_new (CajaBookmark *bookmark,
                     CajaWindow *window,
                     GCallback refresh_callback,
                     CajaBookmarkFailedCallback failed_callback)
{
    BookmarkHolder *new_bookmark_holder;

    new_bookmark_holder = g_new (BookmarkHolder, 1);
    new_bookmark_holder->window = window;
    new_bookmark_holder->bookmark = bookmark;
    new_bookmark_holder->failed_callback = failed_callback;
    /* Ref the bookmark because it might be unreffed away while
     * we're holding onto it (not an issue for window).
     */
    g_object_ref (bookmark);
    new_bookmark_holder->changed_handler_id =
        g_signal_connect_object (bookmark, "appearance_changed",
                                 refresh_callback,
                                 window, G_CONNECT_SWAPPED);

    return new_bookmark_holder;
}

static void
bookmark_holder_free (BookmarkHolder *bookmark_holder)
{
    if (g_signal_handler_is_connected(bookmark_holder->bookmark,
                                      bookmark_holder->changed_handler_id)){
    g_signal_handler_disconnect (bookmark_holder->bookmark,
                                      bookmark_holder->changed_handler_id);
    }
    g_object_unref (bookmark_holder->bookmark);
    g_free (bookmark_holder);
}

static void
bookmark_holder_free_cover (gpointer callback_data, GClosure *closure)
{
    bookmark_holder_free (callback_data);
}

static gboolean
should_open_in_new_tab (void)
{
    /* FIXME this is duplicated */
    GdkEvent *event;

    event = gtk_get_current_event ();

    if (event == NULL)
    {
        return FALSE;
    }

    if (event->type == GDK_BUTTON_PRESS || event->type == GDK_BUTTON_RELEASE)
    {
        return event->button.button == 2;
    }

    gdk_event_free (event);

    return FALSE;
}

static void
activate_bookmark_in_menu_item (GtkAction *action, gpointer user_data)
{
    BookmarkHolder *holder;

    holder = (BookmarkHolder *)user_data;

    if (caja_bookmark_uri_known_not_to_exist (holder->bookmark))
    {
        holder->failed_callback (holder->window, holder->bookmark);
    }
    else
    {
        CajaWindowSlot *slot;
        GFile *location;

        location = caja_bookmark_get_location (holder->bookmark);
        slot = caja_window_get_active_slot (holder->window);
        caja_window_slot_go_to (slot,
                                location,
                                should_open_in_new_tab ());
        g_object_unref (location);
    }
}

void
caja_menus_append_bookmark_to_menu (CajaWindow *window,
                                    CajaBookmark *bookmark,
                                    const char *parent_path,
                                    const char *parent_id,
                                    guint index_in_parent,
                                    GtkActionGroup *action_group,
                                    guint merge_id,
                                    GCallback refresh_callback,
                                    CajaBookmarkFailedCallback failed_callback)
{
    BookmarkHolder *bookmark_holder;
    char action_name[128];
    char *name;
    char *path;
    cairo_surface_t *surface;
    GtkAction *action;
    GtkWidget *menuitem;

    g_assert (CAJA_IS_WINDOW (window));
    g_assert (CAJA_IS_BOOKMARK (bookmark));

    bookmark_holder = bookmark_holder_new (bookmark, window, refresh_callback, failed_callback);
    name = caja_bookmark_get_name (bookmark);

    /* Create menu item with surface */
    surface = caja_bookmark_get_surface (bookmark, GTK_ICON_SIZE_MENU);

    g_snprintf (action_name, sizeof (action_name), "%s%d", parent_id, index_in_parent);

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    action = gtk_action_new (action_name,
                             name,
                             _("Go to the location specified by this bookmark"),
                             NULL);
    G_GNUC_END_IGNORE_DEPRECATIONS;

    g_object_set_data_full (G_OBJECT (action), "menu-icon",
                            cairo_surface_reference (surface),
                            (GDestroyNotify)cairo_surface_destroy);

    g_signal_connect_data (action, "activate",
                           G_CALLBACK (activate_bookmark_in_menu_item),
                           bookmark_holder,
                           bookmark_holder_free_cover, 0);

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    gtk_action_group_add_action (action_group,
                                 GTK_ACTION (action));
    G_GNUC_END_IGNORE_DEPRECATIONS;

    g_object_unref (action);

    gtk_ui_manager_add_ui (window->details->ui_manager,
                           merge_id,
                           parent_path,
                           action_name,
                           action_name,
                           GTK_UI_MANAGER_MENUITEM,
                           FALSE);

    path = g_strdup_printf ("%s/%s", parent_path, action_name);
    menuitem = gtk_ui_manager_get_widget (window->details->ui_manager,
                                          path);
    gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM (menuitem),
            TRUE);

    cairo_surface_destroy (surface);
    g_free (path);
    g_free (name);
}

static void
action_close_window_slot_callback (GtkAction *action,
                                   gpointer user_data)
{
    CajaWindow *window;
    CajaWindowSlot *slot;

    window = CAJA_WINDOW (user_data);
    slot = caja_window_get_active_slot (window);

    caja_window_slot_close (slot);
}

static void
action_connect_to_server_callback (GtkAction *action,
                                   gpointer user_data)
{
    CajaWindow *window = CAJA_WINDOW (user_data);
    GtkWidget *dialog;

    dialog = caja_connect_server_dialog_new (window);

    gtk_widget_show (dialog);
}

static void
action_stop_callback (GtkAction *action,
                      gpointer user_data)
{
    CajaWindow *window;
    CajaWindowSlot *slot;

    window = CAJA_WINDOW (user_data);
    slot = caja_window_get_active_slot (window);

    caja_window_slot_stop_loading (slot);
}

static void
action_home_callback (GtkAction *action,
                      gpointer user_data)
{
    CajaWindow *window;
    CajaWindowSlot *slot;

    window = CAJA_WINDOW (user_data);
    slot = caja_window_get_active_slot (window);

    caja_window_slot_go_home (slot,
                              should_open_in_new_tab ());
}

static void
action_go_to_computer_callback (GtkAction *action,
                                gpointer user_data)
{
    CajaWindow *window;
    CajaWindowSlot *slot;
    GFile *computer;

    window = CAJA_WINDOW (user_data);
    slot = caja_window_get_active_slot (window);

    computer = g_file_new_for_uri (COMPUTER_URI);
    caja_window_slot_go_to (slot,
                            computer,
                            should_open_in_new_tab ());
    g_object_unref (computer);
}

static void
action_go_to_network_callback (GtkAction *action,
                               gpointer user_data)
{
    CajaWindow *window;
    CajaWindowSlot *slot;
    GFile *network;

    window = CAJA_WINDOW (user_data);
    slot = caja_window_get_active_slot (window);

    network = g_file_new_for_uri (NETWORK_URI);
    caja_window_slot_go_to (slot,
                            network,
                            should_open_in_new_tab ());
    g_object_unref (network);
}

static void
action_go_to_templates_callback (GtkAction *action,
                                 gpointer user_data)
{
    CajaWindow *window;
    CajaWindowSlot *slot;
    char *path;
    GFile *location;

    window = CAJA_WINDOW (user_data);
    slot = caja_window_get_active_slot (window);

    path = caja_get_templates_directory ();
    location = g_file_new_for_path (path);
    g_free (path);
    caja_window_slot_go_to (slot,
                            location,
                            should_open_in_new_tab ());
    g_object_unref (location);
}

static void
action_go_to_trash_callback (GtkAction *action,
                             gpointer user_data)
{
    CajaWindow *window;
    CajaWindowSlot *slot;
    GFile *trash;

    window = CAJA_WINDOW (user_data);
    slot = caja_window_get_active_slot (window);

    trash = g_file_new_for_uri ("trash:///");
    caja_window_slot_go_to (slot,
                            trash,
                            should_open_in_new_tab ());
    g_object_unref (trash);
}

static void
action_reload_callback (GtkAction *action,
                        gpointer user_data)
{
    caja_window_reload (CAJA_WINDOW (user_data));
}

static void
action_zoom_in_callback (GtkAction *action,
                         gpointer user_data)
{
    caja_window_zoom_in (CAJA_WINDOW (user_data));
}

static void
action_zoom_out_callback (GtkAction *action,
                          gpointer user_data)
{
    caja_window_zoom_out (CAJA_WINDOW (user_data));
}

static void
action_zoom_normal_callback (GtkAction *action,
                             gpointer user_data)
{
    caja_window_zoom_to_default (CAJA_WINDOW (user_data));
}

static void
action_show_hidden_files_callback (GtkAction *action,
                                   gpointer callback_data)
{
    CajaWindow *window;
    CajaWindowShowHiddenFilesMode mode;

    window = CAJA_WINDOW (callback_data);

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)))
    {
        mode = CAJA_WINDOW_SHOW_HIDDEN_FILES_ENABLE;
    }
    else
    {
        mode = CAJA_WINDOW_SHOW_HIDDEN_FILES_DISABLE;
    }
    G_GNUC_END_IGNORE_DEPRECATIONS;

    caja_window_info_set_hidden_files_mode (window, mode);
}

static void
action_show_backup_files_callback (GtkAction *action,
                                   gpointer callback_data)
{
    CajaWindow *window;
    CajaWindowShowBackupFilesMode mode;

    window = CAJA_WINDOW (callback_data);

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)))
    {
        mode = CAJA_WINDOW_SHOW_BACKUP_FILES_ENABLE;
    }
    else
    {
        mode = CAJA_WINDOW_SHOW_BACKUP_FILES_DISABLE;
    }
    G_GNUC_END_IGNORE_DEPRECATIONS;

    caja_window_info_set_backup_files_mode (window, mode);
}

static void
show_hidden_files_preference_callback (gpointer callback_data)
{
    CajaWindow *window;

    window = CAJA_WINDOW (callback_data);

    if (window->details->show_hidden_files_mode == CAJA_WINDOW_SHOW_HIDDEN_FILES_DEFAULT)
    {
        GtkAction *action;

        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
        action = gtk_action_group_get_action (window->details->main_action_group, CAJA_ACTION_SHOW_HIDDEN_FILES);
        g_assert (GTK_IS_ACTION (action));

        /* update button */
        g_signal_handlers_block_by_func (action, action_show_hidden_files_callback, window);
        gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
                                      g_settings_get_boolean (caja_preferences, CAJA_PREFERENCES_SHOW_HIDDEN_FILES));
        G_GNUC_END_IGNORE_DEPRECATIONS;
        g_signal_handlers_unblock_by_func (action, action_show_hidden_files_callback, window);

        /* inform views */
        caja_window_info_set_hidden_files_mode (window, CAJA_WINDOW_SHOW_HIDDEN_FILES_DEFAULT);

    }
}

static void
show_backup_files_preference_callback (gpointer callback_data)
{
    CajaWindow *window;

    window = CAJA_WINDOW (callback_data);

    if (window->details->show_backup_files_mode == CAJA_WINDOW_SHOW_BACKUP_FILES_DEFAULT)
    {
        GtkAction *action;

        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
        action = gtk_action_group_get_action (window->details->main_action_group, CAJA_ACTION_SHOW_BACKUP_FILES);
        g_assert (GTK_IS_ACTION (action));

        /* update button */
        g_signal_handlers_block_by_func (action, action_show_backup_files_callback, window);
        gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
                                      g_settings_get_boolean (caja_preferences, CAJA_PREFERENCES_SHOW_BACKUP_FILES));
        G_GNUC_END_IGNORE_DEPRECATIONS;
        g_signal_handlers_unblock_by_func (action, action_show_backup_files_callback, window);

        /* inform views */
        caja_window_info_set_backup_files_mode (window, CAJA_WINDOW_SHOW_BACKUP_FILES_DEFAULT);
    }
}

static void
preferences_respond_callback (GtkDialog *dialog,
                              gint response_id)
{
    if (response_id == GTK_RESPONSE_CLOSE)
    {
        gtk_widget_destroy (GTK_WIDGET (dialog));
    }
}

static void
action_preferences_callback (GtkAction *action,
                             gpointer user_data)
{
    GtkWindow *window;

    window = GTK_WINDOW (user_data);

    caja_file_management_properties_dialog_show (G_CALLBACK (preferences_respond_callback), window);
}

static void
action_backgrounds_and_emblems_callback (GtkAction *action,
        gpointer user_data)
{
    GtkWindow *window;

    window = GTK_WINDOW (user_data);

    caja_property_browser_show (gtk_window_get_screen (window));
}

#define ABOUT_GROUP "About"
#define EMAILIFY(string) (g_strdelimit ((string), "%", '@'))

static void
action_about_caja_callback (GtkAction *action,
                            gpointer user_data)
{
    const gchar *license[] =
    {
        N_("Caja is free software; you can redistribute it and/or modify "
        "it under the terms of the GNU General Public License as published by "
        "the Free Software Foundation; either version 2 of the License, or "
        "(at your option) any later version."),
        N_("Caja is distributed in the hope that it will be useful, "
        "but WITHOUT ANY WARRANTY; without even the implied warranty of "
        "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
        "GNU General Public License for more details."),
        N_("You should have received a copy of the GNU General Public License "
        "along with Caja; if not, write to the Free Software Foundation, Inc., "
        "51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA")
    };
    gchar *license_trans;
    GKeyFile *key_file;
    GError *error = NULL;
    char **authors, **documenters;
    gsize n_authors = 0, n_documenters = 0 , i;

    key_file = g_key_file_new ();
    if (!g_key_file_load_from_file (key_file, CAJA_DATADIR G_DIR_SEPARATOR_S "caja.about", 0, &error))
    {
        g_warning ("Couldn't load about data: %s\n", error->message);
        g_error_free (error);
        g_key_file_free (key_file);
        return;
    }

    authors = g_key_file_get_string_list (key_file, ABOUT_GROUP, "Authors", &n_authors, NULL);
    documenters = g_key_file_get_string_list (key_file, ABOUT_GROUP, "Documenters", &n_documenters, NULL);
    g_key_file_free (key_file);

    for (i = 0; i < n_authors; ++i)
        authors[i] = EMAILIFY (authors[i]);
    for (i = 0; i < n_documenters; ++i)
        documenters[i] = EMAILIFY (documenters[i]);

    license_trans = g_strjoin ("\n\n", _(license[0]), _(license[1]), _(license[2]), NULL);

    gtk_show_about_dialog (GTK_WINDOW (user_data),
                           "program-name", _("Caja"),
                           "title", _("About Caja"),
                           "version", VERSION,
                           "comments", _("Caja lets you organize "
                                         "files and folders, both on "
                                         "your computer and online."),
                           "copyright", _("Copyright \xC2\xA9 1999-2009 The Nautilus authors\n"
                                          "Copyright \xC2\xA9 2011-2020 The Caja authors"),
                           "license", license_trans,
                           "wrap-license", TRUE,
                           "authors", authors,
                           "documenters", documenters,
                           "translator-credits", _("translator-credits"),
                           "logo-icon-name", "system-file-manager",
                           "website", "https://mate-desktop.org",
                           "website-label", _("MATE Web Site"),
                           NULL);

    g_strfreev (authors);
    g_strfreev (documenters);
    g_free (license_trans);

}

static void
action_up_callback (GtkAction *action,
                    gpointer user_data)
{
    caja_window_go_up (CAJA_WINDOW (user_data), FALSE, should_open_in_new_tab ());
}

static void
action_caja_manual_callback (GtkAction *action,
                             gpointer user_data)
{
    CajaWindow *window;
    GError *error;

    error = NULL;
    window = CAJA_WINDOW (user_data);

    gtk_show_uri_on_window (GTK_WINDOW (window),
                            CAJA_IS_DESKTOP_WINDOW (window)
                               ? "help:mate-user-guide"
                               : "help:mate-user-guide/goscaja-1",
                            gtk_get_current_event_time (), &error);

    if (error)
    {
        GtkWidget *dialog;

        dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                         GTK_DIALOG_MODAL,
                                         GTK_MESSAGE_ERROR,
                                         GTK_BUTTONS_OK,
                                         _("There was an error displaying help: \n%s"),
                                         error->message);
        g_signal_connect (G_OBJECT (dialog), "response",
                          G_CALLBACK (gtk_widget_destroy),
                          NULL);

        gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
        gtk_widget_show (dialog);
        g_error_free (error);
    }
}

static void
menu_item_select_cb (GtkMenuItem *proxy,
                     CajaWindow *window)
{
    GtkAction *action;
    char *message;

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    action = gtk_activatable_get_related_action (GTK_ACTIVATABLE (proxy));
    G_GNUC_END_IGNORE_DEPRECATIONS;
    g_return_if_fail (action != NULL);

    g_object_get (G_OBJECT (action), "tooltip", &message, NULL);
    if (message)
    {
        gtk_statusbar_push (GTK_STATUSBAR (window->details->statusbar),
                            window->details->help_message_cid, message);
        g_free (message);
    }
}

static void
menu_item_deselect_cb (GtkMenuItem *proxy,
                       CajaWindow *window)
{
    gtk_statusbar_pop (GTK_STATUSBAR (window->details->statusbar),
                       window->details->help_message_cid);
}

static GtkWidget *
get_event_widget (GtkWidget *proxy)
{
    GtkWidget *widget;

    /**
     * Finding the interesting widget requires internal knowledge of
     * the widgets in question. This can't be helped, but by keeping
     * the sneaky code in one place, it can easily be updated.
     */
    if (GTK_IS_MENU_ITEM (proxy))
    {
        /* Menu items already forward middle clicks */
        widget = NULL;
    }
    else if (GTK_IS_MENU_TOOL_BUTTON (proxy))
    {
        widget = eel_gtk_menu_tool_button_get_button (GTK_MENU_TOOL_BUTTON (proxy));
    }
    else if (GTK_IS_TOOL_BUTTON (proxy))
    {
        /* The tool button's button is the direct child */
        widget = gtk_bin_get_child (GTK_BIN (proxy));
    }
    else if (GTK_IS_BUTTON (proxy))
    {
        widget = proxy;
    }
    else
    {
        /* Don't touch anything we don't know about */
        widget = NULL;
    }

    return widget;
}

static gboolean
proxy_button_press_event_cb (GtkButton *button,
                             GdkEventButton *event,
                             gpointer user_data)
{
    if (event->button == 2)
    {
        g_signal_emit_by_name (button, "pressed", 0);
    }

    return FALSE;
}

static gboolean
proxy_button_release_event_cb (GtkButton *button,
                               GdkEventButton *event,
                               gpointer user_data)
{
    if (event->button == 2)
    {
        g_signal_emit_by_name (button, "released", 0);
    }

    return FALSE;
}

static void
disconnect_proxy_cb (GtkUIManager *manager,
                     GtkAction *action,
                     GtkWidget *proxy,
                     CajaWindow *window)
{
    GtkWidget *widget;

    if (GTK_IS_MENU_ITEM (proxy))
    {
        g_signal_handlers_disconnect_by_func
        (proxy, G_CALLBACK (menu_item_select_cb), window);
        g_signal_handlers_disconnect_by_func
        (proxy, G_CALLBACK (menu_item_deselect_cb), window);
    }

    widget = get_event_widget (proxy);
    if (widget)
    {
        g_signal_handlers_disconnect_by_func (widget,
                                              G_CALLBACK (proxy_button_press_event_cb),
                                              action);
        g_signal_handlers_disconnect_by_func (widget,
                                              G_CALLBACK (proxy_button_release_event_cb),
                                              action);
    }

}

static void
connect_proxy_cb (GtkUIManager *manager,
                  GtkAction *action,
                  GtkWidget *proxy,
                  CajaWindow *window)
{
    cairo_surface_t *icon;
    GtkWidget *widget;

    if (GTK_IS_MENU_ITEM (proxy))
    {
        g_signal_connect (proxy, "select",
                          G_CALLBACK (menu_item_select_cb), window);
        g_signal_connect (proxy, "deselect",
                          G_CALLBACK (menu_item_deselect_cb), window);


        /* This is a way to easily get surfaces into the menu items */
        icon = g_object_get_data (G_OBJECT (action), "menu-icon");
        if (icon != NULL)
        {
            gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (proxy),
                                           gtk_image_new_from_surface (icon));
        }
    }
    if (GTK_IS_TOOL_BUTTON (proxy))
    {
        icon = g_object_get_data (G_OBJECT (action), "toolbar-icon");
        if (icon != NULL)
        {
            widget = gtk_image_new_from_surface (icon);
            gtk_widget_show (widget);
            gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (proxy),
                                             widget);
        }
    }

    widget = get_event_widget (proxy);
    if (widget)
    {
        g_signal_connect (widget, "button-press-event",
                          G_CALLBACK (proxy_button_press_event_cb),
                          action);
        g_signal_connect (widget, "button-release-event",
                          G_CALLBACK (proxy_button_release_event_cb),
                          action);
    }
}

static void
trash_state_changed_cb (CajaTrashMonitor *monitor,
                        gboolean state,
                        CajaWindow *window)
{
    GtkActionGroup *action_group;
    GtkAction *action;
    GIcon *gicon;

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    action_group = window->details->main_action_group;
    action = gtk_action_group_get_action (action_group, "Go to Trash");
    G_GNUC_END_IGNORE_DEPRECATIONS;

    gicon = caja_trash_monitor_get_icon ();

    if (gicon)
    {
        g_object_set (action, "gicon", gicon, NULL);
        g_object_unref (gicon);
    }
}

static void
caja_window_initialize_trash_icon_monitor (CajaWindow *window)
{
    CajaTrashMonitor *monitor;

    monitor = caja_trash_monitor_get ();

    trash_state_changed_cb (monitor, TRUE, window);

    g_signal_connect (monitor, "trash_state_changed",
                      G_CALLBACK (trash_state_changed_cb), window);
}

static const GtkActionEntry main_entries[] =
{
    /* name, icon name, label */ { "File", NULL, N_("_File") },
    /* name, icon name, label */ { "Edit", NULL, N_("_Edit") },
    /* name, icon name, label */ { "View", NULL, N_("_View") },
    /* name, icon name, label */ { "Help", NULL, N_("_Help") },
    /* name, icon name */        { "Close", "window-close",
        /* label, accelerator */       N_("_Close"), "<control>W",
        /* tooltip */                  N_("Close this folder"),
        G_CALLBACK (action_close_window_slot_callback)
    },
    {
        "Backgrounds and Emblems", NULL,
        N_("_Backgrounds and Emblems..."),
        NULL, N_("Display patterns, colors, and emblems that can be used to customize appearance"),
        G_CALLBACK (action_backgrounds_and_emblems_callback)
    },
    {
        "Preferences", "preferences-desktop",
        N_("Prefere_nces"),
        NULL, N_("Edit Caja preferences"),
        G_CALLBACK (action_preferences_callback)
    },
    /* name, icon name, label */ { "Up", "go-up", N_("Open _Parent"),
        "<alt>Up", N_("Open the parent folder"),
        G_CALLBACK (action_up_callback)
    },
    /* name, icon name, label */ { "UpAccel", NULL, "UpAccel",
        "", NULL,
        G_CALLBACK (action_up_callback)
    },
    /* name, icon name */        { "Stop", "process-stop",
        /* label, accelerator */       N_("_Stop"), NULL,
        /* tooltip */                  N_("Stop loading the current location"),
        G_CALLBACK (action_stop_callback)
    },
    /* name, icon name */        { "Reload", "view-refresh",
        /* label, accelerator */       N_("_Reload"), "<control>R",
        /* tooltip */                  N_("Reload the current location"),
        G_CALLBACK (action_reload_callback)
    },
    /* name, icon name */        { "Caja Manual", "help-browser",
        /* label, accelerator */       N_("_Contents"), "F1",
        /* tooltip */                  N_("Display Caja help"),
        G_CALLBACK (action_caja_manual_callback)
    },
    /* name, icon name */        { "About Caja", "help-about",
        /* label, accelerator */       N_("_About"), NULL,
        /* tooltip */                  N_("Display credits for the creators of Caja"),
        G_CALLBACK (action_about_caja_callback)
    },
    /* name, icon name */        { "Zoom In", "zoom-in",
        /* label, accelerator */       N_("Zoom _In"), "<control>plus",
        /* tooltip */                  N_("Increase the view size"),
        G_CALLBACK (action_zoom_in_callback)
    },
    /* name, icon name */        { "ZoomInAccel", NULL,
        /* label, accelerator */       "ZoomInAccel", "<control>equal",
        /* tooltip */                  NULL,
        G_CALLBACK (action_zoom_in_callback)
    },
    /* name, icon name */        { "ZoomInAccel2", NULL,
        /* label, accelerator */       "ZoomInAccel2", "<control>KP_Add",
        /* tooltip */                  NULL,
        G_CALLBACK (action_zoom_in_callback)
    },
    /* name, icon name */        { "Zoom Out", "zoom-out",
        /* label, accelerator */       N_("Zoom _Out"), "<control>minus",
        /* tooltip */                  N_("Decrease the view size"),
        G_CALLBACK (action_zoom_out_callback)
    },
    /* name, icon name */        { "ZoomOutAccel", NULL,
        /* label, accelerator */       "ZoomOutAccel", "<control>KP_Subtract",
        /* tooltip */                  NULL,
        G_CALLBACK (action_zoom_out_callback)
    },
    /* name, icon name */        { "Zoom Normal", "zoom-original",
        /* label, accelerator */       N_("Normal Si_ze"), "<control>0",
        /* tooltip */                  N_("Use the normal view size"),
        G_CALLBACK (action_zoom_normal_callback)
    },
    /* name, icon name */        { "Connect to Server", NULL,
        /* label, accelerator */       N_("Connect to _Server..."), NULL,
        /* tooltip */                  N_("Connect to a remote computer or shared disk"),
        G_CALLBACK (action_connect_to_server_callback)
    },
    /* name, icon name */        { "Home", CAJA_ICON_HOME,
        /* label, accelerator */       N_("_Home Folder"), "<alt>Home",
        /* tooltip */                  N_("Open your personal folder"),
        G_CALLBACK (action_home_callback)
    },
    /* name, icon name */        { "Go to Computer", CAJA_ICON_COMPUTER,
        /* label, accelerator */       N_("_Computer"), NULL,
        /* tooltip */                  N_("Browse all local and remote disks and folders accessible from this computer"),
        G_CALLBACK (action_go_to_computer_callback)
    },
    /* name, icon name */        { "Go to Network", CAJA_ICON_NETWORK,
        /* label, accelerator */       N_("_Network"), NULL,
        /* tooltip */                  N_("Browse bookmarked and local network locations"),
        G_CALLBACK (action_go_to_network_callback)
    },
    /* name, icon name */        { "Go to Templates", CAJA_ICON_TEMPLATE,
        /* label, accelerator */       N_("T_emplates"), NULL,
        /* tooltip */                  N_("Open your personal templates folder"),
        G_CALLBACK (action_go_to_templates_callback)
    },
    /* name, icon name */        { "Go to Trash", CAJA_ICON_TRASH,
        /* label, accelerator */       N_("_Trash"), NULL,
        /* tooltip */                  N_("Open your personal trash folder"),
        G_CALLBACK (action_go_to_trash_callback)
    },
};

static const GtkToggleActionEntry main_toggle_entries[] =
{
    /* name, icon name */        { "Show Hidden Files", NULL,
        /* label, accelerator */       N_("Show _Hidden Files"), "<control>H",
        /* tooltip */                  N_("Toggle the display of hidden files in the current window"),
        G_CALLBACK (action_show_hidden_files_callback),
        TRUE
    },
    /* name, stock id */         { "Show Backup Files", NULL,
    /* label, accelerator */       N_("Show Bac_kup Files"), "<control>K",
    /* tooltip */                  N_("Toggle the display of backup files in the current window"),
        G_CALLBACK (action_show_backup_files_callback),
        TRUE
    },

};

/**
 * caja_window_initialize_menus
 *
 * Create and install the set of menus for this window.
 * @window: A recently-created CajaWindow.
 */
void
caja_window_initialize_menus (CajaWindow *window)
{
    GtkActionGroup *action_group;
    GtkUIManager *ui_manager;
    GtkAction *action;
    const char *ui;

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    action_group = gtk_action_group_new ("ShellActions");
    gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
    window->details->main_action_group = action_group;
    gtk_action_group_add_actions (action_group,
                                  main_entries, G_N_ELEMENTS (main_entries),
                                  window);
    gtk_action_group_add_toggle_actions (action_group,
                                         main_toggle_entries, G_N_ELEMENTS (main_toggle_entries),
                                         window);

    action = gtk_action_group_get_action (action_group, CAJA_ACTION_UP);
    g_object_set (action, "short_label", _("_Up"), NULL);

    action = gtk_action_group_get_action (action_group, CAJA_ACTION_HOME);
    g_object_set (action, "short_label", _("_Home"), NULL);

    action = gtk_action_group_get_action (action_group, CAJA_ACTION_SHOW_HIDDEN_FILES);
    g_signal_handlers_block_by_func (action, action_show_hidden_files_callback, window);
    gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
                                  g_settings_get_boolean (caja_preferences, CAJA_PREFERENCES_SHOW_HIDDEN_FILES));
    g_signal_handlers_unblock_by_func (action, action_show_hidden_files_callback, window);
    g_signal_connect_swapped (caja_preferences, "changed::" CAJA_PREFERENCES_SHOW_HIDDEN_FILES,
                              G_CALLBACK(show_hidden_files_preference_callback),
                              window);

    action = gtk_action_group_get_action (action_group, CAJA_ACTION_SHOW_BACKUP_FILES);
    g_signal_handlers_block_by_func (action, action_show_backup_files_callback, window);
    gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
                                  g_settings_get_boolean (caja_preferences, CAJA_PREFERENCES_SHOW_BACKUP_FILES));
    G_GNUC_END_IGNORE_DEPRECATIONS;
    g_signal_handlers_unblock_by_func (action, action_show_backup_files_callback, window);

    g_signal_connect_swapped (caja_preferences, "changed::" CAJA_PREFERENCES_SHOW_BACKUP_FILES,
                              G_CALLBACK(show_backup_files_preference_callback),
                              window);

    window->details->ui_manager = gtk_ui_manager_new ();
    ui_manager = window->details->ui_manager;
    gtk_window_add_accel_group (GTK_WINDOW (window),
                                gtk_ui_manager_get_accel_group (ui_manager));

    g_signal_connect (ui_manager, "connect_proxy",
                      G_CALLBACK (connect_proxy_cb), window);
    g_signal_connect (ui_manager, "disconnect_proxy",
                      G_CALLBACK (disconnect_proxy_cb), window);

    gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
    g_object_unref (action_group); /* owned by ui manager */

    ui = caja_ui_string_get ("caja-shell-ui.xml");
    gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, NULL);

    caja_window_initialize_trash_icon_monitor (window);
}

void
caja_window_finalize_menus (CajaWindow *window)
{
    CajaTrashMonitor *monitor;

    monitor = caja_trash_monitor_get ();

    g_signal_handlers_disconnect_by_func (monitor,
                                          trash_state_changed_cb, window);

    g_signal_handlers_disconnect_by_func (caja_preferences,
                                          show_hidden_files_preference_callback, window);

    g_signal_handlers_disconnect_by_func (caja_preferences,
                                          show_backup_files_preference_callback, window);
}

static GList *
get_extension_menus (CajaWindow *window)
{
    CajaWindowSlot *slot;
    GList *providers;
    GList *items;
    GList *l;

    providers = caja_extensions_get_for_type (CAJA_TYPE_MENU_PROVIDER);
    items = NULL;

    slot = caja_window_get_active_slot (window);

    for (l = providers; l != NULL; l = l->next)
    {
        CajaMenuProvider *provider;
        GList *file_items;

        provider = CAJA_MENU_PROVIDER (l->data);
        file_items = caja_menu_provider_get_background_items (provider,
                     GTK_WIDGET (window),
                     slot->viewed_file);
        items = g_list_concat (items, file_items);
    }

    caja_module_extension_list_free (providers);

    return items;
}

static void
add_extension_menu_items (CajaWindow *window,
                          guint merge_id,
                          GtkActionGroup *action_group,
                          GList *menu_items,
                          const char *subdirectory)
{
    GtkUIManager *ui_manager;
    GList *l;

    ui_manager = window->details->ui_manager;

    for (l = menu_items; l; l = l->next)
    {
        CajaMenuItem *item;
        CajaMenu *menu;
        GtkAction *action;
        char *path;
        const gchar *action_name;

        item = CAJA_MENU_ITEM (l->data);

        g_object_get (item, "menu", &menu, NULL);

        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
        action = caja_action_from_menu_item (item, GTK_WIDGET (window));
        gtk_action_group_add_action_with_accel (action_group, action, NULL);
        action_name = gtk_action_get_name (action);
        G_GNUC_END_IGNORE_DEPRECATIONS;

        path = g_build_path ("/", POPUP_PATH_EXTENSION_ACTIONS, subdirectory, NULL);
        gtk_ui_manager_add_ui (ui_manager,
                               merge_id,
                               path,
                               action_name,
                               action_name,
                               (menu != NULL) ? GTK_UI_MANAGER_MENU : GTK_UI_MANAGER_MENUITEM,
                               FALSE);
        g_free (path);

        path = g_build_path ("/", MENU_PATH_EXTENSION_ACTIONS, subdirectory, NULL);
        gtk_ui_manager_add_ui (ui_manager,
                               merge_id,
                               path,
                               action_name,
                               action_name,
                               (menu != NULL) ? GTK_UI_MANAGER_MENU : GTK_UI_MANAGER_MENUITEM,
                               FALSE);
        g_free (path);

        /* recursively fill the menu */
        if (menu != NULL)
        {
            char *subdir;
            GList *children;

            children = caja_menu_get_items (menu);

            G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
            subdir = g_build_path ("/", subdirectory, "/", gtk_action_get_name (action), NULL);
            G_GNUC_END_IGNORE_DEPRECATIONS;
            add_extension_menu_items (window,
                                      merge_id,
                                      action_group,
                                      children,
                                      subdir);

            caja_menu_item_list_free (children);
            g_free (subdir);
        }
    }
}

void
caja_window_load_extension_menus (CajaWindow *window)
{
    GtkActionGroup *action_group;
    GList *items;
    guint merge_id;

    if (window->details->extensions_menu_merge_id != 0)
    {
        gtk_ui_manager_remove_ui (window->details->ui_manager,
                                  window->details->extensions_menu_merge_id);
        window->details->extensions_menu_merge_id = 0;
    }

    if (window->details->extensions_menu_action_group != NULL)
    {
        gtk_ui_manager_remove_action_group (window->details->ui_manager,
                                            window->details->extensions_menu_action_group);
        window->details->extensions_menu_action_group = NULL;
    }

    merge_id = gtk_ui_manager_new_merge_id (window->details->ui_manager);
    window->details->extensions_menu_merge_id = merge_id;
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    action_group = gtk_action_group_new ("ExtensionsMenuGroup");
    window->details->extensions_menu_action_group = action_group;
    gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
    G_GNUC_END_IGNORE_DEPRECATIONS;
    gtk_ui_manager_insert_action_group (window->details->ui_manager, action_group, 0);
    g_object_unref (action_group); /* owned by ui manager */

    items = get_extension_menus (window);

    if (items != NULL)
    {
        add_extension_menu_items (window, merge_id, action_group, items, "");

        g_list_foreach (items, (GFunc) g_object_unref, NULL);
        g_list_free (items);
    }
}

