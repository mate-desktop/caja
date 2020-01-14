/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

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
 *  	     John Sullivan <sullivan@eazel.com>
 *
 */

/* caja-window.c: Implementation of the main window object */

#include <config.h>

#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-string.h>

#include <libcaja-private/caja-dnd.h>
#include <libcaja-private/caja-file-utilities.h>
#include <libcaja-private/caja-ui-utilities.h>
#include <libcaja-private/caja-file-attributes.h>
#include <libcaja-private/caja-global-preferences.h>
#include <libcaja-private/caja-metadata.h>
#include <libcaja-private/caja-mime-actions.h>
#include <libcaja-private/caja-program-choosing.h>
#include <libcaja-private/caja-search-directory.h>
#include <libcaja-private/caja-search-engine.h>
#include <libcaja-private/caja-signaller.h>

#include "caja-spatial-window.h"
#include "caja-window-private.h"
#include "caja-window-bookmarks.h"
#include "caja-actions.h"
#include "caja-application.h"
#include "caja-desktop-window.h"
#include "caja-bookmarks-window.h"
#include "caja-location-dialog.h"
#include "caja-query-editor.h"
#include "caja-search-bar.h"
#include "caja-window-manage-views.h"

#define MAX_TITLE_LENGTH 180
#define MAX_SHORTNAME_PATH 16

#define SPATIAL_ACTION_PLACES               "Places"
#define SPATIAL_ACTION_GO_TO_LOCATION       "Go to Location"
#define SPATIAL_ACTION_CLOSE_PARENT_FOLDERS "Close Parent Folders"
#define SPATIAL_ACTION_CLOSE_ALL_FOLDERS    "Close All Folders"
#define MENU_PATH_SPATIAL_BOOKMARKS_PLACEHOLDER	"/MenuBar/Other Menus/Places/Bookmarks Placeholder"

struct _CajaSpatialWindowPrivate
{
    GtkActionGroup *spatial_action_group; /* owned by ui_manager */
    char *last_geometry;
    guint save_geometry_timeout_id;

    gboolean saved_data_on_close;
    GtkWidget *content_box;
    GtkWidget *location_button;
    GtkWidget *location_label;
    GtkWidget *location_icon;
};

static const GtkTargetEntry location_button_drag_types[] =
{
    { CAJA_ICON_DND_MATE_ICON_LIST_TYPE, 0, CAJA_ICON_DND_MATE_ICON_LIST },
    { CAJA_ICON_DND_URI_LIST_TYPE, 0, CAJA_ICON_DND_URI_LIST },
};

G_DEFINE_TYPE_WITH_PRIVATE (CajaSpatialWindow, caja_spatial_window, CAJA_TYPE_WINDOW)

static void caja_spatial_window_save_geometry (CajaSpatialWindow *window,
						   CajaFile *viewed_file);

static gboolean
save_window_geometry_timeout (gpointer callback_data)
{
    CajaSpatialWindow *window;
    CajaWindowSlot *slot;

    window = CAJA_SPATIAL_WINDOW (callback_data);
    slot = caja_window_get_active_slot (CAJA_WINDOW (window));

    if (slot != NULL)
    {
        caja_spatial_window_save_geometry (window, slot->viewed_file);
    }

    window->details->save_geometry_timeout_id = 0;

    return FALSE;
}

static gboolean
caja_spatial_window_configure_event (GtkWidget *widget,
                                     GdkEventConfigure *event)
{
    CajaSpatialWindow *window;

    window = CAJA_SPATIAL_WINDOW (widget);

    GTK_WIDGET_CLASS (caja_spatial_window_parent_class)->configure_event (widget, event);

    /* Only save the geometry if the user hasn't resized the window
     * for a second. Otherwise delay the callback another second.
     */
    if (window->details->save_geometry_timeout_id != 0)
    {
        g_source_remove (window->details->save_geometry_timeout_id);
    }

	window->details->save_geometry_timeout_id =
		g_timeout_add_seconds (1, save_window_geometry_timeout, window);

    return FALSE;
}

