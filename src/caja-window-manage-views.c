/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 *  Caja
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000, 2001 Eazel, Inc.
 *
 *  Caja is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  Caja is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this program; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  Authors: Elliot Lee <sopwith@redhat.com>
 *           John Sullivan <sullivan@eazel.com>
 *           Darin Adler <darin@bentspoon.com>
 */

#include <config.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <glib/gi18n.h>

#include <eel/eel-accessibility.h>
#include <eel/eel-debug.h>
#include <eel/eel-gdk-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>

#include <libcaja-private/caja-debug-log.h>
#include <libcaja-private/caja-extensions.h>
#include <libcaja-private/caja-file-attributes.h>
#include <libcaja-private/caja-file-utilities.h>
#include <libcaja-private/caja-file.h>
#include <libcaja-private/caja-global-preferences.h>
#include <libcaja-private/caja-metadata.h>
#include <libcaja-private/caja-mime-actions.h>
#include <libcaja-private/caja-module.h>
#include <libcaja-private/caja-monitor.h>
#include <libcaja-private/caja-search-directory.h>
#include <libcaja-private/caja-view-factory.h>
#include <libcaja-private/caja-window-info.h>
#include <libcaja-private/caja-window-slot-info.h>
#include <libcaja-private/caja-autorun.h>

#include <libcaja-extension/caja-location-widget-provider.h>

#include "caja-window-manage-views.h"
#include "caja-actions.h"
#include "caja-application.h"
#include "caja-location-bar.h"
#include "caja-search-bar.h"
#include "caja-pathbar.h"
#include "caja-window-private.h"
#include "caja-window-slot.h"
#include "caja-navigation-window-slot.h"
#include "caja-trash-bar.h"
#include "caja-x-content-bar.h"
#include "caja-navigation-window-pane.h"

/* FIXME bugzilla.gnome.org 41243:
 * We should use inheritance instead of these special cases
 * for the desktop window.
 */
#include "caja-desktop-window.h"

/* This number controls a maximum character count for a URL that is
 * displayed as part of a dialog. It's fairly arbitrary -- big enough
 * to allow most "normal" URIs to display in full, but small enough to
 * prevent the dialog from getting insanely wide.
 */
#define MAX_URI_IN_DIALOG_LENGTH 60

static void begin_location_change                     (CajaWindowSlot         *slot,
        GFile                      *location,
        GFile                      *previous_location,
        GList                      *new_selection,
        CajaLocationChangeType      type,
        guint                       distance,
        const char                 *scroll_pos,
        CajaWindowGoToCallback      callback,
        gpointer                    user_data);
static void free_location_change                      (CajaWindowSlot         *slot);
static void end_location_change                       (CajaWindowSlot         *slot);
static void cancel_location_change                    (CajaWindowSlot         *slot);
static void got_file_info_for_view_selection_callback (CajaFile               *file,
        gpointer                    callback_data);
static void create_content_view                       (CajaWindowSlot         *slot,
        const char                 *view_id);
static void display_view_selection_failure            (CajaWindow             *window,
        CajaFile               *file,
        GFile                      *location,
        GError                     *error);
static void load_new_location                         (CajaWindowSlot         *slot,
        GFile                      *location,
        GList                      *selection,
        gboolean                    tell_current_content_view,
        gboolean                    tell_new_content_view);
static void location_has_really_changed               (CajaWindowSlot         *slot);
static void update_for_new_location                   (CajaWindowSlot         *slot);

void
caja_window_report_selection_changed (CajaWindowInfo *window)
{
    if (window->details->temporarily_ignore_view_signals)
    {
        return;
    }

    g_signal_emit_by_name (window, "selection_changed");
}

/* set_displayed_location:
 */
static void
set_displayed_location (CajaWindowSlot *slot, GFile *location)
{
    gboolean recreate;

    if (slot->current_location_bookmark == NULL || location == NULL)
    {
        recreate = TRUE;
    }
    else
    {
        GFile *bookmark_location;

        bookmark_location = caja_bookmark_get_location (slot->current_location_bookmark);
        recreate = !g_file_equal (bookmark_location, location);
        g_object_unref (bookmark_location);
    }

    if (recreate)
    {
        char *name;

        /* We've changed locations, must recreate bookmark for current location. */
        if (slot->last_location_bookmark != NULL)
        {
            g_object_unref (slot->last_location_bookmark);
        }
        slot->last_location_bookmark = slot->current_location_bookmark;
        name = g_file_get_basename (location);
        slot->current_location_bookmark = (location == NULL) ? NULL
                                          : caja_bookmark_new (location, name, FALSE, NULL);
        g_free (name);
    }
}

static void
check_bookmark_location_matches (CajaBookmark *bookmark, GFile *location)
{
    GFile *bookmark_location;

    bookmark_location = caja_bookmark_get_location (bookmark);
    if (!g_file_equal (location, bookmark_location))
    {
        char *bookmark_uri, *uri;

        bookmark_uri = g_file_get_uri (bookmark_location);
        uri = g_file_get_uri (location);
        g_warning ("bookmark uri is %s, but expected %s", bookmark_uri, uri);
        g_free (uri);
        g_free (bookmark_uri);
    }
    g_object_unref (bookmark_location);
}

/* Debugging function used to verify that the last_location_bookmark
 * is in the state we expect when we're about to use it to update the
 * Back or Forward list.
 */
static void
check_last_bookmark_location_matches_slot (CajaWindowSlot *slot)
{
    check_bookmark_location_matches (slot->last_location_bookmark,
                                     slot->location);
}

static void
handle_go_back (CajaNavigationWindowSlot *navigation_slot,
                GFile *location)
{
    CajaWindowSlot *slot;
    guint i;
    GList *link;
    CajaBookmark *bookmark = NULL;

    slot = CAJA_WINDOW_SLOT (navigation_slot);

    /* Going back. Move items from the back list to the forward list. */
    g_assert (g_list_length (navigation_slot->back_list) > slot->location_change_distance);
    check_bookmark_location_matches (CAJA_BOOKMARK (g_list_nth_data (navigation_slot->back_list,
                                     slot->location_change_distance)),
                                     location);
    g_assert (slot->location != NULL);

    /* Move current location to Forward list */

    check_last_bookmark_location_matches_slot (slot);

    /* Use the first bookmark in the history list rather than creating a new one. */
    navigation_slot->forward_list = g_list_prepend (navigation_slot->forward_list,
                                    slot->last_location_bookmark);
    g_object_ref (navigation_slot->forward_list->data);

    /* Move extra links from Back to Forward list */
    for (i = 0; i < slot->location_change_distance; ++i)
    {
        bookmark = CAJA_BOOKMARK (navigation_slot->back_list->data);
        navigation_slot->back_list =
            g_list_remove (navigation_slot->back_list, bookmark);
        navigation_slot->forward_list =
            g_list_prepend (navigation_slot->forward_list, bookmark);
    }

    /* One bookmark falls out of back/forward lists and becomes viewed location */
    link = navigation_slot->back_list;
    navigation_slot->back_list = g_list_remove_link (navigation_slot->back_list, link);
    g_object_unref (link->data);
    g_list_free_1 (link);
}

static void
handle_go_forward (CajaNavigationWindowSlot *navigation_slot,
                   GFile *location)
{
    CajaWindowSlot *slot;
    guint i;
    GList *link;
    CajaBookmark *bookmark = NULL;

    slot = CAJA_WINDOW_SLOT (navigation_slot);

    /* Going forward. Move items from the forward list to the back list. */
    g_assert (g_list_length (navigation_slot->forward_list) > slot->location_change_distance);
    check_bookmark_location_matches (CAJA_BOOKMARK (g_list_nth_data (navigation_slot->forward_list,
                                     slot->location_change_distance)),
                                     location);
    g_assert (slot->location != NULL);

    /* Move current location to Back list */
    check_last_bookmark_location_matches_slot (slot);

    /* Use the first bookmark in the history list rather than creating a new one. */
    navigation_slot->back_list = g_list_prepend (navigation_slot->back_list,
                                 slot->last_location_bookmark);
    g_object_ref (navigation_slot->back_list->data);

    /* Move extra links from Forward to Back list */
    for (i = 0; i < slot->location_change_distance; ++i)
    {
        bookmark = CAJA_BOOKMARK (navigation_slot->forward_list->data);
        navigation_slot->forward_list =
            g_list_remove (navigation_slot->back_list, bookmark);
        navigation_slot->back_list =
            g_list_prepend (navigation_slot->forward_list, bookmark);
    }

    /* One bookmark falls out of back/forward lists and becomes viewed location */
    link = navigation_slot->forward_list;
    navigation_slot->forward_list = g_list_remove_link (navigation_slot->forward_list, link);
    g_object_unref (link->data);
    g_list_free_1 (link);
}

