/*
 *  Caja
 *
 *  Copyright (C) 1999, 2000, 2004 Red Hat, Inc.
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
 *           Alexander Larsson <alexl@redhat.com>
 */

/* caja-window.c: Implementation of the main window object */

#include <config.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#ifdef HAVE_X11_XF86KEYSYM_H
#include <X11/XF86keysym.h>
#endif

#include <eel/eel-debug.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-string.h>

#include <libcaja-private/caja-file-utilities.h>
#include <libcaja-private/caja-file-attributes.h>
#include <libcaja-private/caja-global-preferences.h>
#include <libcaja-private/caja-metadata.h>
#include <libcaja-private/caja-mime-actions.h>
#include <libcaja-private/caja-program-choosing.h>
#include <libcaja-private/caja-view-factory.h>
#include <libcaja-private/caja-clipboard.h>
#include <libcaja-private/caja-search-directory.h>
#include <libcaja-private/caja-signaller.h>

#include "caja-window-private.h"
#include "caja-actions.h"
#include "caja-application.h"
#include "caja-bookmarks-window.h"
#include "caja-information-panel.h"
#include "caja-window-manage-views.h"
#include "caja-window-bookmarks.h"
#include "caja-window-slot.h"
#include "caja-navigation-window-slot.h"
#include "caja-search-bar.h"
#include "caja-navigation-window-pane.h"
#include "caja-src-marshal.h"

#define MAX_HISTORY_ITEMS 50

#define EXTRA_VIEW_WIDGETS_BACKGROUND "#a7c6e1"

/* dock items */

#define CAJA_MENU_PATH_EXTRA_VIEWER_PLACEHOLDER	"/MenuBar/View/View Choices/Extra Viewer"
#define CAJA_MENU_PATH_SHORT_LIST_PLACEHOLDER  	"/MenuBar/View/View Choices/Short List"

enum {
	ARG_0,
	ARG_APP
};