static void
caja_spatial_window_unrealize (GtkWidget *widget)
{
    CajaSpatialWindow *window;
    CajaWindowSlot *slot;

    window = CAJA_SPATIAL_WINDOW (widget);
    slot = caja_window_get_active_slot (CAJA_WINDOW (window));

    GTK_WIDGET_CLASS (caja_spatial_window_parent_class)->unrealize (widget);

    if (window->details->save_geometry_timeout_id != 0)
    {
        g_source_remove (window->details->save_geometry_timeout_id);
        window->details->save_geometry_timeout_id = 0;

        if (slot != NULL)
        {
            caja_spatial_window_save_geometry (window, slot->viewed_file);
        }
    }
}

static gboolean
caja_spatial_window_state_event (GtkWidget *widget,
                                 GdkEventWindowState *event)
{
    CajaWindow *window;
    CajaWindowSlot *slot;
    CajaFile *viewed_file;

    window = CAJA_WINDOW (widget);
    slot = window->details->active_pane->active_slot;
    viewed_file = slot->viewed_file;

    if (!CAJA_IS_DESKTOP_WINDOW (widget))
    {

        if (event->changed_mask & GDK_WINDOW_STATE_MAXIMIZED &&
                viewed_file != NULL)
        {
            caja_file_set_boolean_metadata (viewed_file,
                                            CAJA_METADATA_KEY_WINDOW_MAXIMIZED,
                                            FALSE,
                                            event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED);
        }

        if (event->changed_mask & GDK_WINDOW_STATE_STICKY &&
                viewed_file != NULL)
        {
            caja_file_set_boolean_metadata (viewed_file,
                                            CAJA_METADATA_KEY_WINDOW_STICKY,
                                            FALSE,
                                            event->new_window_state & GDK_WINDOW_STATE_STICKY);
        }

        if (event->changed_mask & GDK_WINDOW_STATE_ABOVE &&
                viewed_file != NULL)
        {
            caja_file_set_boolean_metadata (viewed_file,
                                            CAJA_METADATA_KEY_WINDOW_KEEP_ABOVE,
                                            FALSE,
                                            event->new_window_state & GDK_WINDOW_STATE_ABOVE);
        }

    }

    if (GTK_WIDGET_CLASS (caja_spatial_window_parent_class)->window_state_event != NULL)
    {
        return GTK_WIDGET_CLASS (caja_spatial_window_parent_class)->window_state_event (widget, event);
    }

    return FALSE;
}

static void
caja_spatial_window_finalize (GObject *object)
{
    CajaSpatialWindow *window;

    window = CAJA_SPATIAL_WINDOW (object);

    g_free (window->details->last_geometry);

    G_OBJECT_CLASS (caja_spatial_window_parent_class)->finalize (object);
}

static void
caja_spatial_window_save_geometry (CajaSpatialWindow *window,
        		   CajaFile *viewed_file)
{
    char *geometry_string;

    if (viewed_file == NULL)
    {
        /* We never showed a file */
        return;
    }

    if (gtk_widget_get_window (GTK_WIDGET (window)) &&
    	    gtk_widget_get_visible (GTK_WIDGET (window)) &&
	    !CAJA_IS_DESKTOP_WINDOW (window) &&
            !(gdk_window_get_state (gtk_widget_get_window (GTK_WIDGET(window))) & GDK_WINDOW_STATE_MAXIMIZED)) {

        geometry_string = eel_gtk_window_get_geometry_string (GTK_WINDOW (window));

        if (!g_strcmp0 (window->details->last_geometry, geometry_string)) {
        	/* Don't save geometry if it's the same as before. */
        	g_free (geometry_string);
        	return;
        }

        g_free (window->details->last_geometry);
        window->details->last_geometry = geometry_string;

        caja_file_set_metadata (viewed_file,
                                CAJA_METADATA_KEY_WINDOW_GEOMETRY,
                                NULL,
                                geometry_string);
    }
}

static void
caja_spatial_window_save_scroll_position (CajaSpatialWindow *window,
					  CajaWindowSlot *slot)
{
    char *scroll_string;

    if (slot->content_view == NULL ||
            slot->viewed_file == NULL)
    {
        return;
    }

    scroll_string = caja_view_get_first_visible_file (slot->content_view);
    caja_file_set_metadata (slot->viewed_file,
                            CAJA_METADATA_KEY_WINDOW_SCROLL_POSITION,
                            NULL,
                            scroll_string);
    g_free (scroll_string);
}