static void
handle_go_elsewhere (CajaWindowSlot *slot, GFile *location)
{
    if (CAJA_IS_NAVIGATION_WINDOW_SLOT (slot))
    {
        CajaNavigationWindowSlot *navigation_slot;

        navigation_slot = CAJA_NAVIGATION_WINDOW_SLOT (slot);

        /* Clobber the entire forward list, and move displayed location to back list */
        caja_navigation_window_slot_clear_forward_list (navigation_slot);

        if (slot->location != NULL)
        {
            /* If we're returning to the same uri somehow, don't put this uri on back list.
             * This also avoids a problem where set_displayed_location
             * didn't update last_location_bookmark since the uri didn't change.
             */
            if (!g_file_equal (slot->location, location))
            {
                /* Store bookmark for current location in back list, unless there is no current location */
                check_last_bookmark_location_matches_slot (slot);
                /* Use the first bookmark in the history list rather than creating a new one. */
                navigation_slot->back_list = g_list_prepend (navigation_slot->back_list,
                                             slot->last_location_bookmark);
                g_object_ref (navigation_slot->back_list->data);
            }
        }
    }
}

void
caja_window_update_up_button (CajaWindow *window)
{
    CajaWindowSlot *slot;
    gboolean allowed;
    GFile *parent;

    slot = window->details->active_pane->active_slot;

    allowed = FALSE;
    if (slot->location != NULL)
    {
        parent = g_file_get_parent (slot->location);
        allowed = parent != NULL;
        if (parent != NULL)
        {
            g_object_unref (parent);
        }
    }

    caja_window_allow_up (window, allowed);
}

static void
viewed_file_changed_callback (CajaFile *file,
                              CajaWindowSlot *slot)
{
    CajaWindow *window;
    GFile *new_location;
    gboolean is_in_trash, was_in_trash;

    window = slot->pane->window;

    g_assert (CAJA_IS_FILE (file));
    g_assert (CAJA_IS_WINDOW_PANE (slot->pane));
    g_assert (CAJA_IS_WINDOW (window));

    g_assert (file == slot->viewed_file);

    if (!caja_file_is_not_yet_confirmed (file))
    {
        slot->viewed_file_seen = TRUE;
    }

    was_in_trash = slot->viewed_file_in_trash;

    slot->viewed_file_in_trash = is_in_trash = caja_file_is_in_trash (file);

    /* Close window if the file it's viewing has been deleted or moved to trash. */
    if (caja_file_is_gone (file) || (is_in_trash && !was_in_trash))
    {
        /* Don't close the window in the case where the
         * file was never seen in the first place.
         */
        if (slot->viewed_file_seen)
        {
            /* Detecting a file is gone may happen in the
             * middle of a pending location change, we
             * need to cancel it before closing the window
             * or things break.
             */
            /* FIXME: It makes no sense that this call is
             * needed. When the window is destroyed, it
             * calls caja_window_manage_views_destroy,
             * which calls free_location_change, which
             * should be sufficient. Also, if this was
             * really needed, wouldn't it be needed for
             * all other caja_window_close callers?
             */
            end_location_change (slot);

            if (CAJA_IS_NAVIGATION_WINDOW (window))
            {
                /* auto-show existing parent. */
                GFile *go_to_file, *parent, *location;

                go_to_file = NULL;
                location =  caja_file_get_location (file);
                parent = g_file_get_parent (location);
                g_object_unref (location);
                if (parent)
                {
                    go_to_file = caja_find_existing_uri_in_hierarchy (parent);
                    g_object_unref (parent);
                }

                if (go_to_file != NULL)
                {
                    /* the path bar URI will be set to go_to_uri immediately
                     * in begin_location_change, but we don't want the
                     * inexistant children to show up anymore */
                    if (slot == slot->pane->active_slot)
                    {
                        /* multiview-TODO also update CajaWindowSlot
                         * [which as of writing doesn't save/store any path bar state]
                         */
                        caja_path_bar_clear_buttons (CAJA_PATH_BAR (CAJA_NAVIGATION_WINDOW_PANE (slot->pane)->path_bar));
                    }

                    caja_window_slot_go_to (slot, go_to_file, FALSE);
                    g_object_unref (go_to_file);
                }
                else
                {
                    caja_window_slot_go_home (slot, FALSE);
                }
            }
            else
            {
                caja_window_close (window);
            }
        }
    }
    else
    {
        new_location = caja_file_get_location (file);

        /* If the file was renamed, update location and/or
         * title. */
        if (!g_file_equal (new_location,
                           slot->location))
        {
            g_object_unref (slot->location);
            slot->location = new_location;
            if (slot == slot->pane->active_slot)
            {
                caja_window_pane_sync_location_widgets (slot->pane);
            }
        }
        else
        {
            /* TODO?
             *   why do we update title & icon at all in this case? */
            g_object_unref (new_location);
        }

        caja_window_slot_update_title (slot);
        caja_window_slot_update_icon (slot);
    }
}

static void
update_history (CajaWindowSlot *slot,
                CajaLocationChangeType type,
                GFile *new_location)
{
    switch (type)
    {
    case CAJA_LOCATION_CHANGE_STANDARD:
    case CAJA_LOCATION_CHANGE_FALLBACK:
        caja_window_slot_add_current_location_to_history_list (slot);
        handle_go_elsewhere (slot, new_location);
        return;
    case CAJA_LOCATION_CHANGE_RELOAD:
        /* for reload there is no work to do */
        return;
    case CAJA_LOCATION_CHANGE_BACK:
        caja_window_slot_add_current_location_to_history_list (slot);
        handle_go_back (CAJA_NAVIGATION_WINDOW_SLOT (slot), new_location);
        return;
    case CAJA_LOCATION_CHANGE_FORWARD:
        caja_window_slot_add_current_location_to_history_list (slot);
        handle_go_forward (CAJA_NAVIGATION_WINDOW_SLOT (slot), new_location);
        return;
    case CAJA_LOCATION_CHANGE_REDIRECT:
        /* for the redirect case, the caller can do the updating */
        return;
    }
    g_return_if_fail (FALSE);
}

static void
cancel_viewed_file_changed_callback (CajaWindowSlot *slot)
{
    CajaFile *file;

    file = slot->viewed_file;
    if (file != NULL)
    {
        g_signal_handlers_disconnect_by_func (G_OBJECT (file),
                                              G_CALLBACK (viewed_file_changed_callback),
                                              slot);
        caja_file_monitor_remove (file, &slot->viewed_file);
    }
}

static void
new_window_show_callback (GtkWidget *widget,
                          gpointer user_data)
{
    CajaWindow *window;

    window = CAJA_WINDOW (user_data);

    caja_window_close (window);

    g_signal_handlers_disconnect_by_func (widget,
                                          G_CALLBACK (new_window_show_callback),
                                          user_data);
}