enum {
	GO_UP,
	RELOAD,
	PROMPT_FOR_LOCATION,
	ZOOM_CHANGED,
	VIEW_AS_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

typedef struct
{
    CajaWindow *window;
    char *id;
} ActivateViewData;

static void cancel_view_as_callback         (CajaWindowSlot      *slot);
static void caja_window_info_iface_init (CajaWindowInfoIface *iface);
static void action_view_as_callback         (GtkAction               *action,
        ActivateViewData        *data);

static GList *history_list;

G_DEFINE_TYPE_WITH_CODE (CajaWindow, caja_window, GTK_TYPE_WINDOW,
                         G_ADD_PRIVATE (CajaWindow)
                         G_IMPLEMENT_INTERFACE (CAJA_TYPE_WINDOW_INFO,
                                 caja_window_info_iface_init));

static const struct
{
    unsigned int keyval;
    const char *action;
} extra_window_keybindings [] =
{
#ifdef HAVE_X11_XF86KEYSYM_H
    { XF86XK_AddFavorite,	CAJA_ACTION_ADD_BOOKMARK },
    { XF86XK_Favorites,	CAJA_ACTION_EDIT_BOOKMARKS },
    { XF86XK_Go,		CAJA_ACTION_GO_TO_LOCATION },
    /* TODO?{ XF86XK_History,	CAJA_ACTION_HISTORY }, */
    { XF86XK_HomePage,      CAJA_ACTION_GO_HOME },
    { XF86XK_OpenURL,	CAJA_ACTION_GO_TO_LOCATION },
    { XF86XK_Refresh,	CAJA_ACTION_RELOAD },
    { XF86XK_Reload,	CAJA_ACTION_RELOAD },
    { XF86XK_Search,	CAJA_ACTION_SEARCH },
    { XF86XK_Start,		CAJA_ACTION_GO_HOME },
    { XF86XK_Stop,		CAJA_ACTION_STOP },
    { XF86XK_ZoomIn,	CAJA_ACTION_ZOOM_IN },
    { XF86XK_ZoomOut,	CAJA_ACTION_ZOOM_OUT }
#endif
};

static void
caja_window_init (CajaWindow *window)
{
    GtkWidget *grid;
    GtkWidget *menu;
    GtkWidget *statusbar;

    static const gchar css_custom[] =
      "#caja-extra-view-widget {"
      "  background-color: " EXTRA_VIEW_WIDGETS_BACKGROUND ";"
      "}";

    GError *error = NULL;
    GtkCssProvider *provider = gtk_css_provider_new ();
    gtk_css_provider_load_from_data (provider, css_custom, -1, &error);

    if (error != NULL) {
            g_warning ("Can't parse CajaWindow's CSS custom description: %s\n", error->message);
            g_error_free (error);
    } else {
            gtk_style_context_add_provider (gtk_widget_get_style_context (GTK_WIDGET (window)),
                                            GTK_STYLE_PROVIDER (provider),
                                            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }

    g_object_unref (provider);
    window->details = caja_window_get_instance_private (window);

    window->details->panes = NULL;
    window->details->active_pane = NULL;

    window->details->show_hidden_files_mode = CAJA_WINDOW_SHOW_HIDDEN_FILES_DEFAULT;

    /* Set initial window title */
    gtk_window_set_title (GTK_WINDOW (window), _("Caja"));

    grid = gtk_grid_new ();
    gtk_orientable_set_orientation (GTK_ORIENTABLE (grid), GTK_ORIENTATION_VERTICAL);
    window->details->grid = grid;
    gtk_widget_show (grid);
    gtk_container_add (GTK_CONTAINER (window), grid);

    statusbar = gtk_statusbar_new ();
    gtk_widget_set_name (statusbar, "statusbar-noborder");

/* set margin to zero to reduce size of statusbar */
	gtk_widget_set_margin_top (GTK_WIDGET (statusbar), 0);
	gtk_widget_set_margin_bottom (GTK_WIDGET (statusbar), 0);

    window->details->statusbar = statusbar;
    window->details->help_message_cid = gtk_statusbar_get_context_id
                                        (GTK_STATUSBAR (statusbar), "help_message");
    /* Statusbar is packed in the subclasses */

    caja_window_initialize_menus (window);

    menu = gtk_ui_manager_get_widget (window->details->ui_manager, "/MenuBar");
    window->details->menubar = menu;
    gtk_widget_set_hexpand (menu, TRUE);
    gtk_widget_show (menu);
    gtk_grid_attach (GTK_GRID (grid), menu, 0, 0, 1, 1);

    /* Register to menu provider extension signal managing menu updates */
    g_signal_connect_object (caja_signaller_get_current (), "popup_menu_changed",
                             G_CALLBACK (caja_window_load_extension_menus), window, G_CONNECT_SWAPPED);
}

/* Unconditionally synchronize the GtkUIManager of WINDOW. */
static void
caja_window_ui_update (CajaWindow *window)
{
    g_assert (CAJA_IS_WINDOW (window));

    gtk_ui_manager_ensure_update (window->details->ui_manager);
}

static void
caja_window_push_status (CajaWindow *window,
                         const char *text)
{
    g_return_if_fail (CAJA_IS_WINDOW (window));

    /* clear any previous message, underflow is allowed */
    gtk_statusbar_pop (GTK_STATUSBAR (window->details->statusbar), 0);

    if (text != NULL && text[0] != '\0')
    {
        gtk_statusbar_push (GTK_STATUSBAR (window->details->statusbar), 0, text);
    }
}

void
caja_window_sync_status (CajaWindow *window)
{
    CajaWindowSlot *slot;

    slot = window->details->active_pane->active_slot;
    caja_window_push_status (window, slot->status_text);
}

void
caja_window_go_to (CajaWindow *window, GFile *location)
{
    g_return_if_fail (CAJA_IS_WINDOW (window));

    caja_window_slot_go_to (window->details->active_pane->active_slot, location, FALSE);
}

void
caja_window_go_to_tab (CajaWindow *window, GFile *location)
{
    g_return_if_fail (CAJA_IS_WINDOW (window));

    caja_window_slot_go_to (window->details->active_pane->active_slot, location, TRUE);
}

void
caja_window_go_to_full (CajaWindow *window,
                        GFile                 *location,
                        CajaWindowGoToCallback callback,
                        gpointer               user_data)
{
    g_return_if_fail (CAJA_IS_WINDOW (window));

    caja_window_slot_go_to_full (window->details->active_pane->active_slot, location, FALSE, callback, user_data);
}

void
caja_window_go_to_with_selection (CajaWindow *window, GFile *location, GList *new_selection)
{
    g_return_if_fail (CAJA_IS_WINDOW (window));

    caja_window_slot_go_to_with_selection (window->details->active_pane->active_slot, location, new_selection);
}

static gboolean
caja_window_go_up_signal (CajaWindow *window, gboolean close_behind)
{
    caja_window_go_up (window, close_behind, FALSE);
    return TRUE;
}

void
caja_window_new_tab (CajaWindow *window)
{
    CajaWindowSlot *current_slot;
    CajaWindowOpenFlags flags;
    GFile *location = NULL;

    current_slot = window->details->active_pane->active_slot;
    location = caja_window_slot_get_location (current_slot);

    if (location != NULL) {
        CajaWindowSlot *new_slot;
        int new_slot_position;
        char *scheme;

    	flags = 0;

    	new_slot_position = g_settings_get_enum (caja_preferences, CAJA_PREFERENCES_NEW_TAB_POSITION);
    	if (new_slot_position == CAJA_NEW_TAB_POSITION_END) {
    		flags = CAJA_WINDOW_OPEN_SLOT_APPEND;
    	}

    	scheme = g_file_get_uri_scheme (location);
    	if (!strcmp (scheme, "x-caja-search")) {
    		g_object_unref (location);
    		location = g_file_new_for_path (g_get_home_dir ());
    	}
    	g_free (scheme);

    	new_slot = caja_window_open_slot (current_slot->pane, flags);
    	caja_window_set_active_slot (window, new_slot);
    	caja_window_slot_go_to (new_slot, location, FALSE);
    	g_object_unref (location);
    }
}

/*Opens a new window when called from an existing window and goes to the same location that's in the existing window.*/
void
caja_window_new_window (CajaWindow *window)
{
    CajaWindowSlot *current_slot;
    GFile *location = NULL;
    g_return_if_fail (CAJA_IS_WINDOW (window));

    /*Get and set the directory location of current window (slot).*/
    current_slot = window->details->active_pane->active_slot;
    location = caja_window_slot_get_location (current_slot);

    if (location != NULL) 
    {
        CajaWindow *new_window;
        CajaWindowSlot *new_slot;
        CajaWindowOpenFlags flags;
        flags = FALSE;

        /*Create a new window*/
        new_window = caja_application_create_navigation_window (
                     window->application,
        gtk_window_get_screen (GTK_WINDOW (window)));

        /*Create a slot in the new window.*/
        new_slot = new_window->details->active_pane->active_slot;
        g_return_if_fail (CAJA_IS_WINDOW_SLOT (new_slot));

        /*Open a directory at the set location in the new window (slot).*/
        caja_window_slot_open_location_full (new_slot, location,
                                             CAJA_WINDOW_OPEN_ACCORDING_TO_MODE,
                                             flags, NULL, NULL, NULL);
        g_object_unref (location);
    }
}

void
caja_window_go_up (CajaWindow *window, gboolean close_behind, gboolean new_tab)
{
    CajaWindowSlot *slot;
    GFile *parent;
    GList *selection;
    CajaWindowOpenFlags flags;

    g_assert (CAJA_IS_WINDOW (window));

    slot = window->details->active_pane->active_slot;

    if (slot->location == NULL)
    {
        return;
    }

    parent = g_file_get_parent (slot->location);

    if (parent == NULL)
    {
        return;
    }

    selection = g_list_prepend (NULL, g_object_ref (slot->location));

    flags = 0;
    if (close_behind)
    {
        flags |= CAJA_WINDOW_OPEN_FLAG_CLOSE_BEHIND;
    }
    if (new_tab)
    {
        flags |= CAJA_WINDOW_OPEN_FLAG_NEW_TAB;
    }

    caja_window_slot_open_location_full (slot, parent,
                                         CAJA_WINDOW_OPEN_ACCORDING_TO_MODE,
                                         flags,
                                         selection,
                                         NULL, NULL);

    g_object_unref (parent);

    g_list_free_full (selection, g_object_unref);
}

static void
real_set_allow_up (CajaWindow *window,
                   gboolean        allow)
{
    GtkAction *action;

    g_assert (CAJA_IS_WINDOW (window));

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    action = gtk_action_group_get_action (window->details->main_action_group,
                                          CAJA_ACTION_UP);
    gtk_action_set_sensitive (action, allow);
    action = gtk_action_group_get_action (window->details->main_action_group,
                                          CAJA_ACTION_UP_ACCEL);
    gtk_action_set_sensitive (action, allow);
    G_GNUC_END_IGNORE_DEPRECATIONS;
}

void
caja_window_allow_up (CajaWindow *window, gboolean allow)
{
    g_return_if_fail (CAJA_IS_WINDOW (window));

    EEL_CALL_METHOD (CAJA_WINDOW_CLASS, window,
                     set_allow_up, (window, allow));
}

static void
update_cursor (CajaWindow *window)
{
    CajaWindowSlot *slot;

    slot = window->details->active_pane->active_slot;

    if (slot->allow_stop)
    {
        GdkDisplay *display;
        GdkCursor * cursor;

        display = gtk_widget_get_display (GTK_WIDGET (window));
        cursor = gdk_cursor_new_for_display (display, GDK_WATCH);
        gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (window)), cursor);
        g_object_unref (cursor);
    }
    else
    {
        gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (window)), NULL);
    }
}

void
caja_window_sync_allow_stop (CajaWindow *window,
                             CajaWindowSlot *slot)
{
    GtkAction *action;
    gboolean allow_stop;

    g_assert (CAJA_IS_WINDOW (window));

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    action = gtk_action_group_get_action (window->details->main_action_group,
                                          CAJA_ACTION_STOP);
    allow_stop = gtk_action_get_sensitive (action);
    G_GNUC_END_IGNORE_DEPRECATIONS;

    if (slot != window->details->active_pane->active_slot ||
            allow_stop != slot->allow_stop)
    {
        if (slot == window->details->active_pane->active_slot)
        {
            G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
            gtk_action_set_sensitive (action, slot->allow_stop);
            G_GNUC_END_IGNORE_DEPRECATIONS;
        }

        if (gtk_widget_get_realized (GTK_WIDGET (window)))
        {
            update_cursor (window);
        }

        EEL_CALL_METHOD (CAJA_WINDOW_CLASS, window,
                         sync_allow_stop, (window, slot));
    }
}

void
caja_window_allow_reload (CajaWindow *window, gboolean allow)
{
    GtkAction *action;

    g_return_if_fail (CAJA_IS_WINDOW (window));

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    action = gtk_action_group_get_action (window->details->main_action_group,
                                          CAJA_ACTION_RELOAD);
    gtk_action_set_sensitive (action, allow);
    G_GNUC_END_IGNORE_DEPRECATIONS;
}

void
caja_window_go_home (CajaWindow *window)
{
    g_return_if_fail (CAJA_IS_WINDOW (window));

    caja_window_slot_go_home (window->details->active_pane->active_slot, FALSE);
}

void
caja_window_prompt_for_location (CajaWindow *window,
                                 const char     *initial)
{
    g_return_if_fail (CAJA_IS_WINDOW (window));

    EEL_CALL_METHOD (CAJA_WINDOW_CLASS, window,
                     prompt_for_location, (window, initial));
}