static void
caja_spatial_window_save_show_hidden_files_mode (CajaSpatialWindow *window,
						 CajaFile *viewed_file)
{
    CajaWindowShowHiddenFilesMode mode;

    if (viewed_file == NULL) {
        return;
    }

    mode = CAJA_WINDOW (window)->details->show_hidden_files_mode;

    if (mode != CAJA_WINDOW_SHOW_HIDDEN_FILES_DEFAULT) {
        char *show_hidden_file_setting;

        if (mode == CAJA_WINDOW_SHOW_HIDDEN_FILES_ENABLE) {
            show_hidden_file_setting = "1";
        } else {
            show_hidden_file_setting = "0";
        }
        caja_file_set_metadata (viewed_file,
                                CAJA_METADATA_KEY_WINDOW_SHOW_HIDDEN_FILES,
                                NULL,
                                show_hidden_file_setting);
    }
}

static void
caja_spatial_window_show (GtkWidget *widget)
{
    CajaWindow *window;
    CajaWindowSlot *slot;
    GFile *location;

    window = CAJA_WINDOW (widget);
    slot = caja_window_get_active_slot (window);

    GTK_WIDGET_CLASS (caja_spatial_window_parent_class)->show (widget);

    if (slot != NULL && slot->query_editor != NULL)
    {
        caja_query_editor_grab_focus (CAJA_QUERY_EDITOR (slot->query_editor));
    }

    location = caja_window_slot_get_location (slot);
    g_return_if_fail (location != NULL);

    while (location != NULL) {
        CajaFile *file;

        file = caja_file_get (location);

        if  (!caja_file_check_if_ready (file, CAJA_FILE_ATTRIBUTE_INFO)) {
            caja_file_call_when_ready (file,
                                       CAJA_FILE_ATTRIBUTE_INFO,
                                       NULL,
                                       NULL);
        }

        location = g_file_get_parent (location);
    }

    if (location) {
        g_object_unref (location);
    }
}

static void
action_close_parent_folders_callback (GtkAction *action,
                                      gpointer user_data)
{
    caja_application_close_parent_windows (CAJA_SPATIAL_WINDOW (user_data));
}

static void
action_close_all_folders_callback (GtkAction *action,
                                   gpointer user_data)
{
    caja_application_close_all_spatial_windows ();
}

static void
real_prompt_for_location (CajaWindow *window,
                          const char     *initial)
{
    GtkWidget *dialog;

    dialog = caja_location_dialog_new (window);
    if (initial != NULL)
    {
        caja_location_dialog_set_location (CAJA_LOCATION_DIALOG (dialog),
                                           initial);
    }

    gtk_widget_show (dialog);
}

static CajaIconInfo *
real_get_icon (CajaWindow *window,
               CajaWindowSlot *slot)
{
    return caja_file_get_icon (slot->viewed_file,
                               48, gtk_widget_get_scale_factor (GTK_WIDGET (window)),
                               CAJA_FILE_ICON_FLAGS_IGNORE_VISITING |
                               CAJA_FILE_ICON_FLAGS_USE_MOUNT_ICON);
}

static void
sync_window_title (CajaWindow *window)
{
    CajaWindowSlot *slot;

    slot = caja_window_get_active_slot (window);

    /* Don't change desktop's title, it would override the one already defined */
    if (CAJA_IS_DESKTOP_WINDOW (window))
        return;

    if (slot->title == NULL || slot->title[0] == '\0')
    {
        gtk_window_set_title (GTK_WINDOW (window), _("Caja"));
    }
    else
    {
        char *window_title;

        window_title = eel_str_middle_truncate (slot->title, MAX_TITLE_LENGTH);
        gtk_window_set_title (GTK_WINDOW (window), window_title);
        g_free (window_title);
    }
}

static void
real_sync_title (CajaWindow *window,
                 CajaWindowSlot *slot)
{
    g_assert (slot == caja_window_get_active_slot (window));

    sync_window_title (window);
}

static void
real_get_min_size (CajaWindow *window,
                   guint *min_width, guint *min_height)
{
    if (min_width)
    {
        *min_width = CAJA_SPATIAL_WINDOW_MIN_WIDTH;
    }
    if (min_height)
    {
        *min_height = CAJA_SPATIAL_WINDOW_MIN_HEIGHT;
    }
}