void
caja_window_slot_open_location_full (CajaWindowSlot *slot,
                                     GFile *location,
                                     CajaWindowOpenMode mode,
                                     CajaWindowOpenFlags flags,
                                     GList *new_selection,
                                     CajaWindowGoToCallback callback,
                                     gpointer user_data)
{
    CajaWindow *window;
    CajaWindow *target_window;
    CajaWindowPane *pane;
    CajaWindowSlot *target_slot;
    CajaWindowOpenFlags slot_flags;
    gboolean existing = FALSE;
    GFile *old_location;
    char *old_uri, *new_uri;
    GList *l;
    gboolean target_navigation = FALSE;
    gboolean target_same = FALSE;
    gboolean is_desktop = FALSE;
    gboolean is_navigation = FALSE;

    window = slot->pane->window;

    target_window = NULL;
    target_slot = NULL;

    old_uri = caja_window_slot_get_location_uri (slot);
    if (old_uri == NULL)
    {
        old_uri = g_strdup ("(none)");
    }
    new_uri = g_file_get_uri (location);
    caja_debug_log (FALSE, CAJA_DEBUG_LOG_DOMAIN_USER,
                    "window %p open location: old=\"%s\", new=\"%s\"",
                    window,
                    old_uri,
                    new_uri);
    g_free (old_uri);
    g_free (new_uri);

    g_assert (!((flags & CAJA_WINDOW_OPEN_FLAG_NEW_WINDOW) != 0 &&
                (flags & CAJA_WINDOW_OPEN_FLAG_NEW_TAB) != 0));

    is_desktop = CAJA_IS_DESKTOP_WINDOW (window);
    is_navigation = CAJA_IS_NAVIGATION_WINDOW (window);
    target_same = is_desktop &&
    		!caja_desktop_window_loaded (CAJA_DESKTOP_WINDOW (window));

    old_location = caja_window_slot_get_location (slot);

    switch (mode)
    {
    case CAJA_WINDOW_OPEN_ACCORDING_TO_MODE :
        if (g_settings_get_boolean (caja_preferences, CAJA_PREFERENCES_ALWAYS_USE_BROWSER)) {
            /* always use browser: if we're on the desktop the target is a new navigation window,
            * otherwise it's the same window.
            */
            if (is_desktop) {
                new_uri = g_file_get_uri (location);
                if (g_str_has_prefix (new_uri, EEL_DESKTOP_URI))
                    target_same = TRUE;
                else
                    target_navigation = TRUE;
                g_free (new_uri);
            } else {
            	target_same = TRUE;
            }
        } else if (flags & CAJA_WINDOW_OPEN_FLAG_NEW_WINDOW) {
            /* if it's specified to open a new window, and we're not using spatial,
             * the target is a navigation.
             */
            target_navigation = TRUE;
        } else if (is_navigation) {
            target_same = TRUE;
        }
        break;
    case CAJA_WINDOW_OPEN_IN_NAVIGATION :
        target_navigation = TRUE;
        break;
    default :
        g_critical ("Unknown open location mode");
        g_object_unref (old_location);
        return;
    }

    /* now get/create the window according to the mode */
    if (target_same) {
        target_window = window;
    } else if (target_navigation) {
        target_window = caja_application_create_navigation_window
            (window->application,
             gtk_window_get_screen (GTK_WINDOW (window)));
    } else {
        target_window = caja_application_get_spatial_window
            (window->application,
             window,
             NULL,
             location,
             gtk_window_get_screen (GTK_WINDOW (window)),
             &existing);
    }

    /* if the spatial window is already showing, present it and set the
     * new selection, if present.
     */
    if (existing) {
        target_slot = target_window->details->active_pane->active_slot;

        gtk_window_present (GTK_WINDOW (target_window));

        if (new_selection != NULL && slot->content_view != NULL) {
            caja_view_set_selection (target_slot->content_view, new_selection);
        }

        /* call the callback successfully */
        if (callback != NULL) {
            callback (window, NULL, user_data);
        }

        return;
    }

    g_assert (target_window != NULL);

    if ((flags & CAJA_WINDOW_OPEN_FLAG_NEW_TAB) != 0 &&
            CAJA_IS_NAVIGATION_WINDOW (window))
    {
        g_assert (target_window == window);

        int new_slot_position;

        slot_flags = 0;

        new_slot_position = g_settings_get_enum (caja_preferences, CAJA_PREFERENCES_NEW_TAB_POSITION);
        if (new_slot_position == CAJA_NEW_TAB_POSITION_END)
        {
            slot_flags = CAJA_WINDOW_OPEN_SLOT_APPEND;
        }

        target_slot = caja_window_open_slot (window->details->active_pane, slot_flags);
    }

    if ((flags & CAJA_WINDOW_OPEN_FLAG_CLOSE_BEHIND) != 0)
    {
        if (CAJA_IS_SPATIAL_WINDOW (window) && !CAJA_IS_DESKTOP_WINDOW (window))
        {
            if (gtk_widget_get_visible (GTK_WIDGET (target_window)))
            {
                caja_window_close (window);
            }
            else
            {
                g_signal_connect_object (target_window,
                                         "show",
                                         G_CALLBACK (new_window_show_callback),
                                         window,
                                         G_CONNECT_AFTER);
            }
        }
    }

    if (target_slot == NULL)
    {
        if (target_window == window)
        {
            target_slot = slot;
        }
        else
        {
            target_slot = target_window->details->active_pane->active_slot;
        }
    }

    if (!(is_desktop && target_same) && (target_window == window && target_slot == slot &&
             old_location && g_file_equal (old_location, location))) {

        if (callback != NULL) {
            callback (window, NULL, user_data);
        }

	g_object_unref (old_location);
        return;
    }

    begin_location_change (target_slot, location, old_location, new_selection,
                           (is_desktop && target_same) ? CAJA_LOCATION_CHANGE_RELOAD : CAJA_LOCATION_CHANGE_STANDARD, 0, NULL, callback, user_data);

    /* Additionally, load this in all slots that have no location, this means
       we load both panes in e.g. a newly opened dual pane window. */
    for (l = target_window->details->panes; l != NULL; l = l->next)
    {
        pane = l->data;
        slot = pane->active_slot;
        if (slot->location == NULL && slot->pending_location == NULL) {
            begin_location_change (slot, location, old_location, new_selection,
                                   CAJA_LOCATION_CHANGE_STANDARD, 0, NULL, NULL, NULL);
        }
    }

    if (old_location)
    {
        g_object_unref (old_location);
    }
}

void
caja_window_slot_open_location (CajaWindowSlot *slot,
                                GFile *location,
                                gboolean close_behind)
{
    CajaWindowOpenFlags flags;

    flags = 0;
    if (close_behind)
    {
        flags = CAJA_WINDOW_OPEN_FLAG_CLOSE_BEHIND;
    }

    caja_window_slot_open_location_full (slot, location,
                                         CAJA_WINDOW_OPEN_ACCORDING_TO_MODE,
                                         flags, NULL, NULL, NULL);
}

void
caja_window_slot_open_location_with_selection (CajaWindowSlot *slot,
        GFile *location,
        GList *selection,
        gboolean close_behind)
{
    CajaWindowOpenFlags flags;

    flags = 0;
    if (close_behind)
    {
        flags = CAJA_WINDOW_OPEN_FLAG_CLOSE_BEHIND;
    }
    caja_window_slot_open_location_full (slot, location,
                                         CAJA_WINDOW_OPEN_ACCORDING_TO_MODE,
                                         flags, selection, NULL, NULL);
}


void
caja_window_slot_go_home (CajaWindowSlot *slot, gboolean new_tab)
{
    GFile *home;
    CajaWindowOpenFlags flags;

    g_return_if_fail (CAJA_IS_WINDOW_SLOT (slot));

    if (new_tab)
    {
        flags = CAJA_WINDOW_OPEN_FLAG_NEW_TAB;
    }
    else
    {
        flags = 0;
    }

    home = g_file_new_for_path (g_get_home_dir ());
    caja_window_slot_open_location_full (slot, home,
                                         CAJA_WINDOW_OPEN_ACCORDING_TO_MODE,
                                         flags, NULL, NULL, NULL);
    g_object_unref (home);
}

static char *
caja_window_slot_get_view_error_label (CajaWindowSlot *slot)
{
    const CajaViewInfo *info;

    info = caja_view_factory_lookup (caja_window_slot_get_content_view_id (slot));

    return g_strdup (info->error_label);
}

static char *
caja_window_slot_get_view_startup_error_label (CajaWindowSlot *slot)
{
    const CajaViewInfo *info;

    info = caja_view_factory_lookup (caja_window_slot_get_content_view_id (slot));

    return g_strdup (info->startup_error_label);
}

static void
report_current_content_view_failure_to_user (CajaWindowSlot *slot)
{
    CajaWindow *window;
    char *message;

    window = slot->pane->window;

    message = caja_window_slot_get_view_startup_error_label (slot);
    eel_show_error_dialog (message,
                           _("You can choose another view or go to a different location."),
                           GTK_WINDOW (window));
    g_free (message);
}

static void
report_nascent_content_view_failure_to_user (CajaWindowSlot *slot,
        CajaView *view)
{
    CajaWindow *window;
    char *message;

    window = slot->pane->window;

    /* TODO? why are we using the current view's error label here, instead of the next view's?
     * This behavior has already been present in pre-slot days.
     */
    message = caja_window_slot_get_view_error_label (slot);
    eel_show_error_dialog (message,
                           _("The location cannot be displayed with this viewer."),
                           GTK_WINDOW (window));
    g_free (message);
}


const char *
caja_window_slot_get_content_view_id (CajaWindowSlot *slot)
{
    if (slot->content_view == NULL)
    {
        return NULL;
    }
    return caja_view_get_view_id (slot->content_view);
}

gboolean
caja_window_slot_content_view_matches_iid (CajaWindowSlot *slot,
        const char *iid)
{
    if (slot->content_view == NULL)
    {
        return FALSE;
    }
    return g_strcmp0 (caja_view_get_view_id (slot->content_view), iid) == 0;
}