static char *
caja_window_get_location_uri (CajaWindow *window)
{
    CajaWindowSlot *slot;

    g_assert (CAJA_IS_WINDOW (window));

    slot = window->details->active_pane->active_slot;

    if (slot->location)
    {
        return g_file_get_uri (slot->location);
    }
    return NULL;
}

void
caja_window_zoom_in (CajaWindow *window)
{
    g_assert (window != NULL);

    caja_window_pane_zoom_in (window->details->active_pane);
}

void
caja_window_zoom_to_level (CajaWindow *window,
                           CajaZoomLevel level)
{
    g_assert (window != NULL);

    caja_window_pane_zoom_to_level (window->details->active_pane, level);
}

void
caja_window_zoom_out (CajaWindow *window)
{
    g_assert (window != NULL);

    caja_window_pane_zoom_out (window->details->active_pane);
}

void
caja_window_zoom_to_default (CajaWindow *window)
{
    g_assert (window != NULL);

    caja_window_pane_zoom_to_default (window->details->active_pane);
}

/* Code should never force the window taller than this size.
 * (The user can still stretch the window taller if desired).
 */
static guint
get_max_forced_height (GdkScreen *screen)
{
    gint scale = gdk_window_get_scale_factor (gdk_screen_get_root_window (screen));
    return (HeightOfScreen (gdk_x11_screen_get_xscreen (screen)) / scale * 90) / 100;
}

/* Code should never force the window wider than this size.
 * (The user can still stretch the window wider if desired).
 */
static guint
get_max_forced_width (GdkScreen *screen)
{
    gint scale = gdk_window_get_scale_factor (gdk_screen_get_root_window (screen));
    return (WidthOfScreen (gdk_x11_screen_get_xscreen (screen)) / scale * 90) / 100;
}

/* This must be called when construction of CajaWindow is finished,
 * since it depends on the type of the argument, which isn't decided at
 * construction time.
 */
static void
caja_window_set_initial_window_geometry (CajaWindow *window)
{
    GdkScreen *screen;
    guint max_width_for_screen, max_height_for_screen;

    guint default_width = 0;
    guint default_height = 0;

    screen = gtk_window_get_screen (GTK_WINDOW (window));

    max_width_for_screen = get_max_forced_width (screen);
    max_height_for_screen = get_max_forced_height (screen);

    EEL_CALL_METHOD (CAJA_WINDOW_CLASS, window,
                     get_default_size, (window, &default_width, &default_height));

    gtk_window_set_default_size (GTK_WINDOW (window),
                                 MIN (default_width,
                                      max_width_for_screen),
                                 MIN (default_height,
                                      max_height_for_screen));
}

static void
caja_window_constructed (GObject *self)
{
    CajaWindow *window;

    window = CAJA_WINDOW (self);

    caja_window_initialize_bookmarks_menu (window);
    caja_window_set_initial_window_geometry (window);
}

static void
caja_window_set_property (GObject *object,
                          guint arg_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
    CajaWindow *window;

    window = CAJA_WINDOW (object);

    switch (arg_id)
    {
    case ARG_APP:
        window->application = CAJA_APPLICATION (g_value_get_object (value));
        break;
    }
}

static void
caja_window_get_property (GObject *object,
                          guint arg_id,
                          GValue *value,
                          GParamSpec *pspec)
{
    switch (arg_id)
    {
    case ARG_APP:
        g_value_set_object (value, CAJA_WINDOW (object)->application);
        break;
    }
}

static void
free_stored_viewers (CajaWindow *window)
{
    g_list_free_full (window->details->short_list_viewers, g_free);
    window->details->short_list_viewers = NULL;
    g_free (window->details->extra_viewer);
    window->details->extra_viewer = NULL;
}

static void
caja_window_destroy (GtkWidget *object)
{
    CajaWindow *window;
    GList *panes_copy;

    window = CAJA_WINDOW (object);

    /* close all panes safely */
    panes_copy = g_list_copy (window->details->panes);
    g_list_free_full (panes_copy, (GDestroyNotify) caja_window_close_pane);

    /* the panes list should now be empty */
    g_assert (window->details->panes == NULL);
    g_assert (window->details->active_pane == NULL);

    GTK_WIDGET_CLASS (caja_window_parent_class)->destroy (object);
}

static void
caja_window_finalize (GObject *object)
{
    CajaWindow *window;

    window = CAJA_WINDOW (object);

    caja_window_finalize_menus (window);
    free_stored_viewers (window);

    if (window->details->bookmark_list != NULL)
    {
        g_object_unref (window->details->bookmark_list);
    }

    /* caja_window_close() should have run */
    g_assert (window->details->panes == NULL);

    g_object_unref (window->details->ui_manager);

    G_OBJECT_CLASS (caja_window_parent_class)->finalize (object);
}

static GObject *
caja_window_constructor (GType type,
                         guint n_construct_properties,
                         GObjectConstructParam *construct_params)
{
    GObject *object;
    CajaWindow *window;
    CajaWindowSlot *slot;

    object = (* G_OBJECT_CLASS (caja_window_parent_class)->constructor) (type,
             n_construct_properties,
             construct_params);

    window = CAJA_WINDOW (object);

    slot = caja_window_open_slot (window->details->active_pane, 0);
    caja_window_set_active_slot (window, slot);

    return object;
}

void
caja_window_show_window (CajaWindow    *window)
{
    CajaWindowSlot *slot;
    CajaWindowPane *pane;
    GList *l, *walk;

    for (walk = window->details->panes; walk; walk = walk->next)
    {
        pane = walk->data;
        for (l = pane->slots; l != NULL; l = l->next)
        {
            slot = l->data;

            caja_window_slot_update_title (slot);
            caja_window_slot_update_icon (slot);
        }
    }

    gtk_widget_show (GTK_WIDGET (window));

    slot = window->details->active_pane->active_slot;

    if (slot->viewed_file)
    {
        if (CAJA_IS_SPATIAL_WINDOW (window))
        {
            caja_file_set_has_open_window (slot->viewed_file, TRUE);
        }
    }
}

static void
caja_window_view_visible (CajaWindow *window,
                          CajaView *view)
{
    CajaWindowSlot *slot;
    CajaWindowPane *pane;
    GList *l, *walk;

    g_return_if_fail (CAJA_IS_WINDOW (window));

    slot = caja_window_get_slot_for_view (window, view);

    /* Ensure we got the right active state for newly added panes */
    caja_window_slot_is_in_active_pane (slot, slot->pane->is_active);

    if (slot->visible)
    {
        return;
    }

    slot->visible = TRUE;

    pane = slot->pane;

    if (pane->visible)
    {
        return;
    }

    /* Look for other non-visible slots */
    for (l = pane->slots; l != NULL; l = l->next)
    {
        slot = l->data;

        if (!slot->visible)
        {
            return;
        }
    }

    /* None, this pane is visible */
    caja_window_pane_show (pane);

    /* Look for other non-visible panes */
    for (walk = window->details->panes; walk; walk = walk->next)
    {
        pane = walk->data;

        if (!pane->visible)
        {
            return;
        }
    }

    caja_window_pane_grab_focus (window->details->active_pane);

    /* All slots and panes visible, show window */
    caja_window_show_window (window);
}

void
caja_window_close (CajaWindow *window)
{
    g_return_if_fail (CAJA_IS_WINDOW (window));

    EEL_CALL_METHOD (CAJA_WINDOW_CLASS, window,
                     close, (window));

    gtk_widget_destroy (GTK_WIDGET (window));
}