static void
real_get_default_size (CajaWindow *window,
                       guint *default_width, guint *default_height)
{
    if (default_width)
    {
        *default_width = CAJA_SPATIAL_WINDOW_DEFAULT_WIDTH;
    }
    if (default_height)
    {
        *default_height = CAJA_SPATIAL_WINDOW_DEFAULT_HEIGHT;
    }
}

static void
real_sync_allow_stop (CajaWindow *window,
                      CajaWindowSlot *slot)
{
}

static void
real_set_allow_up (CajaWindow *window, gboolean allow)
{
    CajaSpatialWindow *spatial;
    GtkAction *action;

    spatial = CAJA_SPATIAL_WINDOW (window);

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    action = gtk_action_group_get_action (spatial->details->spatial_action_group,
                                          SPATIAL_ACTION_CLOSE_PARENT_FOLDERS);
    gtk_action_set_sensitive (action, allow);
    G_GNUC_END_IGNORE_DEPRECATIONS;

    CAJA_WINDOW_CLASS (caja_spatial_window_parent_class)->set_allow_up (window, allow);
}

static CajaWindowSlot *
real_open_slot (CajaWindowPane *pane,
                CajaWindowOpenSlotFlags flags)
{
    CajaWindowSlot *slot;
    GList *slots;

    g_assert (caja_window_get_active_slot (pane->window) == NULL);

    slots = caja_window_get_slots (pane->window);
    g_assert (slots == NULL);
    g_list_free (slots);

    slot = g_object_new (CAJA_TYPE_WINDOW_SLOT, NULL);
    slot->pane = pane;
    gtk_container_add (GTK_CONTAINER (CAJA_SPATIAL_WINDOW (pane->window)->details->content_box),
                       slot->content_box);
    gtk_widget_show (slot->content_box);
    return slot;
}

static void
save_spatial_data (CajaSpatialWindow *window,
		   CajaWindowSlot *slot)
{
    caja_spatial_window_save_geometry (window, slot->viewed_file);
    caja_spatial_window_save_scroll_position (window, slot);
    caja_spatial_window_save_show_hidden_files_mode (window, slot->viewed_file);
}

static void
real_close_slot (CajaWindowPane *pane,
                 CajaWindowSlot *slot)
{
    CajaSpatialWindow *window;

    window = CAJA_SPATIAL_WINDOW (pane->window);

    /* Save spatial data for close if we didn't already */
    if (!window->details->saved_data_on_close) {
        save_spatial_data (window, slot);
    }

    CAJA_WINDOW_CLASS (caja_spatial_window_parent_class)->close_slot (pane, slot);
}

static void
real_window_close (CajaWindow *window)
{
    CajaWindowSlot *slot;
    CajaSpatialWindow *self;

    self = CAJA_SPATIAL_WINDOW (window);

    /* We're closing the window, save the geometry. */
    /* Note that we do this in window close, not slot close, because slot
     * close is too late, by then the widgets have been unrealized.
     * This is for the close by WM case, if you're closing via Ctrl-W that
     * means we close the slots first and this is not an issue */
    slot = caja_window_get_active_slot (window);

    if (slot != NULL) {
        save_spatial_data (self, slot);
        self->details->saved_data_on_close = TRUE;
    }

    if (CAJA_WINDOW_CLASS (caja_spatial_window_parent_class)->close != NULL) {
        CAJA_WINDOW_CLASS (caja_spatial_window_parent_class)->close (window);
    }
}

static void
location_menu_item_activated_callback (GtkWidget *menu_item,
                                       CajaWindow *window)
{
    CajaWindowSlot *slot;
    GFile *current;
    GFile *dest;
    GdkEvent *event;

	slot = caja_window_get_active_slot (window);
	current = caja_window_slot_get_location (slot);
	dest = g_object_get_data (G_OBJECT (menu_item), "location");

    event = gtk_get_current_event();

    if (!g_file_equal (current, dest))
    {
        GFile *child;
        gboolean close_behind;
        GList *selection;

        close_behind = FALSE;
        selection = NULL;

        child = g_object_get_data (G_OBJECT(menu_item), "child_location");
        if (child != NULL) {
            selection = g_list_prepend (NULL, g_object_ref (child));
        }

        if (event != NULL && ((GdkEventAny *) event)->type == GDK_BUTTON_RELEASE &&
                (((GdkEventButton *) event)->button == 2 ||
                 (((GdkEventButton *) event)->state & GDK_SHIFT_MASK) != 0)) {
            close_behind = TRUE;
        }

        caja_window_slot_open_location_with_selection
        (slot, dest, selection, close_behind);

    	g_list_free_full (selection, g_object_unref);
    }

    if (event != NULL) {
        gdk_event_free (event);
    }

    g_object_unref (current);
}