static gboolean
report_callback (CajaWindowSlot *slot,
                 GError *error)
{
    if (slot->open_callback != NULL) {
        slot->open_callback (slot->pane->window, error, slot->open_callback_user_data);
        slot->open_callback = NULL;
        slot->open_callback_user_data = NULL;

        return TRUE;
    }

    return FALSE;
}

/*
 * begin_location_change
 *
 * Change a window's location.
 * @window: The CajaWindow whose location should be changed.
 * @location: A url specifying the location to load
 * @previous_location: The url that was previously shown in the window that initialized the change, if any
 * @new_selection: The initial selection to present after loading the location
 * @type: Which type of location change is this? Standard, back, forward, or reload?
 * @distance: If type is back or forward, the index into the back or forward chain. If
 * type is standard or reload, this is ignored, and must be 0.
 * @scroll_pos: The file to scroll to when the location is loaded.
 * @callback: function to be called when the location is changed.
 * @user_data: data for @callback.
 *
 * This is the core function for changing the location of a window. Every change to the
 * location begins here.
 */
static void
begin_location_change (CajaWindowSlot *slot,
                       GFile *location,
                       GFile *previous_location,
                       GList *new_selection,
                       CajaLocationChangeType type,
                       guint distance,
                       const char *scroll_pos,
                       CajaWindowGoToCallback callback,
                       gpointer user_data)
{
    CajaWindow *window;
    CajaDirectory *directory;
    gboolean force_reload;
    GFile *parent;

    g_assert (slot != NULL);
    g_assert (location != NULL);
    g_assert (type == CAJA_LOCATION_CHANGE_BACK
              || type == CAJA_LOCATION_CHANGE_FORWARD
              || distance == 0);

    /* If there is no new selection and the new location is
     * a (grand)parent of the old location then we automatically
     * select the folder the previous location was in */
    if (new_selection == NULL && previous_location != NULL &&
        g_file_has_prefix (previous_location, location)) {
        GFile *from_folder;

        from_folder = g_object_ref (previous_location);
        parent = g_file_get_parent (from_folder);
        while (parent != NULL && !g_file_equal (parent, location)) {
            g_object_unref (from_folder);
            from_folder = parent;
            parent = g_file_get_parent (from_folder);
        }
        if (parent != NULL) {
            new_selection = g_list_prepend (NULL, g_object_ref(from_folder));
        }
        g_object_unref (from_folder);
        g_object_unref (parent);
    }

    window = slot->pane->window;
    g_assert (CAJA_IS_WINDOW (window));
    g_object_ref (window);

    end_location_change (slot);

    caja_window_slot_set_allow_stop (slot, TRUE);
    caja_window_slot_set_status (slot, " ");

    g_assert (slot->pending_location == NULL);
    g_assert (slot->pending_selection == NULL);

    slot->pending_location = g_object_ref (location);
    slot->location_change_type = type;
    slot->location_change_distance = distance;
    slot->tried_mount = FALSE;
    slot->pending_selection = g_list_copy_deep (new_selection, (GCopyFunc) g_object_ref, NULL);

    slot->pending_scroll_to = g_strdup (scroll_pos);

    slot->open_callback = callback;
    slot->open_callback_user_data = user_data;

    directory = caja_directory_get (location);

    /* The code to force a reload is here because if we do it
     * after determining an initial view (in the components), then
     * we end up fetching things twice.
     */
    if (type == CAJA_LOCATION_CHANGE_RELOAD)
    {
        force_reload = TRUE;
    }
    else if (!caja_monitor_active ())
    {
        force_reload = TRUE;
    }
    else
    {
        force_reload = !caja_directory_is_local (directory);
    }

    if (force_reload)
    {
        CajaFile *file;

        caja_directory_force_reload (directory);
        file = caja_directory_get_corresponding_file (directory);
        caja_file_invalidate_all_attributes (file);
        caja_file_unref (file);
    }

    caja_directory_unref (directory);

    /* Set current_bookmark scroll pos */
    if (slot->current_location_bookmark != NULL &&
            slot->content_view != NULL)
    {
        char *current_pos;

        current_pos = caja_view_get_first_visible_file (slot->content_view);
        caja_bookmark_set_scroll_pos (slot->current_location_bookmark, current_pos);
        g_free (current_pos);
    }

    /* Get the info needed for view selection */

    slot->determine_view_file = caja_file_get (location);
    g_assert (slot->determine_view_file != NULL);

    /* if the currently viewed file is marked gone while loading the new location,
     * this ensures that the window isn't destroyed */
    cancel_viewed_file_changed_callback (slot);

    caja_file_call_when_ready (slot->determine_view_file,
                               CAJA_FILE_ATTRIBUTE_INFO |
                               CAJA_FILE_ATTRIBUTE_MOUNT,
                               got_file_info_for_view_selection_callback,
                               slot);

    g_object_unref (window);
}

static void
setup_new_spatial_window (CajaWindowSlot *slot, CajaFile *file)
{
    CajaWindow *window;
    char *scroll_string;
    gboolean maximized, sticky, above;

    window = slot->pane->window;

    if (CAJA_IS_SPATIAL_WINDOW (window) && !CAJA_IS_DESKTOP_WINDOW (window))
    {
        char *show_hidden_file_setting;
        char *geometry_string;

        /* load show hidden state */
        show_hidden_file_setting = caja_file_get_metadata
                                   (file, CAJA_METADATA_KEY_WINDOW_SHOW_HIDDEN_FILES,
                                    NULL);
        if (show_hidden_file_setting != NULL)
        {
            GtkAction *action;

            if (strcmp (show_hidden_file_setting, "1") == 0)
            {
                window->details->show_hidden_files_mode = CAJA_WINDOW_SHOW_HIDDEN_FILES_ENABLE;
            }
            else
            {
                window->details->show_hidden_files_mode = CAJA_WINDOW_SHOW_HIDDEN_FILES_DISABLE;
            }

            G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
            /* Update the UI, since we initialize it to the default */
            action = gtk_action_group_get_action (window->details->main_action_group, CAJA_ACTION_SHOW_HIDDEN_FILES);
            gtk_action_block_activate (action);
            gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
                                          window->details->show_hidden_files_mode == CAJA_WINDOW_SHOW_HIDDEN_FILES_ENABLE);
            gtk_action_unblock_activate (action);
            G_GNUC_END_IGNORE_DEPRECATIONS;
        }
        else
        {
            CAJA_WINDOW (window)->details->show_hidden_files_mode = CAJA_WINDOW_SHOW_HIDDEN_FILES_DEFAULT;
        }
        g_free (show_hidden_file_setting);

        /* load the saved window geometry */
        maximized = caja_file_get_boolean_metadata
                    (file, CAJA_METADATA_KEY_WINDOW_MAXIMIZED, FALSE);
        if (maximized)
        {
            gtk_window_maximize (GTK_WINDOW (window));
        }
        else
        {
            gtk_window_unmaximize (GTK_WINDOW (window));
        }

        sticky = caja_file_get_boolean_metadata
                 (file, CAJA_METADATA_KEY_WINDOW_STICKY, FALSE);
        if (sticky)
        {
            gtk_window_stick (GTK_WINDOW (window));
        }
        else
        {
            gtk_window_unstick (GTK_WINDOW (window));
        }

        above = caja_file_get_boolean_metadata
                (file, CAJA_METADATA_KEY_WINDOW_KEEP_ABOVE, FALSE);
        if (above)
        {
            gtk_window_set_keep_above (GTK_WINDOW (window), TRUE);
        }
        else
        {
            gtk_window_set_keep_above (GTK_WINDOW (window), FALSE);
        }

        geometry_string = caja_file_get_metadata
                          (file, CAJA_METADATA_KEY_WINDOW_GEOMETRY, NULL);
        if (geometry_string != NULL)
        {
            eel_gtk_window_set_initial_geometry_from_string
            (GTK_WINDOW (window),
             geometry_string,
             CAJA_SPATIAL_WINDOW_MIN_WIDTH,
             CAJA_SPATIAL_WINDOW_MIN_HEIGHT,
             FALSE);
        }
        g_free (geometry_string);

        if (slot->pending_selection == NULL)
        {
            /* If there is no pending selection, then load the saved scroll position. */
            scroll_string = caja_file_get_metadata
                            (file, CAJA_METADATA_KEY_WINDOW_SCROLL_POSITION,
                             NULL);
        }
        else
        {
            /* If there is a pending selection, we want to scroll to an item in
             * the pending selection list. */
            scroll_string = g_file_get_uri (slot->pending_selection->data);
        }

        /* scroll_string might be NULL if there was no saved scroll position. */
        if (scroll_string != NULL)
        {
            slot->pending_scroll_to = scroll_string;
        }
    }
}