CajaWindowSlot *
caja_window_open_slot (CajaWindowPane *pane,
                       CajaWindowOpenSlotFlags flags)
{
    CajaWindowSlot *slot;

    g_assert (CAJA_IS_WINDOW_PANE (pane));
    g_assert (CAJA_IS_WINDOW (pane->window));

    slot = EEL_CALL_METHOD_WITH_RETURN_VALUE (CAJA_WINDOW_CLASS, pane->window,
            open_slot, (pane, flags));

    g_assert (CAJA_IS_WINDOW_SLOT (slot));
    g_assert (pane->window == slot->pane->window);

    pane->slots = g_list_append (pane->slots, slot);

    return slot;
}

void
caja_window_close_pane (CajaWindowPane *pane)
{
    CajaWindow *window;

    g_assert (CAJA_IS_WINDOW_PANE (pane));
    g_assert (CAJA_IS_WINDOW (pane->window));
    g_assert (g_list_find (pane->window->details->panes, pane) != NULL);

    while (pane->slots != NULL)
    {
        CajaWindowSlot *slot = pane->slots->data;

        caja_window_close_slot (slot);
    }

    window = pane->window;

    /* If the pane was active, set it to NULL. The caller is responsible
     * for setting a new active pane with caja_window_pane_switch_to()
     * if it wants to continue using the window. */
    if (window->details->active_pane == pane)
    {
        window->details->active_pane = NULL;
    }

    window->details->panes = g_list_remove (window->details->panes, pane);

    g_object_unref (pane);
}

static void
real_close_slot (CajaWindowPane *pane,
                 CajaWindowSlot *slot)
{
    caja_window_manage_views_close_slot (pane, slot);
    cancel_view_as_callback (slot);
}

void
caja_window_close_slot (CajaWindowSlot *slot)
{
    CajaWindowPane *pane;

    g_assert (CAJA_IS_WINDOW_SLOT (slot));
    g_assert (CAJA_IS_WINDOW_PANE(slot->pane));
    g_assert (g_list_find (slot->pane->slots, slot) != NULL);

    /* save pane because slot is not valid anymore after this call */
    pane = slot->pane;

    EEL_CALL_METHOD (CAJA_WINDOW_CLASS, slot->pane->window,
                     close_slot, (slot->pane, slot));

    g_object_run_dispose (G_OBJECT (slot));
    slot->pane = NULL;
    g_object_unref (slot);
    pane->slots = g_list_remove (pane->slots, slot);
    pane->active_slots = g_list_remove (pane->active_slots, slot);

}

CajaWindowPane*
caja_window_get_active_pane (CajaWindow *window)
{
    g_assert (CAJA_IS_WINDOW (window));
    return window->details->active_pane;
}

static void
real_set_active_pane (CajaWindow *window, CajaWindowPane *new_pane)
{
    /* make old pane inactive, and new one active.
     * Currently active pane may be NULL (after init). */
    if (window->details->active_pane &&
            window->details->active_pane != new_pane)
    {
        caja_window_pane_set_active (new_pane->window->details->active_pane, FALSE);
    }
    caja_window_pane_set_active (new_pane, TRUE);

    window->details->active_pane = new_pane;
}

/* Make the given pane the active pane of its associated window. This
 * always implies making the containing active slot the active slot of
 * the window. */
void
caja_window_set_active_pane (CajaWindow *window,
                             CajaWindowPane *new_pane)
{
    g_assert (CAJA_IS_WINDOW_PANE (new_pane));
    if (new_pane->active_slot)
    {
        caja_window_set_active_slot (window, new_pane->active_slot);
    }
    else if (new_pane != window->details->active_pane)
    {
        real_set_active_pane (window, new_pane);
    }
}

/* Make both, the given slot the active slot and its corresponding
 * pane the active pane of the associated window.
 * new_slot may be NULL. */
void
caja_window_set_active_slot (CajaWindow *window, CajaWindowSlot *new_slot)
{
    CajaWindowSlot *old_slot;

    g_assert (CAJA_IS_WINDOW (window));

    if (new_slot)
    {
        g_assert (CAJA_IS_WINDOW_SLOT (new_slot));
        g_assert (CAJA_IS_WINDOW_PANE (new_slot->pane));
        g_assert (window == new_slot->pane->window);
        g_assert (g_list_find (new_slot->pane->slots, new_slot) != NULL);
    }

    if (window->details->active_pane != NULL)
    {
        old_slot = window->details->active_pane->active_slot;
    }
    else
    {
        old_slot = NULL;
    }

    if (old_slot == new_slot)
    {
        return;
    }

    /* make old slot inactive if it exists (may be NULL after init, for example) */
    if (old_slot != NULL)
    {
        /* inform window */
        if (old_slot->content_view != NULL)
        {
            caja_window_slot_disconnect_content_view (old_slot, old_slot->content_view);
        }

        /* inform slot & view */
        g_signal_emit_by_name (old_slot, "inactive");
    }

    /* deal with panes */
    if (new_slot &&
            new_slot->pane != window->details->active_pane)
    {
        real_set_active_pane (window, new_slot->pane);
    }

    window->details->active_pane->active_slot = new_slot;

    /* make new slot active, if it exists */
    if (new_slot)
    {
        window->details->active_pane->active_slots =
            g_list_remove (window->details->active_pane->active_slots, new_slot);
        window->details->active_pane->active_slots =
            g_list_prepend (window->details->active_pane->active_slots, new_slot);

        /* inform sidebar panels */
        caja_window_report_location_change (window);
        /* TODO decide whether "selection-changed" should be emitted */

        if (new_slot->content_view != NULL)
        {
            /* inform window */
            caja_window_slot_connect_content_view (new_slot, new_slot->content_view);
        }

        /* inform slot & view */
        g_signal_emit_by_name (new_slot, "active");
    }
}

void
caja_window_slot_close (CajaWindowSlot *slot)
{
    caja_window_pane_slot_close (slot->pane, slot);
}

static void
caja_window_realize (GtkWidget *widget)
{
    GTK_WIDGET_CLASS (caja_window_parent_class)->realize (widget);
    update_cursor (CAJA_WINDOW (widget));
}

static gboolean
caja_window_key_press_event (GtkWidget *widget,
                             GdkEventKey *event)
{
    /* Fix for https://github.com/mate-desktop/caja/issues/1024 */
    if ((event->state & GDK_CONTROL_MASK) &&
        ((event->keyval == '.') || (event->keyval == ';')))
        return TRUE;

    CajaWindow *window;
    int i;

    window = CAJA_WINDOW (widget);

    for (i = 0; i < G_N_ELEMENTS (extra_window_keybindings); i++)
    {
        if (extra_window_keybindings[i].keyval == event->keyval)
        {
            const GList *action_groups;
            GtkAction *action;

            action = NULL;

            action_groups = gtk_ui_manager_get_action_groups (window->details->ui_manager);
            G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
            while (action_groups != NULL && action == NULL)
            {
                action = gtk_action_group_get_action (action_groups->data, extra_window_keybindings[i].action);
                action_groups = action_groups->next;
            }

            g_assert (action != NULL);
            if (gtk_action_is_sensitive (action))
            {
                gtk_action_activate (action);
                return TRUE;
            }
            G_GNUC_END_IGNORE_DEPRECATIONS;

            break;
        }
    }

    return GTK_WIDGET_CLASS (caja_window_parent_class)->key_press_event (widget, event);
}

/*
 * Main API
 */

static void
free_activate_view_data (gpointer data)
{
    ActivateViewData *activate_data;

    activate_data = data;

    g_free (activate_data->id);

    g_slice_free (ActivateViewData, activate_data);
}

static void
action_view_as_callback (GtkAction *action,
                         ActivateViewData *data)
{
    CajaWindow *window;

    window = data->window;

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)))
    {
        CajaWindowSlot *slot;

        slot = window->details->active_pane->active_slot;
        caja_window_slot_set_content_view (slot,
                                           data->id);
    }
    G_GNUC_END_IGNORE_DEPRECATIONS;
}

