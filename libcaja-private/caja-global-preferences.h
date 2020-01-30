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
   write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
   Boston, MA 02110-1301, USA.

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#ifndef CAJA_GLOBAL_PREFERENCES_H
#define CAJA_GLOBAL_PREFERENCES_H

#include <gio/gio.h>

G_BEGIN_DECLS

/* Whether exit when last window destroyed */
#define CAJA_PREFERENCES_EXIT_WITH_LAST_WINDOW		"exit-with-last-window"

/* Desktop Background options */
#define CAJA_PREFERENCES_BACKGROUND_SET			"background-set"
#define CAJA_PREFERENCES_BACKGROUND_COLOR		"background-color"
#define CAJA_PREFERENCES_BACKGROUND_URI			"background-uri"

/* Side Pane Background options */
#define CAJA_PREFERENCES_SIDE_PANE_BACKGROUND_SET	"side-pane-background-set"
#define CAJA_PREFERENCES_SIDE_PANE_BACKGROUND_COLOR	"side-pane-background-color"
#define CAJA_PREFERENCES_SIDE_PANE_BACKGROUND_URI	"side-pane-background-uri"

/* Automount options */
#define CAJA_PREFERENCES_MEDIA_AUTOMOUNT		"automount"
#define CAJA_PREFERENCES_MEDIA_AUTOMOUNT_OPEN		"automount-open"

/* Autorun options */
#define CAJA_PREFERENCES_MEDIA_AUTORUN_NEVER			"autorun-never"
#define CAJA_PREFERENCES_MEDIA_AUTORUN_X_CONTENT_START_APP	"autorun-x-content-start-app"
#define CAJA_PREFERENCES_MEDIA_AUTORUN_X_CONTENT_IGNORE		"autorun-x-content-ignore"
#define CAJA_PREFERENCES_MEDIA_AUTORUN_X_CONTENT_OPEN_FOLDER	"autorun-x-content-open-folder"

/* Trash options */
#define CAJA_PREFERENCES_CONFIRM_TRASH			"confirm-trash"
#define CAJA_PREFERENCES_CONFIRM_MOVE_TO_TRASH	"confirm-move-to-trash"
#define CAJA_PREFERENCES_ENABLE_DELETE			"enable-delete"

/* Desktop options */
#define CAJA_PREFERENCES_DESKTOP_IS_HOME_DIR		"desktop-is-home-dir"
#define CAJA_PREFERENCES_SHOW_NOTIFICATIONS             "show-notifications"

/* Display  */
#define CAJA_PREFERENCES_SHOW_HIDDEN_FILES  		"show-hidden-files"
#define CAJA_PREFERENCES_SHOW_BACKUP_FILES  		"show-backup-files"
#define CAJA_PREFERENCES_SHOW_ADVANCED_PERMISSIONS	"show-advanced-permissions"
#define CAJA_PREFERENCES_DATE_FORMAT			"date-format"
#define CAJA_PREFERENCES_USE_IEC_UNITS			"use-iec-units"
#define CAJA_PREFERENCES_SHOW_ICONS_IN_LIST_VIEW	"show-icons-in-list-view"

/* Mouse */
#define CAJA_PREFERENCES_MOUSE_USE_EXTRA_BUTTONS 	"mouse-use-extra-buttons"
#define CAJA_PREFERENCES_MOUSE_FORWARD_BUTTON		"mouse-forward-button"
#define CAJA_PREFERENCES_MOUSE_BACK_BUTTON		"mouse-back-button"

typedef enum
{
    CAJA_DATE_FORMAT_LOCALE,
    CAJA_DATE_FORMAT_ISO,
    CAJA_DATE_FORMAT_INFORMAL
} CajaDateFormat;

typedef enum
{
    CAJA_NEW_TAB_POSITION_AFTER_CURRENT_TAB,
    CAJA_NEW_TAB_POSITION_END,
} CajaNewTabPosition;

/* Sidebar panels  */
#define CAJA_PREFERENCES_TREE_SHOW_ONLY_DIRECTORIES         "show-only-directories"

/* Single/Double click preference  */
#define CAJA_PREFERENCES_CLICK_POLICY			"click-policy"

/* Activating executable text files */
#define CAJA_PREFERENCES_EXECUTABLE_TEXT_ACTIVATION		"executable-text-activation"

/* Installing new packages when unknown mime type activated */
#define CAJA_PREFERENCES_INSTALL_MIME_ACTIVATION		"install-mime-activation"

/* Spatial or browser mode */
#define CAJA_PREFERENCES_ALWAYS_USE_BROWSER       		"always-use-browser"
#define CAJA_PREFERENCES_NEW_TAB_POSITION       		"tabs-open-position"
#define CAJA_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY			"always-use-location-entry"

/* Which views should be displayed for new windows */
#define CAJA_WINDOW_STATE_START_WITH_LOCATION_BAR			"start-with-location-bar"
#define CAJA_WINDOW_STATE_START_WITH_STATUS_BAR				"start-with-status-bar"
#define CAJA_WINDOW_STATE_START_WITH_SIDEBAR		 		"start-with-sidebar"
#define CAJA_WINDOW_STATE_START_WITH_TOOLBAR				"start-with-toolbar"
#define CAJA_WINDOW_STATE_SIDE_PANE_VIEW                    "side-pane-view"
#define CAJA_WINDOW_STATE_GEOMETRY 	"geometry"
#define CAJA_WINDOW_STATE_MAXIMIZED        "maximized"
#define CAJA_WINDOW_STATE_SIDEBAR_WIDTH  					"sidebar-width"