typedef struct
{
    GCancellable *cancellable;
    CajaWindowSlot *slot;
} MountNotMountedData;

static void
mount_not_mounted_callback (GObject *source_object,
                            GAsyncResult *res,
                            gpointer user_data)
{
    MountNotMountedData *data;
    CajaWindowSlot *slot;
    GError *error;
    GCancellable *cancellable;

    data = user_data;
    slot = data->slot;
    cancellable = data->cancellable;
    g_free (data);

    if (g_cancellable_is_cancelled (cancellable))
    {
        /* Cancelled, don't call back */
        g_object_unref (cancellable);
        return;
    }

    slot->mount_cancellable = NULL;

    slot->determine_view_file = caja_file_get (slot->pending_location);

    error = NULL;
    if (!g_file_mount_enclosing_volume_finish (G_FILE (source_object), res, &error))
    {
        slot->mount_error = error;
        got_file_info_for_view_selection_callback (slot->determine_view_file, slot);
        slot->mount_error = NULL;
        g_error_free (error);
    }
    else
    {
        caja_file_invalidate_all_attributes (slot->determine_view_file);
        caja_file_call_when_ready (slot->determine_view_file,
                                   CAJA_FILE_ATTRIBUTE_INFO,
                                   got_file_info_for_view_selection_callback,
                                   slot);
    }

    g_object_unref (cancellable);
}

static void
got_file_info_for_view_selection_callback (CajaFile *file,
        gpointer callback_data)
{
    GError *error;
    char *view_id;
    CajaWindow *window;
    CajaWindowSlot *slot;
    GFile *location;
    MountNotMountedData *data;
    slot = callback_data;
    g_assert (CAJA_IS_WINDOW_SLOT (slot));
    g_assert (slot->determine_view_file == file);

    window = slot->pane->window;
    g_assert (CAJA_IS_WINDOW (window));

    slot->determine_view_file = NULL;

    if (slot->mount_error)
    {
        error = slot->mount_error;
    }
    else
    {
        error = caja_file_get_file_info_error (file);
    }

    if (error && error->domain == G_IO_ERROR && error->code == G_IO_ERROR_NOT_MOUNTED &&
            !slot->tried_mount)
    {
        GMountOperation *mount_op;

        slot->tried_mount = TRUE;

        mount_op = gtk_mount_operation_new (GTK_WINDOW (window));
        g_mount_operation_set_password_save (mount_op, G_PASSWORD_SAVE_FOR_SESSION);
        location = caja_file_get_location (file);
        data = g_new0 (MountNotMountedData, 1);
        data->cancellable = g_cancellable_new ();
        data->slot = slot;
        slot->mount_cancellable = data->cancellable;
        g_file_mount_enclosing_volume (location, 0, mount_op, slot->mount_cancellable,
                                       mount_not_mounted_callback, data);
        g_object_unref (location);
        g_object_unref (mount_op);

        caja_file_unref (file);

        return;
    }

    location = slot->pending_location;

    view_id = NULL;

    if (error == NULL ||
            (error->domain == G_IO_ERROR && error->code == G_IO_ERROR_NOT_SUPPORTED))
    {
        char *mimetype;

        /* We got the information we need, now pick what view to use: */

        mimetype = caja_file_get_mime_type (file);

        /* If fallback, don't use view from metadata */
        if (slot->location_change_type != CAJA_LOCATION_CHANGE_FALLBACK)
        {
            /* Look in metadata for view */
            view_id = caja_file_get_metadata
                      (file, CAJA_METADATA_KEY_DEFAULT_VIEW, NULL);
            if (view_id != NULL &&
                    !caja_view_factory_view_supports_uri (view_id,
                            location,
                            caja_file_get_file_type (file),
                            mimetype))
            {
                g_free (view_id);
                view_id = NULL;
            }
        }

        /* Otherwise, use default */
        if (view_id == NULL)
        {
            char *uri;
            uri = caja_file_get_uri (file);

            /* Use same view settings for search results as the current folder */
            if (eel_uri_is_search (uri))
            {
                view_id = g_strdup (caja_view_get_view_id (slot->content_view));
            }
            else
            {
                view_id = caja_global_preferences_get_default_folder_viewer_preference_as_iid ();
            }

            g_free (uri);

            if (view_id != NULL &&
                    !caja_view_factory_view_supports_uri (view_id,
                            location,
                            caja_file_get_file_type (file),
                            mimetype))
            {
                g_free (view_id);
                view_id = NULL;
            }
        }

        g_free (mimetype);
    }

    if (view_id != NULL)
    {
        if (!gtk_widget_get_visible (GTK_WIDGET (window)) && CAJA_IS_SPATIAL_WINDOW (window))
        {
            /* We now have the metadata to set up the window position, etc */
            setup_new_spatial_window (slot, file);
        }
        create_content_view (slot, view_id);
        g_free (view_id);

        report_callback (slot, NULL);
    }
    else
    {
        if (!report_callback (slot, error)) {
            display_view_selection_failure (window, file,
                                            location, error);
        }

        if (!gtk_widget_get_visible (GTK_WIDGET (window)))
        {
            CajaApplication *app;

            /* Destroy never-had-a-chance-to-be-seen window. This case
             * happens when a new window cannot display its initial URI.
             */
            /* if this is the only window, we don't want to quit, so we redirect it to home */
            app = CAJA_APPLICATION (g_application_get_default ());

            if (g_list_length (gtk_application_get_windows (GTK_APPLICATION (app))) == 1) {

                /* the user could have typed in a home directory that doesn't exist,
                   in which case going home would cause an infinite loop, so we
                   better test for that */

                if (!caja_is_root_directory (location))
                {
                    if (!caja_is_home_directory (location))
                    {
                        caja_window_slot_go_home (slot, FALSE);
                    }
                    else
                    {
                        GFile *root;

                        root = g_file_new_for_path ("/");
                        /* the last fallback is to go to a known place that can't be deleted! */
                        caja_window_slot_go_to (slot, location, FALSE);
                        g_object_unref (root);
                    }
                }
                else
                {
                    gtk_widget_destroy (GTK_WIDGET (window));
                }
            }
            else
            {
                /* Since this is a window, destroying it will also unref it. */
                gtk_widget_destroy (GTK_WIDGET (window));
            }
        }
        else
        {
            /* Clean up state of already-showing window */
            end_location_change (slot);

            /* TODO? shouldn't we call
             *   cancel_viewed_file_changed_callback (slot);
             * at this point, or in end_location_change()
             */
            /* We're missing a previous location (if opened location
             * in a new tab) so close it and return */
            if (slot->location == NULL)
            {
                caja_window_slot_close (slot);
            }
            else
            {
                CajaFile *viewed_file;

                /* We disconnected this, so we need to re-connect it */
                viewed_file = caja_file_get (slot->location);
                caja_window_slot_set_viewed_file (slot, viewed_file);
                caja_file_monitor_add (viewed_file, &slot->viewed_file, 0);
                g_signal_connect_object (viewed_file, "changed",
                                         G_CALLBACK (viewed_file_changed_callback), slot, 0);
                caja_file_unref (viewed_file);

                /* Leave the location bar showing the bad location that the user
                 * typed (or maybe achieved by dragging or something). Many times
                 * the mistake will just be an easily-correctable typo. The user
                 * can choose "Refresh" to get the original URI back in the location bar.
                 */
            }
        }
    }

    caja_file_unref (file);
}

/* Load a view into the window, either reusing the old one or creating
 * a new one. This happens when you want to load a new location, or just
 * switch to a different view.
 * If pending_location is set we're loading a new location and
 * pending_location/selection will be used. If not, we're just switching
 * view, and the current location will be used.
 */