static GtkRadioAction *
add_view_as_menu_item (CajaWindow *window,
                       const char *placeholder_path,
                       const char *identifier,
                       int index, /* extra_viewer is always index 0 */
                       guint merge_id)
{
    const CajaViewInfo *info;
    GtkRadioAction *action;
    char action_name[32];
    ActivateViewData *data;

    info = caja_view_factory_lookup (identifier);

    g_snprintf (action_name, sizeof (action_name), "view_as_%d", index);
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    action = gtk_radio_action_new (action_name,
                                   _(info->view_menu_label_with_mnemonic),
                                   _(info->display_location_label),
                                   NULL,
                                   0);
    G_GNUC_END_IGNORE_DEPRECATIONS;

    if (index >= 1 && index <= 9)
    {
        char accel[32];
        char accel_path[48];
        unsigned int accel_keyval;

        g_snprintf (accel, sizeof (accel), "%d", index);
        g_snprintf (accel_path, sizeof (accel_path), "<Caja-Window>/%s", action_name);

        accel_keyval = gdk_keyval_from_name (accel);
		g_assert (accel_keyval != GDK_KEY_VoidSymbol);

        gtk_accel_map_add_entry (accel_path, accel_keyval, GDK_CONTROL_MASK);
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
        gtk_action_set_accel_path (GTK_ACTION (action), accel_path);
        G_GNUC_END_IGNORE_DEPRECATIONS;
    }

    if (window->details->view_as_radio_action != NULL)
    {
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
        gtk_radio_action_set_group (action,
                                    gtk_radio_action_get_group (window->details->view_as_radio_action));
        G_GNUC_END_IGNORE_DEPRECATIONS;
    }
    else if (index != 0)
    {
        /* Index 0 is the extra view, and we don't want to use that here,
           as it can get deleted/changed later */
        window->details->view_as_radio_action = action;
    }

    data = g_slice_new (ActivateViewData);
    data->window = window;
    data->id = g_strdup (identifier);
    g_signal_connect_data (action, "activate",
                           G_CALLBACK (action_view_as_callback),
                           data, (GClosureNotify) free_activate_view_data, 0);

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    gtk_action_group_add_action (window->details->view_as_action_group,
                                 GTK_ACTION (action));
    g_object_unref (action);
    G_GNUC_END_IGNORE_DEPRECATIONS;

    gtk_ui_manager_add_ui (window->details->ui_manager,
                           merge_id,
                           placeholder_path,
                           action_name,
                           action_name,
                           GTK_UI_MANAGER_MENUITEM,
                           FALSE);

    return action; /* return value owned by group */
}

/* Make a special first item in the "View as" option menu that represents
 * the current content view. This should only be called if the current
 * content view isn't already in the "View as" option menu.
 */
static void
update_extra_viewer_in_view_as_menus (CajaWindow *window,
                                      const char *id)
{
    gboolean had_extra_viewer;

    had_extra_viewer = window->details->extra_viewer != NULL;

    if (id == NULL)
    {
        if (!had_extra_viewer)
        {
            return;
        }
    }
    else
    {
        if (had_extra_viewer
                && strcmp (window->details->extra_viewer, id) == 0)
        {
            return;
        }
    }
    g_free (window->details->extra_viewer);
    window->details->extra_viewer = g_strdup (id);

    if (window->details->extra_viewer_merge_id != 0)
    {
        gtk_ui_manager_remove_ui (window->details->ui_manager,
                                  window->details->extra_viewer_merge_id);
        window->details->extra_viewer_merge_id = 0;
    }

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    if (window->details->extra_viewer_radio_action != NULL)
    {
        gtk_action_group_remove_action (window->details->view_as_action_group,
                                        GTK_ACTION (window->details->extra_viewer_radio_action));
        window->details->extra_viewer_radio_action = NULL;
    }
    G_GNUC_END_IGNORE_DEPRECATIONS;

    if (id != NULL)
    {
        window->details->extra_viewer_merge_id = gtk_ui_manager_new_merge_id (window->details->ui_manager);
        window->details->extra_viewer_radio_action =
            add_view_as_menu_item (window,
                                   CAJA_MENU_PATH_EXTRA_VIEWER_PLACEHOLDER,
                                   window->details->extra_viewer,
                                   0,
                                   window->details->extra_viewer_merge_id);
    }
}

static void
remove_extra_viewer_in_view_as_menus (CajaWindow *window)
{
    update_extra_viewer_in_view_as_menus (window, NULL);
}

static void
replace_extra_viewer_in_view_as_menus (CajaWindow *window)
{
    CajaWindowSlot *slot;
    const char *id;

    slot = window->details->active_pane->active_slot;

    id = caja_window_slot_get_content_view_id (slot);
    update_extra_viewer_in_view_as_menus (window, id);
}

/**
 * caja_window_synch_view_as_menus:
 *
 * Set the visible item of the "View as" option menu and
 * the marked "View as" item in the View menu to
 * match the current content view.
 *
 * @window: The CajaWindow whose "View as" option menu should be synched.
 */
static void
caja_window_synch_view_as_menus (CajaWindow *window)
{
    CajaWindowSlot *slot;
    int index;
    char action_name[32];
    GList *node;
    GtkAction *action;

    g_assert (CAJA_IS_WINDOW (window));

    slot = window->details->active_pane->active_slot;

    if (slot->content_view == NULL)
    {
        return;
    }
    for (node = window->details->short_list_viewers, index = 1;
            node != NULL;
            node = node->next, ++index)
    {
        if (caja_window_slot_content_view_matches_iid (slot, (char *)node->data))
        {
            break;
        }
    }
    if (node == NULL)
    {
        replace_extra_viewer_in_view_as_menus (window);
        index = 0;
    }
    else
    {
        remove_extra_viewer_in_view_as_menus (window);
    }

    g_snprintf (action_name, sizeof (action_name), "view_as_%d", index);
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    action = gtk_action_group_get_action (window->details->view_as_action_group,
                                          action_name);
    G_GNUC_END_IGNORE_DEPRECATIONS;

    /* Don't trigger the action callback when we're synchronizing */
    g_signal_handlers_block_matched (action,
                                     G_SIGNAL_MATCH_FUNC,
                                     0, 0,
                                     NULL,
                                     action_view_as_callback,
                                     NULL);
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
    G_GNUC_END_IGNORE_DEPRECATIONS;
    g_signal_handlers_unblock_matched (action,
                                       G_SIGNAL_MATCH_FUNC,
                                       0, 0,
                                       NULL,
                                       action_view_as_callback,
                                       NULL);
}

static void
refresh_stored_viewers (CajaWindow *window)
{
    CajaWindowSlot *slot;
    GList *viewers;
    char *uri, *mimetype;

    slot = window->details->active_pane->active_slot;

    uri = caja_file_get_uri (slot->viewed_file);
    mimetype = caja_file_get_mime_type (slot->viewed_file);
    viewers = caja_view_factory_get_views_for_uri (uri,
              caja_file_get_file_type (slot->viewed_file),
              mimetype);
    g_free (uri);
    g_free (mimetype);

    free_stored_viewers (window);
    window->details->short_list_viewers = viewers;
}