static void
menu_deactivate_callback (GtkWidget *menu,
                          gpointer   data)
{
    GMainLoop *loop;

    loop = data;

    if (g_main_loop_is_running (loop))
    {
        g_main_loop_quit (loop);
    }
}

static gboolean
location_button_pressed_callback (GtkWidget      *widget,
                                  GdkEventButton *event,
                                  CajaWindow *window)
{
	CajaWindowSlot *slot;
    CajaView *view;

	slot = caja_window_get_active_slot (window);
	view = slot->content_view;

    if (event->button == 3 && view != NULL)
    {
        caja_view_pop_up_location_context_menu (view, event, NULL);
    }

    return FALSE;
}

static void
location_button_clicked_callback (GtkWidget         *widget,
                                  CajaSpatialWindow *window)
{
    CajaWindowSlot *slot;
    GtkWidget *popup, *menu_item, *first_item = NULL;
    cairo_surface_t *surface = NULL;
    GFile *location;
    GFile *child_location;
    GMainLoop *loop;

    slot = caja_window_get_active_slot (CAJA_WINDOW (window));

    popup = gtk_menu_new ();

    gtk_menu_set_reserve_toggle_size (GTK_MENU (popup), FALSE);

    first_item = NULL;

    location = caja_window_slot_get_location (slot);
    g_return_if_fail (location != NULL);

    child_location = NULL;
    while (location != NULL) {
        CajaFile *file;
        char *name;

        file = caja_file_get (location);

        name = caja_file_get_display_name (file);

        surface = NULL;

        surface = caja_file_get_icon_surface (file,
                                              caja_get_icon_size_for_stock_size (GTK_ICON_SIZE_MENU),
                                              TRUE,
                                              gtk_widget_get_scale_factor (widget),
                                              CAJA_FILE_ICON_FLAGS_IGNORE_VISITING);

        if (surface != NULL)
        {
            menu_item = eel_image_menu_item_new_from_surface (surface, name);
            cairo_surface_destroy (surface);
        }
        else
        {
            menu_item = eel_image_menu_item_new_from_icon ("document-open", name);
        }

        if (first_item == NULL) {
            first_item = menu_item;
        }

        gtk_widget_show (menu_item);
        g_signal_connect (menu_item, "activate",
                          G_CALLBACK (location_menu_item_activated_callback),
                          window);

        g_object_set_data_full (G_OBJECT (menu_item),
                                "location",
                                g_object_ref (location),
                                (GDestroyNotify)g_object_unref);

        if (child_location) {
            g_object_set_data_full (G_OBJECT (menu_item),
                                    "child_location",
                                    g_object_ref (child_location),
                                    (GDestroyNotify)g_object_unref);
        }

        gtk_menu_shell_prepend (GTK_MENU_SHELL (popup), menu_item);

        if (child_location) {
            g_object_unref (child_location);
        }
        child_location = location;
        location = g_file_get_parent (location);
    }

    if (child_location) {
        g_object_unref (child_location);
    }
    if (location) {
        g_object_unref (location);
    }

    gtk_menu_set_screen (GTK_MENU (popup), gtk_widget_get_screen (widget));

    loop = g_main_loop_new (NULL, FALSE);

    g_signal_connect (popup, "deactivate",
                      G_CALLBACK (menu_deactivate_callback),
                      loop);

    gtk_grab_add (popup);
    gtk_menu_popup_at_widget (GTK_MENU (popup),
                              widget,
                              GDK_GRAVITY_SOUTH_WEST,
                              GDK_GRAVITY_NORTH_WEST,
                              NULL);

    gtk_menu_shell_select_item (GTK_MENU_SHELL (popup), first_item);
    g_main_loop_run (loop);
    gtk_grab_remove (popup);
    g_main_loop_unref (loop);
    g_object_ref_sink (popup);
    g_object_unref (popup);
}