static void
create_content_view (CajaWindowSlot *slot,
                     const char *view_id)
{
    CajaWindow *window;
    CajaView *view;
    GList *selection;

    window = slot->pane->window;

    /* FIXME bugzilla.gnome.org 41243:
     * We should use inheritance instead of these special cases
     * for the desktop window.
     */
    if (CAJA_IS_DESKTOP_WINDOW (window))
    {
        /* We force the desktop to use a desktop_icon_view. It's simpler
         * to fix it here than trying to make it pick the right view in
         * the first place.
         */
        view_id = CAJA_DESKTOP_ICON_VIEW_IID;
    }

    if (slot->content_view != NULL &&
            g_strcmp0 (caja_view_get_view_id (slot->content_view),
                        view_id) == 0)
    {
        /* reuse existing content view */
        view = slot->content_view;
        slot->new_content_view = view;
        g_object_ref (view);
    }
    else
    {
        /* create a new content view */
        view = caja_view_factory_create (view_id,
                                         CAJA_WINDOW_SLOT_INFO (slot));

        eel_accessibility_set_name (view, _("Content View"));
        eel_accessibility_set_description (view, _("View of the current folder"));

        slot->new_content_view = view;
        caja_window_slot_connect_content_view (slot, slot->new_content_view);
    }

    /* Actually load the pending location and selection: */

    if (slot->pending_location != NULL)
    {
        load_new_location (slot,
                           slot->pending_location,
                           slot->pending_selection,
                           FALSE,
                           TRUE);

    	g_list_free_full (slot->pending_selection, g_object_unref);
        slot->pending_selection = NULL;
    }
    else if (slot->location != NULL)
    {
        selection = caja_view_get_selection (slot->content_view);
        load_new_location (slot,
                           slot->location,
                           selection,
                           FALSE,
                           TRUE);
    	g_list_free_full (selection, g_object_unref);
    }
    else
    {
        /* Something is busted, there was no location to load.
           Just load the homedir. */
        caja_window_slot_go_home (slot, FALSE);

    }
}

static void
load_new_location (CajaWindowSlot *slot,
                   GFile *location,
                   GList *selection,
                   gboolean tell_current_content_view,
                   gboolean tell_new_content_view)
{
    CajaWindow *window;
    GList *selection_copy;
    CajaView *view;
    char *uri;

    g_assert (slot != NULL);
    g_assert (location != NULL);

    window = slot->pane->window;
    g_assert (CAJA_IS_WINDOW (window));

    selection_copy = g_list_copy_deep (selection, (GCopyFunc) g_object_ref, NULL);

    view = NULL;

    /* Note, these may recurse into report_load_underway */
    if (slot->content_view != NULL && tell_current_content_view)
    {
        view = slot->content_view;
        uri = g_file_get_uri (location);
        caja_view_load_location (slot->content_view, uri);
        g_free (uri);
    }

    if (slot->new_content_view != NULL && tell_new_content_view &&
            (!tell_current_content_view ||
             slot->new_content_view != slot->content_view) )
    {
        view = slot->new_content_view;
        uri = g_file_get_uri (location);
        caja_view_load_location (slot->new_content_view, uri);
        g_free (uri);
    }
    if (view != NULL)
    {
        /* slot->new_content_view might have changed here if
           report_load_underway was called from load_location */
        caja_view_set_selection (view, selection_copy);
    }

    g_list_free_full (selection_copy, g_object_unref);
}

/* A view started to load the location its viewing, either due to
 * a load_location request, or some internal reason. Expect
 * a matching load_compete later
 */
void
caja_window_report_load_underway (CajaWindow *window,
                                  CajaView *view)
{
    CajaWindowSlot *slot;

    g_assert (CAJA_IS_WINDOW (window));

    if (window->details->temporarily_ignore_view_signals)
    {
        return;
    }

    slot = caja_window_get_slot_for_view (window, view);
    g_assert (slot != NULL);

    if (view == slot->new_content_view)
    {
        location_has_really_changed (slot);
    }
    else
    {
        caja_window_slot_set_allow_stop (slot, TRUE);
    }
}

static void
caja_window_emit_location_change (CajaWindow *window,
                                  GFile *location)
{
    char *uri;

    uri = g_file_get_uri (location);
    g_signal_emit_by_name (window, "loading_uri", uri);
    g_free (uri);
}

/* reports location change to window's "loading-uri" clients, i.e.
 * sidebar panels [used when switching tabs]. It will emit the pending
 * location, or the existing location if none is pending.
 */
void
caja_window_report_location_change (CajaWindow *window)
{
    CajaWindowSlot *slot;
    GFile *location;

    g_assert (CAJA_IS_WINDOW (window));

    slot = window->details->active_pane->active_slot;
    g_assert (CAJA_IS_WINDOW_SLOT (slot));

    location = NULL;

    if (slot->pending_location != NULL)
    {
        location = slot->pending_location;
    }

    if (location == NULL && slot->location != NULL)
    {
        location = slot->location;
    }

    if (location != NULL)
    {
        caja_window_emit_location_change (window, location);
    }
}

/* This is called when we have decided we can actually change to the new view/location situation. */
static void
location_has_really_changed (CajaWindowSlot *slot)
{
    CajaWindow *window;
    GFile *location_copy;

    window = slot->pane->window;

    if (slot->new_content_view != NULL)
    {
        GtkWidget *widget;

        widget = caja_view_get_widget (slot->new_content_view);
        /* Switch to the new content view. */
        if (gtk_widget_get_parent (widget) == NULL)
        {
            if (slot->content_view != NULL)
            {
                caja_window_slot_disconnect_content_view (slot, slot->content_view);
            }
            caja_window_slot_set_content_view_widget (slot, slot->new_content_view);
        }
        g_object_unref (slot->new_content_view);
        slot->new_content_view = NULL;
    }

    if (slot->pending_location != NULL)
    {
        /* Tell the window we are finished. */
        update_for_new_location (slot);
    }

    location_copy = NULL;
    if (slot->location != NULL)
    {
        location_copy = g_object_ref (slot->location);
    }

    free_location_change (slot);

    if (location_copy != NULL)
    {
        if (slot == caja_window_get_active_slot (window))
        {
            caja_window_emit_location_change (window, location_copy);
        }

        g_object_unref (location_copy);
    }
}

static void
slot_add_extension_extra_widgets (CajaWindowSlot *slot)
{
    GList *providers, *l;
    char *uri;
    GtkWidget *widget = NULL;

    providers = caja_extensions_get_for_type (CAJA_TYPE_LOCATION_WIDGET_PROVIDER);

    uri = g_file_get_uri (slot->location);
    for (l = providers; l != NULL; l = l->next)
    {
        CajaLocationWidgetProvider *provider;

        provider = CAJA_LOCATION_WIDGET_PROVIDER (l->data);
        widget = caja_location_widget_provider_get_widget (provider, uri, GTK_WIDGET (slot->pane->window));
        if (widget != NULL)
        {
            caja_window_slot_add_extra_location_widget (slot, widget);
        }
    }
    g_free (uri);

    caja_module_extension_list_free (providers);
}

static void
caja_window_slot_show_x_content_bar (CajaWindowSlot *slot, GMount *mount, const char **x_content_types)
{
    unsigned int n;

    g_assert (CAJA_IS_WINDOW_SLOT (slot));

    for (n = 0; x_content_types[n] != NULL; n++)
    {
        GAppInfo *default_app;

        /* skip blank media; the burn:/// location will provide it's own cluebar */
        if (g_str_has_prefix (x_content_types[n], "x-content/blank-"))
        {
            continue;
        }

        /* don't show the cluebar for windows software */
        if (g_content_type_is_a (x_content_types[n], "x-content/win32-software"))
        {
            continue;
        }

        /* only show the cluebar if a default app is available */
        default_app = g_app_info_get_default_for_type (x_content_types[n], FALSE);
        if (default_app != NULL)
        {
            GtkWidget *bar;
            bar = caja_x_content_bar_new (mount, x_content_types[n]);
            gtk_widget_show (bar);
            caja_window_slot_add_extra_location_widget (slot, bar);
            g_object_unref (default_app);
        }
    }
}

static void
caja_window_slot_show_trash_bar (CajaWindowSlot *slot,
                                 CajaWindow *window)
{
    GtkWidget *bar;

    bar = caja_trash_bar_new (window);
    gtk_widget_show (bar);

    caja_window_slot_add_extra_location_widget (slot, bar);
}

typedef struct
{
    CajaWindowSlot *slot;
    GCancellable *cancellable;
    GMount *mount;
} FindMountData;

static void
found_content_type_cb (const char **x_content_types, FindMountData *data)
{
    CajaWindowSlot *slot;

    if (g_cancellable_is_cancelled (data->cancellable))
    {
        goto out;
    }

    slot = data->slot;

    if (x_content_types != NULL && x_content_types[0] != NULL)
    {
        caja_window_slot_show_x_content_bar (slot, data->mount, x_content_types);
    }

    slot->find_mount_cancellable = NULL;

out:
    g_object_unref (data->mount);
    g_object_unref (data->cancellable);
    g_free (data);
}

static void
found_mount_cb (GObject *source_object,
                GAsyncResult *res,
                gpointer user_data)
{
    FindMountData *data = user_data;
    GMount *mount;

    if (g_cancellable_is_cancelled (data->cancellable))
    {
        goto out;
    }

    mount = g_file_find_enclosing_mount_finish (G_FILE (source_object),
            res,
            NULL);
    if (mount != NULL)
    {
        data->mount = mount;
        caja_autorun_get_x_content_types_for_mount_async (mount,
                (CajaAutorunGetContent)found_content_type_cb,
                data->cancellable,
                data);
        return;
    }

    data->slot->find_mount_cancellable = NULL;

out:
    g_object_unref (data->cancellable);
    g_free (data);
}

