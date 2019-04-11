/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Caja
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000, 2001 Eazel, Inc.
 *
 *  Caja is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  Caja is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  Authors: Elliot Lee <sopwith@redhat.com>
 *           Darin Adler <darin@bentspoon.com>
 *
 */

#ifndef CAJA_WINDOW_PRIVATE_H
#define CAJA_WINDOW_PRIVATE_H

#include <libcaja-private/caja-directory.h>

#include "caja-window.h"
#include "caja-window-slot.h"
#include "caja-window-pane.h"
#include "caja-spatial-window.h"
#include "caja-navigation-window.h"
#include "caja-bookmark-list.h"

struct _CajaNavigationWindowPane;

/* FIXME bugzilla.gnome.org 42575: Migrate more fields into here. */
struct _CajaWindowPrivate
{
    GtkWidget *grid;

    GtkWidget *statusbar;
    GtkWidget *menubar;

    GtkUIManager *ui_manager;
    GtkActionGroup *main_action_group; /* owned by ui_manager */
    guint help_message_cid;

    /* Menus. */
    guint extensions_menu_merge_id;
    GtkActionGroup *extensions_menu_action_group;

    GtkActionGroup *bookmarks_action_group;
    guint bookmarks_merge_id;
    CajaBookmarkList *bookmark_list;

    CajaWindowShowHiddenFilesMode show_hidden_files_mode;
    CajaWindowShowBackupFilesMode show_backup_files_mode;

    /* View As menu */
    GList *short_list_viewers;
    char *extra_viewer;

    /* View As choices */
    GtkActionGroup *view_as_action_group; /* owned by ui_manager */
    GtkRadioAction *view_as_radio_action;
    GtkRadioAction *extra_viewer_radio_action;
    guint short_list_merge_id;
    guint extra_viewer_merge_id;

    /* Ensures that we do not react on signals of a
     * view that is re-used as new view when its loading
     * is cancelled
     */
    gboolean temporarily_ignore_view_signals;

    /* available panes, and active pane.
     * Both of them may never be NULL.
     */
    GList *panes;
    CajaWindowPane *active_pane;

    /* So we can tell which window initiated
     * an unmount operation.
     */
    gboolean initiated_unmount;
};

struct _CajaNavigationWindowPrivate
{
    GtkWidget *content_paned;
    GtkWidget *content_box;
    GtkActionGroup *navigation_action_group; /* owned by ui_manager */

    GtkSizeGroup *header_size_group;

    /* Side Pane */
    int side_pane_width;
    CajaSidebar *current_side_panel;

    /* Menus */
    GtkActionGroup *go_menu_action_group;
    guint refresh_go_menu_idle_id;
    guint go_menu_merge_id;

    /* Toolbar */
    GtkWidget *toolbar;

    guint extensions_toolbar_merge_id;
    GtkActionGroup *extensions_toolbar_action_group;

    /* spinner */
    gboolean    spinner_active;
    GtkWidget  *spinner;

    /* focus widget before the location bar has been shown temporarily */
    GtkWidget *last_focus_widget;

    /* split view */
    GtkWidget *split_view_hpane;
};

#define CAJA_MENU_PATH_BACK_ITEM			"/menu/Go/Back"
#define CAJA_MENU_PATH_FORWARD_ITEM			"/menu/Go/Forward"
#define CAJA_MENU_PATH_UP_ITEM			"/menu/Go/Up"

#define CAJA_MENU_PATH_RELOAD_ITEM			"/menu/View/Reload"
#define CAJA_MENU_PATH_ZOOM_IN_ITEM			"/menu/View/Zoom Items Placeholder/Zoom In"
#define CAJA_MENU_PATH_ZOOM_OUT_ITEM		"/menu/View/Zoom Items Placeholder/Zoom Out"
#define CAJA_MENU_PATH_ZOOM_NORMAL_ITEM		"/menu/View/Zoom Items Placeholder/Zoom Normal"

#define CAJA_COMMAND_BACK				"/commands/Back"
#define CAJA_COMMAND_FORWARD			"/commands/Forward"
#define CAJA_COMMAND_UP				"/commands/Up"

#define CAJA_COMMAND_RELOAD				"/commands/Reload"
#define CAJA_COMMAND_BURN_CD			"/commands/Burn CD"
#define CAJA_COMMAND_STOP				"/commands/Stop"
#define CAJA_COMMAND_ZOOM_IN			"/commands/Zoom In"
#define CAJA_COMMAND_ZOOM_OUT			"/commands/Zoom Out"
#define CAJA_COMMAND_ZOOM_NORMAL			"/commands/Zoom Normal"

/* window geometry */
/* Min values are very small, and a Caja window at this tiny size is *almost*
 * completely unusable. However, if all the extra bits (sidebar, location bar, etc)
 * are turned off, you can see an icon or two at this size. See bug 5946.
 */

#define CAJA_SPATIAL_WINDOW_MIN_WIDTH			100
#define CAJA_SPATIAL_WINDOW_MIN_HEIGHT			100
#define CAJA_SPATIAL_WINDOW_DEFAULT_WIDTH			500
#define CAJA_SPATIAL_WINDOW_DEFAULT_HEIGHT			300