static int
get_dnd_icon_size (CajaSpatialWindow *window)
{
	CajaWindowSlot *active_slot;
    CajaView *view;
    CajaZoomLevel zoom_level;

	active_slot = caja_window_get_active_slot (CAJA_WINDOW (window));
	view = active_slot->content_view;

    if (view == NULL)
    {
        return CAJA_ICON_SIZE_STANDARD;
    }
    else
    {
        zoom_level = caja_view_get_zoom_level (view);
        return caja_get_icon_size_for_zoom_level (zoom_level);
    }
}

static void
location_button_drag_begin_callback (GtkWidget             *widget,
                                     GdkDragContext        *context,
                                     CajaSpatialWindow *window)
{
    CajaWindowSlot *slot;
    cairo_surface_t *surface;

    slot = CAJA_WINDOW (window)->details->active_pane->active_slot;

    surface = caja_file_get_icon_surface (slot->viewed_file,
                                          get_dnd_icon_size (window),
                                          FALSE,
                                          gtk_widget_get_scale_factor (widget),
                                          CAJA_FILE_ICON_FLAGS_IGNORE_VISITING | CAJA_FILE_ICON_FLAGS_FOR_DRAG_ACCEPT);

    gtk_drag_set_icon_surface (context, surface);

    cairo_surface_destroy (surface);
}

/* build MATE icon list, which only contains the window's URI.
 * If we just used URIs, moving the folder to trash
 * wouldn't work */
static void
get_data_binder (CajaDragEachSelectedItemDataGet iteratee,
                 gpointer                            iterator_context,
                 gpointer                            data)
{
    CajaSpatialWindow *window;
    CajaWindowSlot *slot;
    char *location;
    int icon_size;

    g_assert (CAJA_IS_SPATIAL_WINDOW (iterator_context));
    window = CAJA_SPATIAL_WINDOW (iterator_context);

    slot = CAJA_WINDOW (window)->details->active_pane->active_slot;

    location = caja_window_slot_get_location_uri (slot);
    icon_size = get_dnd_icon_size (window);

    iteratee (location,
              0,
              0,
              icon_size,
              icon_size,
              data);

    g_free (location);
}

static void
location_button_drag_data_get_callback (GtkWidget             *widget,
                                        GdkDragContext        *context,
                                        GtkSelectionData      *selection_data,
                                        guint                  info,
                                        guint                  time,
                                        CajaSpatialWindow *window)
{
    caja_drag_drag_data_get (widget, context, selection_data,
                             info, time, window, get_data_binder);
}

void
caja_spatial_window_set_location_button  (CajaSpatialWindow *window,
        GFile                 *location)
{
    if (location != NULL)
    {
        CajaFile *file;
        char *name;
        GError *error;

        file = caja_file_get (location);

        /* FIXME: monitor for name change... */
        name = caja_file_get_display_name (file);
        gtk_label_set_label (GTK_LABEL (window->details->location_label),
                             name);
        g_free (name);
        gtk_widget_set_sensitive (window->details->location_button, TRUE);

        error = caja_file_get_file_info_error (file);
        if (error == NULL)
        {
            cairo_surface_t *surface;

            surface = caja_file_get_icon_surface (file,
                                                  caja_get_icon_size_for_stock_size (GTK_ICON_SIZE_MENU),
                                                  TRUE,
                                                  gtk_widget_get_scale_factor (window->details->location_button),
                                                  CAJA_FILE_ICON_FLAGS_IGNORE_VISITING);

            if (surface != NULL)
            {
                gtk_image_set_from_surface (GTK_IMAGE (window->details->location_icon), surface);
                cairo_surface_destroy (surface);
            }
            else
            {
                gtk_image_set_from_icon_name (GTK_IMAGE (window->details->location_icon),
                                              "document-open", GTK_ICON_SIZE_MENU);
            }
        }
        g_object_unref (file);

    }
    else
    {
        gtk_label_set_label (GTK_LABEL (window->details->location_label),
                             "");
        gtk_widget_set_sensitive (window->details->location_button, FALSE);
    }
}

static void
action_go_to_location_callback (GtkAction *action,
                                gpointer user_data)
{
    CajaWindow *window;

    window = CAJA_WINDOW (user_data);

    caja_window_prompt_for_location (window, NULL);
}

static void
action_add_bookmark_callback (GtkAction *action,
                              gpointer user_data)
{
    CajaWindow *window;

    window = CAJA_WINDOW (user_data);

    if (!CAJA_IS_DESKTOP_WINDOW (window))   /* don't bookmark x-caja-desktop:/// */
    {
        caja_window_add_bookmark_for_current_location (window);
    }
}