static void
load_view_as_menu (CajaWindow *window)
{
    GList *node;
    guint merge_id;

    if (window->details->short_list_merge_id != 0)
    {
        gtk_ui_manager_remove_ui (window->details->ui_manager,
                                  window->details->short_list_merge_id);
        window->details->short_list_merge_id = 0;
    }
    if (window->details->extra_viewer_merge_id != 0)
    {
        gtk_ui_manager_remove_ui (window->details->ui_manager,
                                  window->details->extra_viewer_merge_id);
        window->details->extra_viewer_merge_id = 0;
        window->details->extra_viewer_radio_action = NULL;
    }
    if (window->details->view_as_action_group != NULL)
    {
        gtk_ui_manager_remove_action_group (window->details->ui_manager,
                                            window->details->view_as_action_group);
        window->details->view_as_action_group = NULL;
    }


    refresh_stored_viewers (window);

    merge_id = gtk_ui_manager_new_merge_id (window->details->ui_manager);
    window->details->short_list_merge_id = merge_id;
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    window->details->view_as_action_group = gtk_action_group_new ("ViewAsGroup");
    gtk_action_group_set_translation_domain (window->details->view_as_action_group, GETTEXT_PACKAGE);
    G_GNUC_END_IGNORE_DEPRECATIONS;
    window->details->view_as_radio_action = NULL;

    if (g_list_length (window->details->short_list_viewers) > 1) {
        int index;
        /* Add a menu item for each view in the preferred list for this location. */
        /* Start on 1, because extra_viewer gets index 0 */
        for (node = window->details->short_list_viewers, index = 1;
                node != NULL;
                node = node->next, ++index)
        {
            /* Menu item in View menu. */
            add_view_as_menu_item (window,
                    CAJA_MENU_PATH_SHORT_LIST_PLACEHOLDER,
                    node->data,
                    index,
                    merge_id);
        }
    }
    gtk_ui_manager_insert_action_group (window->details->ui_manager,
                                        window->details->view_as_action_group,
                                        -1);
    g_object_unref (window->details->view_as_action_group); /* owned by ui_manager */

    caja_window_synch_view_as_menus (window);

    g_signal_emit (window, signals[VIEW_AS_CHANGED], 0);

}

static void
load_view_as_menus_callback (CajaFile *file,
                             gpointer callback_data)
{
    CajaWindow *window;
    CajaWindowSlot *slot;

    slot = callback_data;
    window = CAJA_WINDOW (slot->pane->window);

    if (slot == window->details->active_pane->active_slot)
    {
        load_view_as_menu (window);
    }
}

static void
cancel_view_as_callback (CajaWindowSlot *slot)
{
    caja_file_cancel_call_when_ready (slot->viewed_file,
                                      load_view_as_menus_callback,
                                      slot);
}

void
caja_window_load_view_as_menus (CajaWindow *window)
{
    CajaWindowSlot *slot;
    CajaFileAttributes attributes;

    g_return_if_fail (CAJA_IS_WINDOW (window));

    attributes = caja_mime_actions_get_required_file_attributes ();

    slot = window->details->active_pane->active_slot;

    cancel_view_as_callback (slot);
    caja_file_call_when_ready (slot->viewed_file,
                               attributes,
                               load_view_as_menus_callback,
                               slot);
}

void
caja_window_display_error (CajaWindow *window, const char *error_msg)
{
    GtkWidget *dialog;

    g_return_if_fail (CAJA_IS_WINDOW (window));

    dialog = gtk_message_dialog_new (GTK_WINDOW (window), 0, GTK_MESSAGE_ERROR,
                                     GTK_BUTTONS_OK, error_msg, NULL);
    gtk_widget_show (dialog);
}

static char *
real_get_title (CajaWindow *window)
{
    g_assert (CAJA_IS_WINDOW (window));

    return caja_window_slot_get_title (window->details->active_pane->active_slot);
}

static void
real_sync_title (CajaWindow *window,
                 CajaWindowSlot *slot)
{
    if (slot == window->details->active_pane->active_slot)
    {
        char *copy;

        copy = g_strdup (slot->title);
        g_signal_emit_by_name (window, "title_changed",
                               slot->title);
        g_free (copy);
    }
}

void
caja_window_sync_title (CajaWindow *window,
                        CajaWindowSlot *slot)
{
    EEL_CALL_METHOD (CAJA_WINDOW_CLASS, window,
                     sync_title, (window, slot));
}

void
caja_window_sync_zoom_widgets (CajaWindow *window)
{
    CajaWindowSlot *slot;
    CajaView *view;
    GtkAction *action;
    gboolean supports_zooming;
    gboolean can_zoom, can_zoom_in, can_zoom_out;
    CajaZoomLevel zoom_level;

    slot = window->details->active_pane->active_slot;
    view = slot->content_view;

    if (view != NULL)
    {
        supports_zooming = caja_view_supports_zooming (view);
        zoom_level = caja_view_get_zoom_level (view);
        can_zoom = supports_zooming &&
                   zoom_level >= CAJA_ZOOM_LEVEL_SMALLEST &&
                   zoom_level <= CAJA_ZOOM_LEVEL_LARGEST;
        can_zoom_in = can_zoom && caja_view_can_zoom_in (view);
        can_zoom_out = can_zoom && caja_view_can_zoom_out (view);
    }
    else
    {
        zoom_level = CAJA_ZOOM_LEVEL_STANDARD;
        supports_zooming = FALSE;
        can_zoom = FALSE;
        can_zoom_in = FALSE;
        can_zoom_out = FALSE;
    }

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    action = gtk_action_group_get_action (window->details->main_action_group,
                                          CAJA_ACTION_ZOOM_IN);
    gtk_action_set_visible (action, supports_zooming);
    gtk_action_set_sensitive (action, can_zoom_in);

    action = gtk_action_group_get_action (window->details->main_action_group,
                                          CAJA_ACTION_ZOOM_OUT);
    gtk_action_set_visible (action, supports_zooming);
    gtk_action_set_sensitive (action, can_zoom_out);

    action = gtk_action_group_get_action (window->details->main_action_group,
                                          CAJA_ACTION_ZOOM_NORMAL);
    gtk_action_set_visible (action, supports_zooming);
    gtk_action_set_sensitive (action, can_zoom);
    G_GNUC_END_IGNORE_DEPRECATIONS;

    g_signal_emit (window, signals[ZOOM_CHANGED], 0,
                   zoom_level, supports_zooming, can_zoom,
                   can_zoom_in, can_zoom_out);
}

static void
zoom_level_changed_callback (CajaView *view,
                             CajaWindow *window)
{
    g_assert (CAJA_IS_WINDOW (window));

    /* This is called each time the component in
     * the active slot successfully completed
     * a zooming operation.
     */
    caja_window_sync_zoom_widgets (window);
}


/* These are called
 *   A) when switching the view within the active slot
 *   B) when switching the active slot
 *   C) when closing the active slot (disconnect)
*/
void
caja_window_connect_content_view (CajaWindow *window,
                                  CajaView *view)
{
    CajaWindowSlot *slot;

    g_assert (CAJA_IS_WINDOW (window));
    g_assert (CAJA_IS_VIEW (view));

    slot = caja_window_get_slot_for_view (window, view);
    g_assert (slot == caja_window_get_active_slot (window));

    g_signal_connect (view, "zoom-level-changed",
                      G_CALLBACK (zoom_level_changed_callback),
                      window);

    /* Update displayed view in menu. Only do this if we're not switching
     * locations though, because if we are switching locations we'll
     * install a whole new set of views in the menu later (the current
     * views in the menu are for the old location).
     */
    if (slot->pending_location == NULL)
    {
        caja_window_load_view_as_menus (window);
    }

    caja_view_grab_focus (view);
}

void
caja_window_disconnect_content_view (CajaWindow *window,
                                     CajaView *view)
{
    CajaWindowSlot *slot;

    g_assert (CAJA_IS_WINDOW (window));
    g_assert (CAJA_IS_VIEW (view));

    slot = caja_window_get_slot_for_view (window, view);
    g_assert (slot == caja_window_get_active_slot (window));

    g_signal_handlers_disconnect_by_func (view, G_CALLBACK (zoom_level_changed_callback), window);
}

/**
 * caja_window_show:
 * @widget:	GtkWidget
 *
 * Call parent and then show/hide window items
 * base on user prefs.
 */
