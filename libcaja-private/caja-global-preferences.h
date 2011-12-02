/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* caja-global-preferences.h - Caja specific preference keys and
                                   functions.

   Copyright (C) 1999, 2000, 2001 Eazel, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this program; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#ifndef CAJA_GLOBAL_PREFERENCES_H
#define CAJA_GLOBAL_PREFERENCES_H

#include <eel/eel-preferences.h>

#ifdef __cplusplus
extern "C" {
#endif

    /* Whether exit when last window destroyed */
#define CAJA_PREFERENCES_EXIT_WITH_LAST_WINDOW				"preferences/exit_with_last_window"

    /* Which theme is active */
#define CAJA_PREFERENCES_THEME				"/desktop/mate/file_views/icon_theme"

    /* Desktop Background options */
#define CAJA_PREFERENCES_BACKGROUND_SET                     "preferences/background_set"
#define CAJA_PREFERENCES_BACKGROUND_COLOR                   "preferences/background_color"
#define CAJA_PREFERENCES_BACKGROUND_FILENAME                "preferences/background_filename"

    /* Side Pane Background options */
#define CAJA_PREFERENCES_SIDE_PANE_BACKGROUND_SET                     "preferences/side_pane_background_set"
#define CAJA_PREFERENCES_SIDE_PANE_BACKGROUND_COLOR                   "preferences/side_pane_background_color"
#define CAJA_PREFERENCES_SIDE_PANE_BACKGROUND_FILENAME                "preferences/side_pane_background_filename"

    /* How wide the sidebar is (or how wide it will be when expanded) */
#define CAJA_PREFERENCES_SIDEBAR_WIDTH  			"preferences/sidebar_width"

    /* Automount options */
#define CAJA_PREFERENCES_MEDIA_AUTOMOUNT	                "preferences/media_automount"
#define CAJA_PREFERENCES_MEDIA_AUTOMOUNT_OPEN		"preferences/media_automount_open"

    /* Autorun options */
#define CAJA_PREFERENCES_MEDIA_AUTORUN_NEVER                "preferences/media_autorun_never"
#define CAJA_PREFERENCES_MEDIA_AUTORUN_X_CONTENT_START_APP  "preferences/media_autorun_x_content_start_app"
#define CAJA_PREFERENCES_MEDIA_AUTORUN_X_CONTENT_IGNORE     "preferences/media_autorun_x_content_ignore"
#define CAJA_PREFERENCES_MEDIA_AUTORUN_X_CONTENT_OPEN_FOLDER "preferences/media_autorun_x_content_open_folder"

    /* Trash options */
#define CAJA_PREFERENCES_CONFIRM_TRASH			"preferences/confirm_trash"
#define CAJA_PREFERENCES_ENABLE_DELETE			"preferences/enable_delete"

    /* Desktop options */
#define CAJA_PREFERENCES_SHOW_DESKTOP			"preferences/show_desktop"
#define CAJA_PREFERENCES_DESKTOP_IS_HOME_DIR                "preferences/desktop_is_home_dir"
#define CAJA_PREFERENCES_DESKTOP_FONT			"preferences/desktop_font"

    /* Display  */
#define CAJA_PREFERENCES_SHOW_HIDDEN_FILES  		"/desktop/mate/file_views/show_hidden_files"
#define CAJA_PREFERENCES_SHOW_BACKUP_FILES  		"/desktop/mate/file_views/show_backup_files"
#define CAJA_PREFERENCES_SHOW_ADVANCED_PERMISSIONS		"preferences/show_advanced_permissions"
#define CAJA_PREFERENCES_DATE_FORMAT			"preferences/date_format"

    /* Mouse */
#define CAJA_PREFERENCES_MOUSE_USE_EXTRA_BUTTONS 		"preferences/mouse_use_extra_buttons"
#define CAJA_PREFERENCES_MOUSE_FORWARD_BUTTON		"preferences/mouse_forward_button"
#define CAJA_PREFERENCES_MOUSE_BACK_BUTTON			"preferences/mouse_back_button"

    typedef enum
    {
        CAJA_DATE_FORMAT_LOCALE,
        CAJA_DATE_FORMAT_ISO,
        CAJA_DATE_FORMAT_INFORMAL
    }
    CajaDateFormat;

    typedef enum
    {
        CAJA_NEW_TAB_POSITION_AFTER_CURRENT_TAB,
        CAJA_NEW_TAB_POSITION_END,
    } CajaNewTabPosition;

    /* Sidebar panels  */
#define CAJA_PREFERENCES_TREE_SHOW_ONLY_DIRECTORIES         "sidebar_panels/tree/show_only_directories"

    /* Single/Double click preference  */
#define CAJA_PREFERENCES_CLICK_POLICY			"preferences/click_policy"

    /* Activating executable text files */
#define CAJA_PREFERENCES_EXECUTABLE_TEXT_ACTIVATION		"preferences/executable_text_activation"

    /* Installing new packages when unknown mime type activated */
#define CAJA_PREFERENCES_INSTALL_MIME_ACTIVATION		"preferences/install_mime_activation"

    /* Spatial or browser mode */
#define CAJA_PREFERENCES_ALWAYS_USE_BROWSER       		"preferences/always_use_browser"
#define CAJA_PREFERENCES_NEW_TAB_POSITION       		"preferences/tabs_open_position"

    /* Which views should be displayed for new windows */
#define CAJA_PREFERENCES_START_WITH_LOCATION_BAR		"preferences/start_with_location_bar"
#define CAJA_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY		"preferences/always_use_location_entry"
#define CAJA_PREFERENCES_START_WITH_STATUS_BAR		"preferences/start_with_status_bar"
#define CAJA_PREFERENCES_START_WITH_SIDEBAR		 	"preferences/start_with_sidebar"
#define CAJA_PREFERENCES_START_WITH_TOOLBAR			"preferences/start_with_toolbar"
#define CAJA_PREFERENCES_SIDE_PANE_VIEW                     "preferences/side_pane_view"
#define CAJA_PREFERENCES_NAVIGATION_WINDOW_SAVED_GEOMETRY 	"preferences/navigation_window_saved_geometry"
#define CAJA_PREFERENCES_NAVIGATION_WINDOW_MAXIMIZED        "preferences/navigation_window_saved_maximized"

    /* Sorting order */
#define CAJA_PREFERENCES_SORT_DIRECTORIES_FIRST		"preferences/sort_directories_first"

    /* The default folder viewer - one of the two enums below */
#define CAJA_PREFERENCES_DEFAULT_FOLDER_VIEWER		"preferences/default_folder_viewer"

    enum
    {
        CAJA_DEFAULT_FOLDER_VIEWER_ICON_VIEW,
        CAJA_DEFAULT_FOLDER_VIEWER_COMPACT_VIEW,
        CAJA_DEFAULT_FOLDER_VIEWER_LIST_VIEW,
        CAJA_DEFAULT_FOLDER_VIEWER_OTHER
    };

    /* These IIDs are used by the preferences code and in caja-application.c */
#define CAJA_ICON_VIEW_IID		"OAFIID:Caja_File_Manager_Icon_View"
#define CAJA_COMPACT_VIEW_IID	"OAFIID:Caja_File_Manager_Compact_View"
#define CAJA_LIST_VIEW_IID		"OAFIID:Caja_File_Manager_List_View"


    /* Icon View */
#define CAJA_PREFERENCES_ICON_VIEW_DEFAULT_SORT_IN_REVERSE_ORDER	"icon_view/default_sort_in_reverse_order"
#define CAJA_PREFERENCES_ICON_VIEW_DEFAULT_SORT_ORDER		"icon_view/default_sort_order"
#define CAJA_PREFERENCES_ICON_VIEW_DEFAULT_USE_TIGHTER_LAYOUT	"icon_view/default_use_tighter_layout"
#define CAJA_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL		"icon_view/default_zoom_level"
#define CAJA_PREFERENCES_ICON_VIEW_DEFAULT_USE_MANUAL_LAYOUT	"icon_view/default_use_manual_layout"

#define CAJA_PREFERENCES_ICON_VIEW_LABELS_BESIDE_ICONS      	"icon_view/labels_beside_icons"


    /* The icon view uses 2 variables to store the sort order and
     * whether to use manual layout.  However, the UI for these
     * preferences presensts them as single option menu.  So we
     * use the following preference as a proxy for the other two.
     * In caja-global-preferences.c we install callbacks for
     * the proxy preference and update the other 2 when it changes
     */
#define CAJA_PREFERENCES_ICON_VIEW_DEFAULT_SORT_ORDER_OR_MANUAL_LAYOUT "icon_view/default_sort_order_or_manual_layout"

    /* Which text attributes appear beneath icon names */
#define CAJA_PREFERENCES_ICON_VIEW_CAPTIONS				"icon_view/captions"

    /* The default size for thumbnail icons */
#define CAJA_PREFERENCES_ICON_VIEW_THUMBNAIL_SIZE			"icon_view/thumbnail_size"

    /* ellipsization preferences */
#define CAJA_PREFERENCES_ICON_VIEW_TEXT_ELLIPSIS_LIMIT		"icon_view/text_ellipsis_limit"
#define CAJA_PREFERENCES_DESKTOP_TEXT_ELLIPSIS_LIMIT		"desktop/text_ellipsis_limit"

    /* Compact View */
#define CAJA_PREFERENCES_COMPACT_VIEW_DEFAULT_ZOOM_LEVEL		"compact_view/default_zoom_level"
#define CAJA_PREFERENCES_COMPACT_VIEW_ALL_COLUMNS_SAME_WIDTH	"compact_view/all_columns_have_same_width"

    /* List View */
#define CAJA_PREFERENCES_LIST_VIEW_DEFAULT_SORT_IN_REVERSE_ORDER	"list_view/default_sort_in_reverse_order"
#define CAJA_PREFERENCES_LIST_VIEW_DEFAULT_SORT_ORDER		"list_view/default_sort_order"
#define CAJA_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL		"list_view/default_zoom_level"
#define CAJA_PREFERENCES_LIST_VIEW_DEFAULT_VISIBLE_COLUMNS	        "list_view/default_visible_columns"
#define CAJA_PREFERENCES_LIST_VIEW_DEFAULT_COLUMN_ORDER	        "list_view/default_column_order"

    /* News panel */
#define CAJA_PREFERENCES_NEWS_MAX_ITEMS				"news/max_items"
#define CAJA_PREFERENCES_NEWS_UPDATE_INTERVAL			"news/update_interval"

    /* File Indexing */
#define CAJA_PREFERENCES_SEARCH_BAR_TYPE				"preferences/search_bar_type"

    enum
    {
        CAJA_CLICK_POLICY_SINGLE,
        CAJA_CLICK_POLICY_DOUBLE
    };

    enum
    {
        CAJA_EXECUTABLE_TEXT_LAUNCH,
        CAJA_EXECUTABLE_TEXT_DISPLAY,
        CAJA_EXECUTABLE_TEXT_ASK
    };

    typedef enum
    {
        CAJA_SPEED_TRADEOFF_ALWAYS,
        CAJA_SPEED_TRADEOFF_LOCAL_ONLY,
        CAJA_SPEED_TRADEOFF_NEVER
    } CajaSpeedTradeoffValue;

#define CAJA_PREFERENCES_SHOW_TEXT_IN_ICONS		"preferences/show_icon_text"
#define CAJA_PREFERENCES_SHOW_DIRECTORY_ITEM_COUNTS "preferences/show_directory_item_counts"
#define CAJA_PREFERENCES_SHOW_IMAGE_FILE_THUMBNAILS	"preferences/show_image_thumbnails"
#define CAJA_PREFERENCES_IMAGE_FILE_THUMBNAIL_LIMIT	"preferences/thumbnail_limit"
#define CAJA_PREFERENCES_PREVIEW_SOUND		"preferences/preview_sound"

    typedef enum
    {
        CAJA_COMPLEX_SEARCH_BAR,
        CAJA_SIMPLE_SEARCH_BAR
    } CajaSearchBarMode;

#define CAJA_PREFERENCES_DESKTOP_HOME_VISIBLE          "desktop/home_icon_visible"
#define CAJA_PREFERENCES_DESKTOP_HOME_NAME             "desktop/home_icon_name"
#define CAJA_PREFERENCES_DESKTOP_COMPUTER_VISIBLE      "desktop/computer_icon_visible"
#define CAJA_PREFERENCES_DESKTOP_COMPUTER_NAME         "desktop/computer_icon_name"
#define CAJA_PREFERENCES_DESKTOP_TRASH_VISIBLE         "desktop/trash_icon_visible"
#define CAJA_PREFERENCES_DESKTOP_TRASH_NAME            "desktop/trash_icon_name"
#define CAJA_PREFERENCES_DESKTOP_VOLUMES_VISIBLE	   "desktop/volumes_visible"
#define CAJA_PREFERENCES_DESKTOP_NETWORK_VISIBLE       "desktop/network_icon_visible"
#define CAJA_PREFERENCES_DESKTOP_NETWORK_NAME          "desktop/network_icon_name"

    /* Lockdown */
#define CAJA_PREFERENCES_LOCKDOWN_COMMAND_LINE         "/desktop/mate/lockdown/disable_command_line"

    void caja_global_preferences_init                      (void);
    char *caja_global_preferences_get_default_folder_viewer_preference_as_iid (void);
#ifdef __cplusplus
}
#endif

#endif /* CAJA_GLOBAL_PREFERENCES_H */