static void
action_edit_bookmarks_callback (GtkAction *action,
                                gpointer user_data)
{
    caja_window_edit_bookmarks (CAJA_WINDOW (user_data));
}

static void
action_search_callback (GtkAction *action,
                        gpointer user_data)
{
    CajaWindow *window;
    char *uri;
    GFile *f;

    window = CAJA_WINDOW (user_data);

    uri = caja_search_directory_generate_new_uri ();
    f = g_file_new_for_uri (uri);
    caja_window_go_to (window, f);
    g_object_unref (f);
    g_free (uri);
}

static const GtkActionEntry spatial_entries[] =
{
    /* name, icon name, label */ { SPATIAL_ACTION_PLACES, NULL, N_("_Places") },
    /* name, icon name, label */ {
        SPATIAL_ACTION_GO_TO_LOCATION, NULL, N_("Open _Location..."),
        "<control>L", N_("Specify a location to open"),
        G_CALLBACK (action_go_to_location_callback)
    },
    /* name, icon name, label */ {
        SPATIAL_ACTION_CLOSE_PARENT_FOLDERS, NULL, N_("Close P_arent Folders"),
        "<control><shift>W", N_("Close this folder's parents"),
        G_CALLBACK (action_close_parent_folders_callback)
    },
    /* name, icon name, label */ {
        SPATIAL_ACTION_CLOSE_ALL_FOLDERS, NULL, N_("Clos_e All Folders"),
        "<control>Q", N_("Close all folder windows"),
        G_CALLBACK (action_close_all_folders_callback)
    },
    /* name, icon name, label */ { "Add Bookmark", "list-add", N_("_Add Bookmark"),
        "<control>d", N_("Add a bookmark for the current location to this menu"),
        G_CALLBACK (action_add_bookmark_callback)
    },
    /* name, icon name, label */ { "Edit Bookmarks", NULL, N_("_Edit Bookmarks..."),
        "<control>b", N_("Display a window that allows editing the bookmarks in this menu"),
        G_CALLBACK (action_edit_bookmarks_callback)
    },
    /* name, icon name, label */ { "Search", "edit-find", N_("_Search for Files..."),
        "<control>F", N_("Locate documents and folders on this computer by name or content"),
        G_CALLBACK (action_search_callback)
    },
};

