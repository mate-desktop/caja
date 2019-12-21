/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-window-slot.c: Caja window slot

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
#include <eel/eel-string.h>

#include <libcaja-private/caja-file.h>
#include <libcaja-private/caja-file-utilities.h>
#include <libcaja-private/caja-window-slot-info.h>

#include "caja-window-slot.h"
#include "caja-navigation-window-slot.h"
#include "caja-desktop-window.h"
#include "caja-window-private.h"
#include "caja-window-manage-views.h"

static void caja_window_slot_dispose    (GObject *object);

static void caja_window_slot_info_iface_init (CajaWindowSlotInfoIface *iface);

G_DEFINE_TYPE_WITH_CODE (CajaWindowSlot,
                         caja_window_slot,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CAJA_TYPE_WINDOW_SLOT_INFO,
                                 caja_window_slot_info_iface_init))

#define parent_class caja_window_slot_parent_class

static void
query_editor_changed_callback (CajaSearchBar *bar,
                               CajaQuery *query,
                               gboolean reload,
                               CajaWindowSlot *slot)
{
    CajaDirectory *directory;

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
real_update_query_editor (CajaWindowSlot *slot)
{
    CajaDirectory *directory;

    directory = caja_directory_get (slot->location);

    if (CAJA_IS_SEARCH_DIRECTORY (directory))
    {
        GtkWidget *query_editor;
        CajaQuery *query;
        CajaSearchDirectory *search_directory;

        search_directory = CAJA_SEARCH_DIRECTORY (directory);

        query_editor = caja_query_editor_new (caja_search_directory_is_saved_search (search_directory),
                                              caja_search_directory_is_indexed (search_directory));

        slot->query_editor = CAJA_QUERY_EDITOR (query_editor);

        caja_window_slot_add_extra_location_widget (slot, query_editor);
        gtk_widget_show (query_editor);
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
    }

    caja_directory_unref (directory);
}


static void
real_active (CajaWindowSlot *slot)
{
    CajaWindow *window;

    window = slot->pane->window;

    /* sync window to new slot */
    caja_window_sync_status (window);
    caja_window_sync_allow_stop (window, slot);
    caja_window_sync_title (window, slot);
    caja_window_sync_zoom_widgets (window);
    caja_window_pane_sync_location_widgets (slot->pane);
    caja_window_pane_sync_search_widgets (slot->pane);

    if (slot->viewed_file != NULL)
    {
        caja_window_load_view_as_menus (window);
        caja_window_load_extension_menus (window);
    }
}

static void
caja_window_slot_active (CajaWindowSlot *slot)
{
    CajaWindow *window;
    CajaWindowPane *pane;

    g_assert (CAJA_IS_WINDOW_SLOT (slot));

    pane = CAJA_WINDOW_PANE (slot->pane);
    window = CAJA_WINDOW (slot->pane->window);
    g_assert (g_list_find (pane->slots, slot) != NULL);
    g_assert (slot == window->details->active_pane->active_slot);

    EEL_CALL_METHOD (CAJA_WINDOW_SLOT_CLASS, slot,
                     active, (slot));
}

static void
real_inactive (CajaWindowSlot *slot)
{
    CajaWindow *window;

    window = CAJA_WINDOW (slot->pane->window);
    g_assert (slot == window->details->active_pane->active_slot);
}

static void
caja_window_slot_inactive (CajaWindowSlot *slot)
{
    CajaWindow *window;
    CajaWindowPane *pane;

    g_assert (CAJA_IS_WINDOW_SLOT (slot));

    pane = CAJA_WINDOW_PANE (slot->pane);
    window = CAJA_WINDOW (pane->window);

    g_assert (g_list_find (pane->slots, slot) != NULL);
    g_assert (slot == window->details->active_pane->active_slot);

    EEL_CALL_METHOD (CAJA_WINDOW_SLOT_CLASS, slot,
                     inactive, (slot));
}


static void
caja_window_slot_init (CajaWindowSlot *slot)
{
    GtkWidget *content_box, *eventbox, *extras_vbox, *frame;

    content_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    slot->content_box = content_box;
    gtk_widget_show (content_box);

    frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
    gtk_box_pack_start (GTK_BOX (content_box), frame, FALSE, FALSE, 0);
    slot->extra_location_frame = frame;

    eventbox = gtk_event_box_new ();
    gtk_widget_set_name (eventbox, "caja-extra-view-widget");
    gtk_container_add (GTK_CONTAINER (frame), eventbox);
    gtk_widget_show (eventbox);

    extras_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width (GTK_CONTAINER (extras_vbox), 6);
    slot->extra_location_widgets = extras_vbox;
    gtk_container_add (GTK_CONTAINER (eventbox), extras_vbox);
    gtk_widget_show (extras_vbox);

    slot->view_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start (GTK_BOX (content_box), slot->view_box, TRUE, TRUE, 0);
    gtk_widget_show (slot->view_box);

    slot->title = g_strdup (_("Loading..."));
}

static void
caja_window_slot_class_init (CajaWindowSlotClass *class)
{
    class->active = real_active;
    class->inactive = real_inactive;
    class->update_query_editor = real_update_query_editor;

    G_OBJECT_CLASS (class)->dispose = caja_window_slot_dispose;
}

static int
caja_window_slot_get_selection_count (CajaWindowSlot *slot)
{
    g_assert (CAJA_IS_WINDOW_SLOT (slot));

    if (slot->content_view != NULL)
    {
        return caja_view_get_selection_count (slot->content_view);
    }
    return 0;
}

GFile *
caja_window_slot_get_location (CajaWindowSlot *slot)
{
    g_assert (slot != NULL);
    g_assert (CAJA_IS_WINDOW (slot->pane->window));

    if (slot->location != NULL)
    {
        return g_object_ref (slot->location);
    }
    return NULL;
}

char *
caja_window_slot_get_location_uri (CajaWindowSlotInfo *slot)
{
    g_assert (CAJA_IS_WINDOW_SLOT (slot));

    if (slot->location)
    {
        return g_file_get_uri (slot->location);
    }
    return NULL;
}

static void
caja_window_slot_make_hosting_pane_active (CajaWindowSlot *slot)
{
    g_assert (CAJA_IS_WINDOW_SLOT (slot));
    g_assert (CAJA_IS_WINDOW_PANE (slot->pane));

    caja_window_set_active_slot (slot->pane->window, slot);
}

char *
caja_window_slot_get_title (CajaWindowSlot *slot)
{
    char *title;

    g_assert (CAJA_IS_WINDOW_SLOT (slot));

    title = NULL;
    if (slot->new_content_view != NULL)
    {
        title = caja_view_get_title (slot->new_content_view);
    }
    else if (slot->content_view != NULL)
    {
        title = caja_view_get_title (slot->content_view);
    }

    if (title == NULL)
    {
        title = caja_compute_title_for_location (slot->location);
    }

    return title;
}

static CajaWindow *
caja_window_slot_get_window (CajaWindowSlot *slot)
{
    g_assert (CAJA_IS_WINDOW_SLOT (slot));
    return slot->pane->window;
}

/* caja_window_slot_set_title:
 *
 * Sets slot->title, and if it changed
 * synchronizes the actual GtkWindow title which
 * might look a bit different (e.g. with "file browser:" added)
 */
static void
caja_window_slot_set_title (CajaWindowSlot *slot,
                            const char *title)
{
    CajaWindow *window;
    gboolean changed;

    g_assert (CAJA_IS_WINDOW_SLOT (slot));

    window = CAJA_WINDOW (slot->pane->window);

    changed = FALSE;

    if (eel_strcmp (title, slot->title) != 0)
    {
        changed = TRUE;

        g_free (slot->title);
        slot->title = g_strdup (title);
    }

    if (eel_strlen (slot->title) > 0 && slot->current_location_bookmark &&
            caja_bookmark_set_name (slot->current_location_bookmark,
                                    slot->title))
    {
        changed = TRUE;

        /* Name of item in history list changed, tell listeners. */
        caja_send_history_list_changed ();
    }

    if (changed)
    {
        caja_window_sync_title (window, slot);
    }
}


/* caja_window_slot_update_title:
 *
 * Re-calculate the slot title.
 * Called when the location or view has changed.
 * @slot: The CajaWindowSlot in question.
 *
 */
void
caja_window_slot_update_title (CajaWindowSlot *slot)
{
    char *title;

    title = caja_window_slot_get_title (slot);
    caja_window_slot_set_title (slot, title);
    g_free (title);
}

/* caja_window_slot_update_icon:
 *
 * Re-calculate the slot icon
 * Called when the location or view or icon set has changed.
 * @slot: The CajaWindowSlot in question.
 */
void
caja_window_slot_update_icon (CajaWindowSlot *slot)
{
    CajaWindow *window;
    CajaIconInfo *info;
    const char *icon_name;

    window = slot->pane->window;

    g_return_if_fail (CAJA_IS_WINDOW (window));

    info = EEL_CALL_METHOD_WITH_RETURN_VALUE (CAJA_WINDOW_CLASS, window,
            get_icon, (window, slot));

    if (slot != slot->pane->active_slot)
        return;

    icon_name = NULL;
    if (info)
    {
        icon_name = caja_icon_info_get_used_name (info);
        if (icon_name != NULL)
        {
            /* Gtk+ doesn't short circuit this (yet), so avoid lots of work
             * if we're setting to the same icon. This happens a lot e.g. when
             * the trash directory changes due to the file count changing.
             */
            if (g_strcmp0 (icon_name, gtk_window_get_icon_name (GTK_WINDOW (window))) != 0)
            {
                if (g_strcmp0 (icon_name, "text-x-generic") == 0)
                    gtk_window_set_icon_name (GTK_WINDOW (window), "folder-saved-search");
                else
                    gtk_window_set_icon_name (GTK_WINDOW (window), icon_name);
            }
        }
        else
        {
            GdkPixbuf *pixbuf;

            pixbuf = caja_icon_info_get_pixbuf_nodefault (info);

            if (pixbuf)
            {
                gtk_window_set_icon (GTK_WINDOW (window), pixbuf);
                g_object_unref (pixbuf);
            }
        }

        g_object_unref (info);
    }
}

void
caja_window_slot_is_in_active_pane (CajaWindowSlot *slot,
                                    gboolean is_active)
{
    /* NULL is valid, and happens during init */
    if (!slot)
    {
        return;
    }

    /* it may also be that the content is not a valid directory view during init */
    if (slot->content_view != NULL)
    {
        caja_view_set_is_active (slot->content_view, is_active);
    }

    if (slot->new_content_view != NULL)
    {
        caja_view_set_is_active (slot->new_content_view, is_active);
    }
}

void
caja_window_slot_connect_content_view (CajaWindowSlot *slot,
                                       CajaView *view)
{
    CajaWindow *window;

    window = slot->pane->window;
    if (window != NULL && slot == caja_window_get_active_slot (window))
    {
        caja_window_connect_content_view (window, view);
    }
}

void
caja_window_slot_disconnect_content_view (CajaWindowSlot *slot,
        CajaView *view)
{
    CajaWindow *window;

    window = slot->pane->window;
    if (window != NULL && window->details->active_pane && window->details->active_pane->active_slot == slot)
    {
        caja_window_disconnect_content_view (window, view);
    }
}

void
caja_window_slot_set_content_view_widget (CajaWindowSlot *slot,
        CajaView *new_view)
{
    CajaWindow *window;
    GtkWidget *widget;

    window = slot->pane->window;
    g_assert (CAJA_IS_WINDOW (window));

    if (slot->content_view != NULL)
    {
        /* disconnect old view */
        caja_window_slot_disconnect_content_view (slot, slot->content_view);

        widget = caja_view_get_widget (slot->content_view);
        gtk_widget_destroy (widget);
        g_object_unref (slot->content_view);
        slot->content_view = NULL;
    }

    if (new_view != NULL)
    {
        widget = caja_view_get_widget (new_view);
        gtk_box_pack_start (GTK_BOX (slot->view_box), widget,
                            TRUE, TRUE, 0);

        gtk_widget_show (widget);

        slot->content_view = new_view;
        g_object_ref (slot->content_view);

        /* connect new view */
        caja_window_slot_connect_content_view (slot, new_view);
    }
}

void
caja_window_slot_set_allow_stop (CajaWindowSlot *slot,
                                 gboolean allow)
{
    CajaWindow *window;

    g_assert (CAJA_IS_WINDOW_SLOT (slot));

    slot->allow_stop = allow;

    window = CAJA_WINDOW (slot->pane->window);
    caja_window_sync_allow_stop (window, slot);
}

void
caja_window_slot_set_status (CajaWindowSlot *slot,
                             const char *status)
{
    CajaWindow *window;

    g_assert (CAJA_IS_WINDOW_SLOT (slot));

    g_free (slot->status_text);
    slot->status_text = g_strdup (status);

    window = CAJA_WINDOW (slot->pane->window);
    if (slot == window->details->active_pane->active_slot)
    {
        caja_window_sync_status (window);
    }
}

/* caja_window_slot_update_query_editor:
 *
 * Update the query editor.
 * Called when the location has changed.
 *
 * @slot: The CajaWindowSlot in question.
 */
void
caja_window_slot_update_query_editor (CajaWindowSlot *slot)
{
    if (slot->query_editor != NULL)
    {
        gtk_widget_destroy (GTK_WIDGET (slot->query_editor));
        g_assert (slot->query_editor == NULL);
    }

    EEL_CALL_METHOD (CAJA_WINDOW_SLOT_CLASS, slot,
                     update_query_editor, (slot));

    eel_add_weak_pointer (&slot->query_editor);
}

static void
remove_all (GtkWidget *widget,
            gpointer data)
{
    GtkContainer *container;
    container = GTK_CONTAINER (data);

    gtk_container_remove (container, widget);
}

void
caja_window_slot_remove_extra_location_widgets (CajaWindowSlot *slot)
{
    gtk_container_foreach (GTK_CONTAINER (slot->extra_location_widgets),
                           remove_all,
                           slot->extra_location_widgets);
    gtk_widget_hide (slot->extra_location_frame);
}

void
caja_window_slot_add_extra_location_widget (CajaWindowSlot *slot,
        GtkWidget *widget)
{
    gtk_box_pack_start (GTK_BOX (slot->extra_location_widgets),
                        widget, TRUE, TRUE, 0);
    gtk_widget_show (slot->extra_location_frame);
}

void
caja_window_slot_add_current_location_to_history_list (CajaWindowSlot *slot)
{

    if ((slot->pane->window == NULL || !CAJA_IS_DESKTOP_WINDOW (slot->pane->window)) &&
            caja_add_bookmark_to_history_list (slot->current_location_bookmark))
    {
        caja_send_history_list_changed ();
    }
}

/* returns either the pending or the actual current location - used by side panes. */
static char *
real_slot_info_get_current_location (CajaWindowSlotInfo *info)
{
    CajaWindowSlot *slot;

    slot = CAJA_WINDOW_SLOT (info);

    if (slot->pending_location != NULL)
    {
        return g_file_get_uri (slot->pending_location);
    }

    if (slot->location != NULL)
    {
        return g_file_get_uri (slot->location);
    }

    g_assert_not_reached ();
    return NULL;
}

static CajaView *
real_slot_info_get_current_view (CajaWindowSlotInfo *info)
{
    CajaWindowSlot *slot;

    slot = CAJA_WINDOW_SLOT (info);

    if (slot->content_view != NULL)
    {
        return g_object_ref (slot->content_view);
    }
    else if (slot->new_content_view)
    {
        return g_object_ref (slot->new_content_view);
    }

    return NULL;
}

static void
caja_window_slot_dispose (GObject *object)
{
    CajaWindowSlot *slot;
    GtkWidget *widget;

    slot = CAJA_WINDOW_SLOT (object);

    if (slot->content_view)
    {
        widget = caja_view_get_widget (slot->content_view);
        gtk_widget_destroy (widget);
        g_object_unref (slot->content_view);
        slot->content_view = NULL;
    }

    if (slot->new_content_view)
    {
        widget = caja_view_get_widget (slot->new_content_view);
        gtk_widget_destroy (widget);
        g_object_unref (slot->new_content_view);
        slot->new_content_view = NULL;
    }

    caja_window_slot_set_viewed_file (slot, NULL);

    g_clear_object (&slot->location);

    g_list_free_full (slot->pending_selection, g_free);
    slot->pending_selection = NULL;

    if (slot->current_location_bookmark != NULL)
    {
        g_object_unref (slot->current_location_bookmark);
        slot->current_location_bookmark = NULL;
    }
    if (slot->last_location_bookmark != NULL)
    {
        g_object_unref (slot->last_location_bookmark);
        slot->last_location_bookmark = NULL;
    }

    if (slot->find_mount_cancellable != NULL)
    {
        g_cancellable_cancel (slot->find_mount_cancellable);
        slot->find_mount_cancellable = NULL;
    }

    slot->pane = NULL;

    g_free (slot->title);
    slot->title = NULL;

    g_free (slot->status_text);
    slot->status_text = NULL;

    G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
caja_window_slot_info_iface_init (CajaWindowSlotInfoIface *iface)
{
    iface->active = caja_window_slot_active;
    iface->inactive = caja_window_slot_inactive;
    iface->get_window = caja_window_slot_get_window;
    iface->get_selection_count = caja_window_slot_get_selection_count;
    iface->get_current_location = real_slot_info_get_current_location;
    iface->get_current_view = real_slot_info_get_current_view;
    iface->set_status = caja_window_slot_set_status;
    iface->get_title = caja_window_slot_get_title;
    iface->open_location = caja_window_slot_open_location_full;
    iface->make_hosting_pane_active = caja_window_slot_make_hosting_pane_active;
}