static void
caja_window_show (GtkWidget *widget)
{
    CajaWindow *window;

    window = CAJA_WINDOW (widget);

    GTK_WIDGET_CLASS (caja_window_parent_class)->show (widget);

    caja_window_ui_update (window);
}

GtkUIManager *
caja_window_get_ui_manager (CajaWindow *window)
{
    g_return_val_if_fail (CAJA_IS_WINDOW (window), NULL);

    return window->details->ui_manager;
}

CajaWindowPane *
caja_window_get_next_pane (CajaWindow *window)
{
    CajaWindowPane *next_pane;
    GList *node;

    /* return NULL if there is only one pane */
    if (!window->details->panes || !window->details->panes->next)
    {
        return NULL;
    }

    /* get next pane in the (wrapped around) list */
    node = g_list_find (window->details->panes, window->details->active_pane);
    g_return_val_if_fail (node, NULL);
    if (node->next)
    {
        next_pane = node->next->data;
    }
    else
    {
        next_pane =  window->details->panes->data;
    }

    return next_pane;
}


void
caja_window_slot_set_viewed_file (CajaWindowSlot *slot,
                                  CajaFile *file)
{
    CajaFileAttributes attributes;

    if (slot->viewed_file == file)
    {
        return;
    }

    caja_file_ref (file);

    cancel_view_as_callback (slot);

    if (slot->viewed_file != NULL)
    {
        CajaWindow *window;

        window = slot->pane->window;

        if (CAJA_IS_SPATIAL_WINDOW (window))
        {
            caja_file_set_has_open_window (slot->viewed_file,
                                           FALSE);
        }
        caja_file_monitor_remove (slot->viewed_file,
                                  slot);
    }

    if (file != NULL)
    {
        attributes =
            CAJA_FILE_ATTRIBUTE_INFO |
            CAJA_FILE_ATTRIBUTE_LINK_INFO;
        caja_file_monitor_add (file, slot, attributes);
    }

    caja_file_unref (slot->viewed_file);
    slot->viewed_file = file;
}

void
caja_send_history_list_changed (void)
{
    g_signal_emit_by_name (caja_signaller_get_current (),
                           "history_list_changed");
}

static void
free_history_list (void)
{
    g_list_free_full (history_list, g_object_unref);
    history_list = NULL;
}

/* Remove the this URI from the history list.
 * Do not sent out a change notice.
 * We pass in a bookmark for convenience.
 */
static void
remove_from_history_list (CajaBookmark *bookmark)
{
    GList *node;

    /* Compare only the uris here. Comparing the names also is not
     * necessary and can cause problems due to the asynchronous
     * nature of when the title of the window is set.
     */
    node = g_list_find_custom (history_list,
                               bookmark,
                               caja_bookmark_compare_uris);

    /* Remove any older entry for this same item. There can be at most 1. */
    if (node != NULL)
    {
        history_list = g_list_remove_link (history_list, node);
        g_object_unref (node->data);
        g_list_free_1 (node);
    }
}

gboolean
caja_add_bookmark_to_history_list (CajaBookmark *bookmark)
{
    /* Note that the history is shared amongst all windows so
     * this is not a CajaNavigationWindow function. Perhaps it belongs
     * in its own file.
     */
    GList *l, *next;
    static gboolean free_history_list_is_set_up;

    g_assert (CAJA_IS_BOOKMARK (bookmark));

    if (!free_history_list_is_set_up)
    {
        eel_debug_call_at_shutdown (free_history_list);
        free_history_list_is_set_up = TRUE;
    }

    /*	g_warning ("Add to history list '%s' '%s'",
    		   caja_bookmark_get_name (bookmark),
    		   caja_bookmark_get_uri (bookmark)); */

    if (!history_list ||
            caja_bookmark_compare_uris (history_list->data, bookmark))
    {
        int i;

        g_object_ref (bookmark);
        remove_from_history_list (bookmark);
        history_list = g_list_prepend (history_list, bookmark);

        for (i = 0, l = history_list; l; l = next)
        {
            next = l->next;

            if (i++ >= MAX_HISTORY_ITEMS)
            {
                g_object_unref (l->data);
                history_list = g_list_delete_link (history_list, l);
            }
        }

        return TRUE;
    }

    return FALSE;
}

void
caja_remove_from_history_list_no_notify (GFile *location)
{
    CajaBookmark *bookmark;

    bookmark = caja_bookmark_new (location, "", FALSE, NULL);
    remove_from_history_list (bookmark);
    g_object_unref (bookmark);
}

gboolean
caja_add_to_history_list_no_notify (GFile *location,
                                    const char *name,
                                    gboolean has_custom_name,
                                    GIcon *icon)
{
    CajaBookmark *bookmark;
    gboolean ret;

    bookmark = caja_bookmark_new (location, name, has_custom_name, icon);
    ret = caja_add_bookmark_to_history_list (bookmark);
    g_object_unref (bookmark);

    return ret;
}

CajaWindowSlot *
caja_window_get_slot_for_view (CajaWindow *window,
                               CajaView *view)
{
    CajaWindowSlot *slot;
    GList *l, *walk;

    for (walk = window->details->panes; walk; walk = walk->next)
    {
        CajaWindowPane *pane = walk->data;

        for (l = pane->slots; l != NULL; l = l->next)
        {
            slot = l->data;
            if (slot->content_view == view ||
                    slot->new_content_view == view)
            {
                return slot;
            }
        }
    }

    return NULL;
}

void
caja_forget_history (void)
{
    CajaWindowSlot *slot;
    CajaNavigationWindowSlot *navigation_slot;
    GList *window_node, *l, *walk;
    CajaApplication *app;

    app = CAJA_APPLICATION (g_application_get_default ());
    /* Clear out each window's back & forward lists. Also, remove
     * each window's current location bookmark from history list
     * so it doesn't get clobbered.
     */
    for (window_node = gtk_application_get_windows (GTK_APPLICATION (app));
            window_node != NULL;
            window_node = window_node->next)
    {

        if (CAJA_IS_NAVIGATION_WINDOW (window_node->data))
        {
            CajaNavigationWindow *window;

            window = CAJA_NAVIGATION_WINDOW (window_node->data);

            for (walk = CAJA_WINDOW (window_node->data)->details->panes; walk; walk = walk->next)
            {
                CajaWindowPane *pane = walk->data;
                for (l = pane->slots; l != NULL; l = l->next)
                {
                    navigation_slot = l->data;

                    caja_navigation_window_slot_clear_back_list (navigation_slot);
                    caja_navigation_window_slot_clear_forward_list (navigation_slot);
                }
            }

            caja_navigation_window_allow_back (window, FALSE);
            caja_navigation_window_allow_forward (window, FALSE);
        }

        for (walk = CAJA_WINDOW (window_node->data)->details->panes; walk; walk = walk->next)
        {
            CajaWindowPane *pane = walk->data;
            for (l = pane->slots; l != NULL; l = l->next)
            {
                slot = l->data;
                history_list = g_list_remove (history_list,
                                              slot->current_location_bookmark);
            }
        }
    }

    /* Clobber history list. */
    free_history_list ();

    /* Re-add each window's current location to history list. */
    for (window_node = gtk_application_get_windows (GTK_APPLICATION (app));
            window_node != NULL;
            window_node = window_node->next)
    {
        CajaWindow *window;
        CajaWindowSlot *slot;
        GList *l;

        window = CAJA_WINDOW (window_node->data);
        for (walk = window->details->panes; walk; walk = walk->next)
        {
            CajaWindowPane *pane = walk->data;
            for (l = pane->slots; l != NULL; l = l->next)
            {
                slot = CAJA_WINDOW_SLOT (l->data);
                caja_window_slot_add_current_location_to_history_list (slot);
            }
        }
    }
}

GList *
caja_get_history_list (void)
{
    return history_list;
}

static GList *
caja_window_get_history (CajaWindow *window)
{
    return g_list_copy_deep (history_list, (GCopyFunc) g_object_ref, NULL);
}