static void
caja_spatial_window_init (CajaSpatialWindow *window)
{
    GtkWidget *arrow;
    GtkWidget *hbox, *vbox;
    GtkActionGroup *action_group;
    GtkUIManager *ui_manager;
    GtkTargetList *targets;
    const char *ui;
    CajaWindow *win;
    CajaWindowPane *pane;

    window->details = caja_spatial_window_get_instance_private (window);

    win = CAJA_WINDOW (window);

    gtk_widget_set_hexpand (win->details->statusbar, TRUE);
    gtk_grid_attach (GTK_GRID (win->details->grid),
                     win->details->statusbar,
                     0, 5, 1, 1);
    gtk_widget_show (win->details->statusbar);

    pane = caja_window_pane_new (win);
    win->details->panes = g_list_prepend (win->details->panes, pane);

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_set_homogeneous (GTK_BOX (vbox), TRUE);
    gtk_widget_set_hexpand (vbox, TRUE);
    gtk_widget_set_vexpand (vbox, TRUE);
    gtk_grid_attach (GTK_GRID (CAJA_WINDOW (window)->details->grid),
                     vbox,
                     0, 1, 1, 3);

    gtk_widget_show (vbox);
    window->details->content_box = vbox;

    window->details->location_button = gtk_button_new ();
    g_signal_connect (window->details->location_button,
                      "button-press-event",
                      G_CALLBACK (location_button_pressed_callback),
                      window);
    gtk_button_set_relief (GTK_BUTTON (window->details->location_button),
                           GTK_RELIEF_NORMAL);

    gtk_widget_show (window->details->location_button);
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
    gtk_container_add (GTK_CONTAINER (window->details->location_button),
                       hbox);
    gtk_widget_show (hbox);

    window->details->location_icon = gtk_image_new_from_icon_name ("document-open", GTK_ICON_SIZE_MENU);
    gtk_box_pack_start (GTK_BOX (hbox), window->details->location_icon, FALSE, FALSE, 0);
    gtk_widget_show (window->details->location_icon);

    window->details->location_label = gtk_label_new ("");
    gtk_label_set_ellipsize (GTK_LABEL (window->details->location_label), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars (GTK_LABEL (window->details->location_label), MAX_SHORTNAME_PATH);
    gtk_box_pack_start (GTK_BOX (hbox), window->details->location_label,
                        FALSE, FALSE, 0);
    gtk_widget_show (window->details->location_label);

    arrow = gtk_image_new_from_icon_name ("pan-down-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_box_pack_start (GTK_BOX (hbox), arrow, FALSE, FALSE, 0);
    gtk_widget_show (arrow);

    gtk_drag_source_set (window->details->location_button,
                         GDK_BUTTON1_MASK | GDK_BUTTON2_MASK, location_button_drag_types,
                         G_N_ELEMENTS (location_button_drag_types),
                         GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK | GDK_ACTION_ASK);
    g_signal_connect (window->details->location_button,
                      "drag_begin",
                      G_CALLBACK (location_button_drag_begin_callback),
                      window);
    g_signal_connect (window->details->location_button,
                      "drag_data_get",
                      G_CALLBACK (location_button_drag_data_get_callback),
                      window);

    targets = gtk_drag_source_get_target_list (window->details->location_button);
    gtk_target_list_add_text_targets (targets, CAJA_ICON_DND_TEXT);

    gtk_widget_set_sensitive (window->details->location_button, FALSE);
    g_signal_connect (window->details->location_button,
                      "clicked",
                      G_CALLBACK (location_button_clicked_callback), window);
    gtk_box_pack_start (GTK_BOX (CAJA_WINDOW (window)->details->statusbar),
                        window->details->location_button,
                        FALSE, TRUE, 0);

    gtk_box_reorder_child (GTK_BOX (CAJA_WINDOW (window)->details->statusbar),
                           window->details->location_button, 0);

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    action_group = gtk_action_group_new ("SpatialActions");
    gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
    window->details->spatial_action_group = action_group;
    gtk_action_group_add_actions (action_group,
                                  spatial_entries, G_N_ELEMENTS (spatial_entries),
                                  window);
    G_GNUC_END_IGNORE_DEPRECATIONS;

    ui_manager = caja_window_get_ui_manager (CAJA_WINDOW (window));
    gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
    g_object_unref (action_group); /* owned by ui manager */

    ui = caja_ui_string_get ("caja-spatial-window-ui.xml");
    gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, NULL);

    caja_window_set_active_pane (win, pane);
}

static void
caja_spatial_window_class_init (CajaSpatialWindowClass *klass)
{
    GtkBindingSet *binding_set;
	CajaWindowClass *nclass = CAJA_WINDOW_CLASS (klass);
	GtkWidgetClass *wclass = GTK_WIDGET_CLASS (klass);

	nclass->window_type = CAJA_WINDOW_SPATIAL;
	nclass->bookmarks_placeholder = MENU_PATH_SPATIAL_BOOKMARKS_PLACEHOLDER;
	nclass->prompt_for_location = real_prompt_for_location;
	nclass->get_icon = real_get_icon;
	nclass->sync_title = real_sync_title;
	nclass->get_min_size = real_get_min_size;
	nclass->get_default_size = real_get_default_size;
	nclass->sync_allow_stop = real_sync_allow_stop;
	nclass->set_allow_up = real_set_allow_up;
	nclass->open_slot = real_open_slot;
	nclass->close = real_window_close;
	nclass->close_slot = real_close_slot;

	wclass->show = caja_spatial_window_show;
	wclass->configure_event = caja_spatial_window_configure_event;
	wclass->unrealize = caja_spatial_window_unrealize;
	wclass->window_state_event = caja_spatial_window_state_event;

	G_OBJECT_CLASS (klass)->finalize = caja_spatial_window_finalize;

	binding_set = gtk_binding_set_by_class (klass);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_BackSpace, GDK_SHIFT_MASK,
                                  "go_up", 1,
                                  G_TYPE_BOOLEAN, TRUE);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_Up, GDK_SHIFT_MASK | GDK_MOD1_MASK,
                                  "go_up", 1,
                                  G_TYPE_BOOLEAN, TRUE);
}