/* Handle the changes for the CajaWindow itself. */
static void
update_for_new_location (CajaWindowSlot *slot)
{
    CajaWindow *window;
    GFile *new_location;
    CajaFile *file;
    gboolean location_really_changed;
    FindMountData *data;

    window = slot->pane->window;

    new_location = slot->pending_location;
    slot->pending_location = NULL;

    set_displayed_location (slot, new_location);

    update_history (slot, slot->location_change_type, new_location);

    location_really_changed =
        slot->location == NULL ||
        !g_file_equal (slot->location, new_location);

    /* Set the new location. */
    if (slot->location)
    {
        g_object_unref (slot->location);
    }
    slot->location = new_location;

    /* Create a CajaFile for this location, so we can catch it
     * if it goes away.
     */
    cancel_viewed_file_changed_callback (slot);
    file = caja_file_get (slot->location);
    caja_window_slot_set_viewed_file (slot, file);
    slot->viewed_file_seen = !caja_file_is_not_yet_confirmed (file);
    slot->viewed_file_in_trash = caja_file_is_in_trash (file);
    caja_file_monitor_add (file, &slot->viewed_file, 0);
    g_signal_connect_object (file, "changed",
                             G_CALLBACK (viewed_file_changed_callback), slot, 0);
    caja_file_unref (file);

    if (slot == window->details->active_pane->active_slot)
    {
        /* Check if we can go up. */
        caja_window_update_up_button (window);

        caja_window_sync_zoom_widgets (window);

        /* Set up the content view menu for this new location. */
        caja_window_load_view_as_menus (window);

        /* Load menus from caja extensions for this location */
        caja_window_load_extension_menus (window);
    }

    if (location_really_changed)
    {
        CajaDirectory *directory;

        caja_window_slot_remove_extra_location_widgets (slot);

        directory = caja_directory_get (slot->location);

        caja_window_slot_update_query_editor (slot);

        if (caja_directory_is_in_trash (directory))
        {
            caja_window_slot_show_trash_bar (slot, window);
        }

        /* need the mount to determine if we should put up the x-content cluebar */
        if (slot->find_mount_cancellable != NULL)
        {
            g_cancellable_cancel (slot->find_mount_cancellable);
            slot->find_mount_cancellable = NULL;
        }

        data = g_new (FindMountData, 1);
        data->slot = slot;
        data->cancellable = g_cancellable_new ();
        data->mount = NULL;

        slot->find_mount_cancellable = data->cancellable;
        g_file_find_enclosing_mount_async (slot->location,
                                           G_PRIORITY_DEFAULT,
                                           data->cancellable,
                                           found_mount_cb,
                                           data);

        caja_directory_unref (directory);

        slot_add_extension_extra_widgets (slot);
    }

    caja_window_slot_update_title (slot);
    caja_window_slot_update_icon (slot);

    if (slot == slot->pane->active_slot)
    {
        caja_window_pane_sync_location_widgets (slot->pane);

        if (location_really_changed)
        {
            caja_window_pane_sync_search_widgets (slot->pane);
        }

        if (CAJA_IS_NAVIGATION_WINDOW (window) &&
                slot->pane == window->details->active_pane)
        {
            caja_navigation_window_load_extension_toolbar_items (CAJA_NAVIGATION_WINDOW (window));
        }
    }
}

/* A location load previously announced by load_underway
 * has been finished */
void
caja_window_report_load_complete (CajaWindow *window,
                                  CajaView *view)
{
    CajaWindowSlot *slot;

    g_assert (CAJA_IS_WINDOW (window));

    if (window->details->temporarily_ignore_view_signals)
    {
        return;
    }

    slot = caja_window_get_slot_for_view (window, view);
    g_assert (slot != NULL);

    /* Only handle this if we're expecting it.
     * Don't handle it if its from an old view we've switched from */
    if (view == slot->content_view)
    {
        if (slot->pending_scroll_to != NULL)
        {
            caja_view_scroll_to_file (slot->content_view,
                                      slot->pending_scroll_to);
        }
        end_location_change (slot);
    }
}

static void
end_location_change (CajaWindowSlot *slot)
{
    CajaWindow *window;
    char *uri;

    window = slot->pane->window;

    uri = caja_window_slot_get_location_uri (slot);
    if (uri)
    {
        caja_debug_log (FALSE, CAJA_DEBUG_LOG_DOMAIN_USER,
                        "finished loading window %p: %s", window, uri);
        g_free (uri);
    }

    caja_window_slot_set_allow_stop (slot, FALSE);

    /* Now we can free pending_scroll_to, since the load_complete
     * callback already has been emitted.
     */
    g_free (slot->pending_scroll_to);
    slot->pending_scroll_to = NULL;

    free_location_change (slot);
}

static void
free_location_change (CajaWindowSlot *slot)
{
    CajaWindow *window;

    window = slot->pane->window;
    g_assert (CAJA_IS_WINDOW (window));

    if (slot->pending_location)
    {
        g_object_unref (slot->pending_location);
    }
    slot->pending_location = NULL;

    g_list_free_full (slot->pending_selection, g_object_unref);
    slot->pending_selection = NULL;

    /* Don't free pending_scroll_to, since thats needed until
     * the load_complete callback.
     */

    if (slot->mount_cancellable != NULL)
    {
        g_cancellable_cancel (slot->mount_cancellable);
        slot->mount_cancellable = NULL;
    }

    if (slot->determine_view_file != NULL)
    {
        caja_file_cancel_call_when_ready
        (slot->determine_view_file,
         got_file_info_for_view_selection_callback, slot);
        slot->determine_view_file = NULL;
    }

    if (slot->new_content_view != NULL)
    {
        window->details->temporarily_ignore_view_signals = TRUE;
        caja_view_stop_loading (slot->new_content_view);
        window->details->temporarily_ignore_view_signals = FALSE;

        caja_window_slot_disconnect_content_view (slot, slot->new_content_view);
        g_object_unref (slot->new_content_view);
        slot->new_content_view = NULL;
    }
}

static void
cancel_location_change (CajaWindowSlot *slot)
{
    if (slot->pending_location != NULL
            && slot->location != NULL
            && slot->content_view != NULL)
    {
        GList *selection;

        /* No need to tell the new view - either it is the
         * same as the old view, in which case it will already
         * be told, or it is the very pending change we wish
         * to cancel.
         */
        selection = caja_view_get_selection (slot->content_view);
        load_new_location (slot,
                           slot->location,
                           selection,
                           TRUE,
                           FALSE);
    	g_list_free_full (selection, g_object_unref);
    }

    end_location_change (slot);
}

void
caja_window_report_view_failed (CajaWindow *window,
                                CajaView *view)
{
    CajaWindowSlot *slot;
    gboolean do_close_window;
    GFile *fallback_load_location;

    if (window->details->temporarily_ignore_view_signals)
    {
        return;
    }

    slot = caja_window_get_slot_for_view (window, view);
    g_assert (slot != NULL);

    g_warning ("A view failed. The UI will handle this with a dialog but this should be debugged.");

    do_close_window = FALSE;
    fallback_load_location = NULL;

    if (view == slot->content_view)
    {
        caja_window_slot_disconnect_content_view (slot, view);
        caja_window_slot_set_content_view_widget (slot, NULL);

        report_current_content_view_failure_to_user (slot);
    }
    else
    {
        /* Only report error on first try */
        if (slot->location_change_type != CAJA_LOCATION_CHANGE_FALLBACK)
        {
            report_nascent_content_view_failure_to_user (slot, view);

            fallback_load_location = g_object_ref (slot->pending_location);
        }
        else
        {
            if (!gtk_widget_get_visible (GTK_WIDGET (window)))
            {
                do_close_window = TRUE;
            }
        }
    }

    cancel_location_change (slot);

    if (fallback_load_location != NULL)
    {
        /* We loose the pending selection change here, but who cares... */
        begin_location_change (slot, fallback_load_location, NULL, NULL,
                               CAJA_LOCATION_CHANGE_FALLBACK, 0, NULL, NULL, NULL);
        g_object_unref (fallback_load_location);
    }

    if (do_close_window)
    {
        gtk_widget_destroy (GTK_WIDGET (window));
    }
}