static CajaWindowType
caja_window_get_window_type (CajaWindow *window)
{
    g_assert (CAJA_IS_WINDOW (window));

    return CAJA_WINDOW_GET_CLASS (window)->window_type;
}

static int
caja_window_get_selection_count (CajaWindow *window)
{
    CajaWindowSlot *slot;

    g_assert (CAJA_IS_WINDOW (window));

    slot = window->details->active_pane->active_slot;

    if (slot->content_view != NULL)
    {
        return caja_view_get_selection_count (slot->content_view);
    }

    return 0;
}

static GList *
caja_window_get_selection (CajaWindow *window)
{
    CajaWindowSlot *slot;

    g_assert (CAJA_IS_WINDOW (window));

    slot = window->details->active_pane->active_slot;

    if (slot->content_view != NULL)
    {
        return caja_view_get_selection (slot->content_view);
    }
    return NULL;
}

static CajaWindowShowHiddenFilesMode
caja_window_get_hidden_files_mode (CajaWindowInfo *window)
{
    return window->details->show_hidden_files_mode;
}

static void
caja_window_set_hidden_files_mode (CajaWindowInfo *window,
                                   CajaWindowShowHiddenFilesMode  mode)
{
    window->details->show_hidden_files_mode = mode;

    g_signal_emit_by_name (window, "hidden_files_mode_changed");
}

static CajaWindowShowBackupFilesMode
caja_window_get_backup_files_mode (CajaWindowInfo *window)
{
    return window->details->show_backup_files_mode;
}

static void
caja_window_set_backup_files_mode (CajaWindowInfo *window,
                                   CajaWindowShowBackupFilesMode  mode)
{
    window->details->show_backup_files_mode = mode;

    g_signal_emit_by_name (window, "backup_files_mode_changed");
}

static gboolean
caja_window_get_initiated_unmount (CajaWindowInfo *window)
{
    return window->details->initiated_unmount;
}

static void
caja_window_set_initiated_unmount (CajaWindowInfo *window,
                                   gboolean initiated_unmount)
{
    window->details->initiated_unmount = initiated_unmount;
}

static char *
caja_window_get_cached_title (CajaWindow *window)
{
    CajaWindowSlot *slot;

    g_assert (CAJA_IS_WINDOW (window));

    slot = window->details->active_pane->active_slot;

    return g_strdup (slot->title);
}

CajaWindowSlot *
caja_window_get_active_slot (CajaWindow *window)
{
    g_assert (CAJA_IS_WINDOW (window));

    return window->details->active_pane->active_slot;
}

CajaWindowSlot *
caja_window_get_extra_slot (CajaWindow *window)
{
    CajaWindowPane *extra_pane;
    GList *node;

    g_assert (CAJA_IS_WINDOW (window));


    /* return NULL if there is only one pane */
    if (window->details->panes == NULL ||
            window->details->panes->next == NULL)
    {
        return NULL;
    }

    /* get next pane in the (wrapped around) list */
    node = g_list_find (window->details->panes,
                        window->details->active_pane);
    g_return_val_if_fail (node, FALSE);

    if (node->next)
    {
        extra_pane = node->next->data;
    }
    else
    {
        extra_pane =  window->details->panes->data;
    }

    return extra_pane->active_slot;
}

GList *
caja_window_get_slots (CajaWindow *window)
{
    GList *walk,*list;

    g_assert (CAJA_IS_WINDOW (window));

    list = NULL;
    for (walk = window->details->panes; walk; walk = walk->next)
    {
        CajaWindowPane *pane = walk->data;
        list  = g_list_concat (list, g_list_copy(pane->slots));
    }
    return list;
}

static void
caja_window_info_iface_init (CajaWindowInfoIface *iface)
{
    iface->report_load_underway = caja_window_report_load_underway;
    iface->report_load_complete = caja_window_report_load_complete;
    iface->report_selection_changed = caja_window_report_selection_changed;
    iface->report_view_failed = caja_window_report_view_failed;
    iface->view_visible = caja_window_view_visible;
    iface->close_window = caja_window_close;
    iface->push_status = caja_window_push_status;
    iface->get_window_type = caja_window_get_window_type;
    iface->get_title = caja_window_get_cached_title;
    iface->get_history = caja_window_get_history;
    iface->get_current_location = caja_window_get_location_uri;
    iface->get_ui_manager = caja_window_get_ui_manager;
    iface->get_selection_count = caja_window_get_selection_count;
    iface->get_selection = caja_window_get_selection;
    iface->get_hidden_files_mode = caja_window_get_hidden_files_mode;
    iface->set_hidden_files_mode = caja_window_set_hidden_files_mode;

    iface->get_backup_files_mode = caja_window_get_backup_files_mode;
    iface->set_backup_files_mode = caja_window_set_backup_files_mode;

    iface->get_active_slot = caja_window_get_active_slot;
    iface->get_extra_slot = caja_window_get_extra_slot;
    iface->get_initiated_unmount = caja_window_get_initiated_unmount;
    iface->set_initiated_unmount = caja_window_set_initiated_unmount;
}

static void
caja_window_class_init (CajaWindowClass *class)
{
    GtkBindingSet *binding_set;

    G_OBJECT_CLASS (class)->constructor = caja_window_constructor;
    G_OBJECT_CLASS (class)->constructed = caja_window_constructed;
    G_OBJECT_CLASS (class)->get_property = caja_window_get_property;
    G_OBJECT_CLASS (class)->set_property = caja_window_set_property;
    G_OBJECT_CLASS (class)->finalize = caja_window_finalize;

    GTK_WIDGET_CLASS (class)->destroy = caja_window_destroy;

    GTK_WIDGET_CLASS (class)->show = caja_window_show;

    GTK_WIDGET_CLASS (class)->realize = caja_window_realize;
    GTK_WIDGET_CLASS (class)->key_press_event = caja_window_key_press_event;
    class->get_title = real_get_title;
    class->sync_title = real_sync_title;
    class->set_allow_up = real_set_allow_up;
    class->close_slot = real_close_slot;

    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     ARG_APP,
                                     g_param_spec_object ("app",
                                             "Application",
                                             "The CajaApplication associated with this window.",
                                             CAJA_TYPE_APPLICATION,
                                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    signals[GO_UP] =
        g_signal_new ("go_up",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                      G_STRUCT_OFFSET (CajaWindowClass, go_up),
                      g_signal_accumulator_true_handled, NULL,
                      caja_src_marshal_BOOLEAN__BOOLEAN,
                      G_TYPE_BOOLEAN, 1, G_TYPE_BOOLEAN);
    signals[RELOAD] =
        g_signal_new ("reload",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                      G_STRUCT_OFFSET (CajaWindowClass, reload),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
    signals[PROMPT_FOR_LOCATION] =
        g_signal_new ("prompt-for-location",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                      G_STRUCT_OFFSET (CajaWindowClass, prompt_for_location),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE, 1, G_TYPE_STRING);
    signals[ZOOM_CHANGED] =
        g_signal_new ("zoom-changed",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL, NULL,
                      caja_src_marshal_VOID__INT_BOOLEAN_BOOLEAN_BOOLEAN_BOOLEAN,
                      G_TYPE_NONE, 5,
                      G_TYPE_INT, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN,
                      G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);
    signals[VIEW_AS_CHANGED] =
        g_signal_new ("view-as-changed",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    binding_set = gtk_binding_set_by_class (class);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_BackSpace, 0,
                                  "go_up", 1,
                                  G_TYPE_BOOLEAN, FALSE);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_F5, 0,
                                  "reload", 0);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_slash, 0,
                                  "prompt-for-location", 1,
                                  G_TYPE_STRING, "/");

    class->reload = caja_window_reload;
    class->go_up = caja_window_go_up_signal;
}