/* Sorting order */
#define CAJA_PREFERENCES_SORT_DIRECTORIES_FIRST		        "sort-directories-first"
#define CAJA_PREFERENCES_DEFAULT_SORT_ORDER			        "default-sort-order"
#define CAJA_PREFERENCES_DEFAULT_SORT_IN_REVERSE_ORDER	    "default-sort-in-reverse-order"

/* The default folder viewer - one of the two enums below */
#define CAJA_PREFERENCES_DEFAULT_FOLDER_VIEWER		"default-folder-viewer"

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
#define CAJA_PREFERENCES_ICON_VIEW_DEFAULT_USE_TIGHTER_LAYOUT	    "default-use-tighter-layout"
#define CAJA_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL		        "default-zoom-level"

#define CAJA_PREFERENCES_ICON_VIEW_LABELS_BESIDE_ICONS      	"labels-beside-icons"



    /* Which text attributes appear beneath icon names */
#define CAJA_PREFERENCES_ICON_VIEW_CAPTIONS				"captions"

    /* The default size for thumbnail icons */
#define CAJA_PREFERENCES_ICON_VIEW_THUMBNAIL_SIZE			"thumbnail-size"

    /* ellipsization preferences */
#define CAJA_PREFERENCES_ICON_VIEW_TEXT_ELLIPSIS_LIMIT		"text-ellipsis-limit"
#define CAJA_PREFERENCES_DESKTOP_TEXT_ELLIPSIS_LIMIT		"text-ellipsis-limit"

    /* Compact View */
#define CAJA_PREFERENCES_COMPACT_VIEW_DEFAULT_ZOOM_LEVEL		"default-zoom-level"
#define CAJA_PREFERENCES_COMPACT_VIEW_ALL_COLUMNS_SAME_WIDTH	"all-columns-have-same-width"

    /* List View */
#define CAJA_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL		"default-zoom-level"
#define CAJA_PREFERENCES_LIST_VIEW_DEFAULT_VISIBLE_COLUMNS	"default-visible-columns"
#define CAJA_PREFERENCES_LIST_VIEW_DEFAULT_COLUMN_ORDER		"default-column-order"

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

#define CAJA_PREFERENCES_SHOW_TEXT_IN_ICONS		    "show-icon-text"
#define CAJA_PREFERENCES_SHOW_DIRECTORY_ITEM_COUNTS "show-directory-item-counts"
#define CAJA_PREFERENCES_SHOW_IMAGE_FILE_THUMBNAILS	"show-image-thumbnails"
#define CAJA_PREFERENCES_IMAGE_FILE_THUMBNAIL_LIMIT	"thumbnail-limit"
#define CAJA_PREFERENCES_PREVIEW_SOUND		        "preview-sound"

    typedef enum
    {
        CAJA_COMPLEX_SEARCH_BAR,
        CAJA_SIMPLE_SEARCH_BAR
    } CajaSearchBarMode;

#define CAJA_PREFERENCES_DESKTOP_FONT                  "font"
#define CAJA_PREFERENCES_DESKTOP_HOME_VISIBLE          "home-icon-visible"
#define CAJA_PREFERENCES_DESKTOP_HOME_NAME             "home-icon-name"
#define CAJA_PREFERENCES_DESKTOP_COMPUTER_VISIBLE      "computer-icon-visible"
#define CAJA_PREFERENCES_DESKTOP_COMPUTER_NAME         "computer-icon-name"
#define CAJA_PREFERENCES_DESKTOP_TRASH_VISIBLE         "trash-icon-visible"
#define CAJA_PREFERENCES_DESKTOP_TRASH_NAME            "trash-icon-name"
#define CAJA_PREFERENCES_DESKTOP_VOLUMES_VISIBLE       "volumes-visible"
#define CAJA_PREFERENCES_DESKTOP_NETWORK_VISIBLE       "network-icon-visible"
#define CAJA_PREFERENCES_DESKTOP_NETWORK_NAME          "network-icon-name"
#define CAJA_PREFERENCES_LOCKDOWN_COMMAND_LINE         "disable-command-line"
#define CAJA_PREFERENCES_DISABLED_EXTENSIONS           "disabled-extensions"

void caja_global_preferences_init                      (void);
char *caja_global_preferences_get_default_folder_viewer_preference_as_iid (void);

extern GSettings *caja_preferences;
extern GSettings *caja_media_preferences;
extern GSettings *caja_window_state;
extern GSettings *caja_icon_view_preferences;
extern GSettings *caja_desktop_preferences;
extern GSettings *caja_tree_sidebar_preferences;
extern GSettings *caja_compact_view_preferences;
extern GSettings *caja_list_view_preferences;
extern GSettings *caja_extension_preferences;

extern GSettings *mate_background_preferences;
extern GSettings *mate_lockdown_preferences;

G_END_DECLS

#endif /* CAJA_GLOBAL_PREFERENCES_H */