static void
display_view_selection_failure (CajaWindow *window, CajaFile *file,
                                GFile *location, GError *error)
{
    char *full_uri_for_display;
    char *uri_for_display;
    char *error_message;
    char *detail_message;
    char *scheme_string;

    /* Some sort of failure occurred. How 'bout we tell the user? */
    full_uri_for_display = g_file_get_parse_name (location);
    /* Truncate the URI so it doesn't get insanely wide. Note that even
     * though the dialog uses wrapped text, if the URI doesn't contain
     * white space then the text-wrapping code is too stupid to wrap it.
     */
    uri_for_display = eel_str_middle_truncate
                      (full_uri_for_display, MAX_URI_IN_DIALOG_LENGTH);
    g_free (full_uri_for_display);

    error_message = NULL;
    detail_message = NULL;
    if (error == NULL)
    {
        if (caja_file_is_directory (file))
        {
            error_message = g_strdup_printf
                            (_("Could not display \"%s\"."),
                             uri_for_display);
            detail_message = g_strdup
                             (_("Caja has no installed viewer capable of displaying the folder."));
        }
        else
        {
            error_message = g_strdup_printf
                            (_("Could not display \"%s\"."),
                             uri_for_display);
            detail_message = g_strdup
                             (_("The location is not a folder."));
        }
    }
    else if (error->domain == G_IO_ERROR)
    {
        switch (error->code)
        {
        case G_IO_ERROR_NOT_FOUND:
            error_message = g_strdup_printf
                            (_("Could not find \"%s\"."),
                             uri_for_display);
            detail_message = g_strdup
                             (_("Please check the spelling and try again."));
            break;
        case G_IO_ERROR_NOT_SUPPORTED:
            scheme_string = g_file_get_uri_scheme (location);

            error_message = g_strdup_printf (_("Could not display \"%s\"."),
                                             uri_for_display);
            if (scheme_string != NULL)
            {
                detail_message = g_strdup_printf (_("Caja cannot handle \"%s\" locations."),
                                                  scheme_string);
            }
            else
            {
                detail_message = g_strdup (_("Caja cannot handle this kind of location."));
            }
            g_free (scheme_string);
            break;
        case G_IO_ERROR_NOT_MOUNTED:
            error_message = g_strdup_printf (_("Could not display \"%s\"."),
                                             uri_for_display);
            detail_message = g_strdup (_("Unable to mount the location."));
            break;

        case G_IO_ERROR_PERMISSION_DENIED:
            error_message = g_strdup_printf (_("Could not display \"%s\"."),
                                             uri_for_display);
            detail_message = g_strdup (_("Access was denied."));
            break;

        case G_IO_ERROR_HOST_NOT_FOUND:
            /* This case can be hit for user-typed strings like "foo" due to
             * the code that guesses web addresses when there's no initial "/".
             * But this case is also hit for legitimate web addresses when
             * the proxy is set up wrong.
             */
            error_message = g_strdup_printf (_("Could not display \"%s\", because the host could not be found."),
                                             uri_for_display);
            detail_message = g_strdup (_("Check that the spelling is correct and that your proxy settings are correct."));
            break;
        case G_IO_ERROR_CANCELLED:
        case G_IO_ERROR_FAILED_HANDLED:
            g_free (uri_for_display);
            return;

        default:
            break;
        }
    }

    if (error_message == NULL)
    {
        error_message = g_strdup_printf (_("Could not display \"%s\"."),
                                         uri_for_display);
        detail_message = g_strdup_printf (_("Error: %s\nPlease select another viewer and try again."), error->message);
    }

    eel_show_error_dialog (error_message, detail_message, NULL);

    g_free (uri_for_display);
    g_free (error_message);
    g_free (detail_message);
}


void
caja_window_slot_stop_loading (CajaWindowSlot *slot)
{
    CajaWindow *window;

    window = CAJA_WINDOW (slot->pane->window);
    g_assert (CAJA_IS_WINDOW (window));

    caja_view_stop_loading (slot->content_view);

    if (slot->new_content_view != NULL)
    {
        window->details->temporarily_ignore_view_signals = TRUE;
        caja_view_stop_loading (slot->new_content_view);
        window->details->temporarily_ignore_view_signals = FALSE;
    }

    cancel_location_change (slot);
}

void
caja_window_slot_set_content_view (CajaWindowSlot *slot,
                                   const char *id)
{
    CajaWindow *window;
    CajaFile *file;
    char *uri;

    g_assert (slot != NULL);
    g_assert (slot->location != NULL);
    g_assert (id != NULL);

    window = slot->pane->window;
    g_assert (CAJA_IS_WINDOW (window));

    uri = caja_window_slot_get_location_uri (slot);
    caja_debug_log (FALSE, CAJA_DEBUG_LOG_DOMAIN_USER,
                    "change view of window %p: \"%s\" to \"%s\"",
                    window, uri, id);
    g_free (uri);

    if (caja_window_slot_content_view_matches_iid (slot, id))
    {
        return;
    }

    end_location_change (slot);

    file = caja_file_get (slot->location);
    caja_file_set_metadata
    (file, CAJA_METADATA_KEY_DEFAULT_VIEW, NULL, id);
    caja_file_unref (file);

    caja_window_slot_set_allow_stop (slot, TRUE);

    if (caja_view_get_selection_count (slot->content_view) == 0)
    {
        /* If there is no selection, queue a scroll to the same icon that
         * is currently visible */
        slot->pending_scroll_to = caja_view_get_first_visible_file (slot->content_view);
    }
    slot->location_change_type = CAJA_LOCATION_CHANGE_RELOAD;

    create_content_view (slot, id);
}

void
caja_window_manage_views_close_slot (CajaWindowPane *pane,
                                     CajaWindowSlot *slot)
{
    if (slot->content_view != NULL)
    {
        caja_window_slot_disconnect_content_view (slot, slot->content_view);
    }

    free_location_change (slot);
    cancel_viewed_file_changed_callback (slot);
}

void
caja_navigation_window_back_or_forward (CajaNavigationWindow *window,
                                        gboolean back, guint distance, gboolean new_tab)
{
    CajaWindowSlot *slot;
    CajaNavigationWindowSlot *navigation_slot;
    GList *list;
    GFile *location;
    guint len;
    CajaBookmark *bookmark;

    slot = CAJA_WINDOW (window)->details->active_pane->active_slot;
    navigation_slot = (CajaNavigationWindowSlot *) slot;
    list = back ? navigation_slot->back_list : navigation_slot->forward_list;

    len = (guint) g_list_length (list);

    /* If we can't move in the direction at all, just return. */
    if (len == 0)
        return;

    /* If the distance to move is off the end of the list, go to the end
       of the list. */
    if (distance >= len)
        distance = len - 1;

    bookmark = g_list_nth_data (list, distance);
    location = caja_bookmark_get_location (bookmark);

    if (new_tab)
    {
        caja_window_slot_open_location_full (slot, location,
                                             CAJA_WINDOW_OPEN_ACCORDING_TO_MODE,
                                             CAJA_WINDOW_OPEN_FLAG_NEW_TAB,
                                             NULL, NULL, NULL);
    }
    else
    {
        GFile *old_location;
        char *scroll_pos;

        old_location = caja_window_slot_get_location (slot);
        scroll_pos = caja_bookmark_get_scroll_pos (bookmark);
        begin_location_change
        (slot,
         location, old_location, NULL,
         back ? CAJA_LOCATION_CHANGE_BACK : CAJA_LOCATION_CHANGE_FORWARD,
         distance,
         scroll_pos,
         NULL, NULL);

        if (old_location) {
            g_object_unref (old_location);
        }

        g_free (scroll_pos);
    }

    g_object_unref (location);
}

/* reload the contents of the window */
void
caja_window_slot_reload (CajaWindowSlot *slot)
{
    GFile *location;
    char *current_pos;
    GList *selection;

    g_assert (CAJA_IS_WINDOW_SLOT (slot));

    if (slot->location == NULL)
    {
        return;
    }

    /* peek_slot_field (window, location) can be free'd during the processing
     * of begin_location_change, so make a copy
     */
    location = g_object_ref (slot->location);
    current_pos = NULL;
    selection = NULL;
    if (slot->content_view != NULL)
    {
        current_pos = caja_view_get_first_visible_file (slot->content_view);
        selection = caja_view_get_selection (slot->content_view);
    }
    begin_location_change
    (slot, location, location, selection,
     CAJA_LOCATION_CHANGE_RELOAD, 0, current_pos,
     NULL, NULL);
    g_free (current_pos);
    g_object_unref (location);
    g_list_free_full (selection, g_object_unref);
}

void
caja_window_reload (CajaWindow *window)
{
    g_assert (CAJA_IS_WINDOW (window));

    caja_window_slot_reload (window->details->active_pane->active_slot);
}