#define CAJA_NAVIGATION_WINDOW_MIN_WIDTH			200
#define CAJA_NAVIGATION_WINDOW_MIN_HEIGHT			200
#define CAJA_NAVIGATION_WINDOW_DEFAULT_WIDTH		800
#define CAJA_NAVIGATION_WINDOW_DEFAULT_HEIGHT		550

typedef void (*CajaBookmarkFailedCallback) (CajaWindow *window,
        CajaBookmark *bookmark);

void               caja_window_set_status                            (CajaWindow    *window,
        CajaWindowSlot *slot,
        const char        *status);
void               caja_window_load_view_as_menus                    (CajaWindow    *window);
void               caja_window_load_extension_menus                  (CajaWindow    *window);
void               caja_window_initialize_menus                      (CajaWindow    *window);
void               caja_window_finalize_menus                        (CajaWindow    *window);
CajaWindowPane *caja_window_get_next_pane                        (CajaWindow *window);
void               caja_menus_append_bookmark_to_menu                (CajaWindow    *window,
        CajaBookmark  *bookmark,
        const char        *parent_path,
        const char        *parent_id,
        guint              index_in_parent,
        GtkActionGroup    *action_group,
        guint              merge_id,
        GCallback          refresh_callback,
        CajaBookmarkFailedCallback failed_callback);
void               caja_window_update_find_menu_item                 (CajaWindow    *window);
void               caja_window_zoom_in                               (CajaWindow    *window);
void               caja_window_zoom_out                              (CajaWindow    *window);
void               caja_window_zoom_to_level                         (CajaWindow    *window,
        CajaZoomLevel  level);
void               caja_window_zoom_to_default                       (CajaWindow    *window);

CajaWindowSlot *caja_window_open_slot                            (CajaWindowPane *pane,
        CajaWindowOpenSlotFlags flags);
void                caja_window_close_slot                           (CajaWindowSlot *slot);

CajaWindowSlot *caja_window_get_slot_for_view                    (CajaWindow *window,
        CajaView   *view);

GList *              caja_window_get_slots                           (CajaWindow    *window);
CajaWindowSlot * caja_window_get_active_slot                     (CajaWindow    *window);
CajaWindowSlot * caja_window_get_extra_slot                      (CajaWindow    *window);
void                 caja_window_set_active_slot                     (CajaWindow    *window,
        CajaWindowSlot *slot);
void                 caja_window_set_active_pane                     (CajaWindow *window,
        CajaWindowPane *new_pane);
CajaWindowPane * caja_window_get_active_pane                     (CajaWindow *window);

void               caja_send_history_list_changed                    (void);
void               caja_remove_from_history_list_no_notify           (GFile             *location);
gboolean           caja_add_bookmark_to_history_list                 (CajaBookmark  *bookmark);
gboolean           caja_add_to_history_list_no_notify                (GFile             *location,
        const char        *name,
        gboolean           has_custom_name,
        GIcon            *icon);
GList *            caja_get_history_list                             (void);
void               caja_window_bookmarks_preference_changed_callback (gpointer           user_data);


/* sync window GUI with current slot. Used when changing slots,
 * and when updating the slot state.
 */
void caja_window_sync_status           (CajaWindow *window);
void caja_window_sync_allow_stop       (CajaWindow *window,
                                        CajaWindowSlot *slot);
void caja_window_sync_title            (CajaWindow *window,
                                        CajaWindowSlot *slot);
void caja_window_sync_zoom_widgets     (CajaWindow *window);

/* Navigation window menus */
void               caja_navigation_window_initialize_actions                    (CajaNavigationWindow    *window);
void               caja_navigation_window_initialize_menus                      (CajaNavigationWindow    *window);
void               caja_navigation_window_remove_bookmarks_menu_callback        (CajaNavigationWindow    *window);

void               caja_navigation_window_remove_bookmarks_menu_items           (CajaNavigationWindow    *window);
void               caja_navigation_window_update_show_hide_menu_items           (CajaNavigationWindow     *window);
void               caja_navigation_window_update_spatial_menu_item              (CajaNavigationWindow     *window);
void               caja_navigation_window_remove_go_menu_callback    (CajaNavigationWindow    *window);
void               caja_navigation_window_remove_go_menu_items       (CajaNavigationWindow    *window);

/* Navigation window toolbar */
void               caja_navigation_window_activate_spinner                     (CajaNavigationWindow    *window);
void               caja_navigation_window_initialize_toolbars                   (CajaNavigationWindow    *window);
void               caja_navigation_window_load_extension_toolbar_items          (CajaNavigationWindow    *window);
void               caja_navigation_window_set_spinner_active                   (CajaNavigationWindow    *window,
        gboolean                     active);
void               caja_navigation_window_go_back                               (CajaNavigationWindow    *window);
void               caja_navigation_window_go_forward                            (CajaNavigationWindow    *window);
void               caja_window_close_pane                                       (CajaWindowPane *pane);
void               caja_navigation_window_update_split_view_actions_sensitivity (CajaNavigationWindow    *window);

#endif /* CAJA_WINDOW_PRIVATE_H */
