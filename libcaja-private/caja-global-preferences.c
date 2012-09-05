/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* caja-global-preferences.c - Caja specific preference keys and
                                   functions.

   Copyright (C) 1999, 2000, 2001 Eazel, Inc.

   The Mate Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Mate Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Mate Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
   Boston, MA 02110-1301, USA.

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#include <config.h>
#include "caja-global-preferences.h"

#include "caja-file-utilities.h"
#include "caja-file.h"
#include <eel/eel-enumeration.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <glib/gi18n.h>

/* Constants */
#define STRING_ARRAY_DEFAULT_TOKENS_DELIMETER ","
#define PREFERENCES_SORT_ORDER_MANUALLY 100

/* Path for mate-vfs preferences */
static const char *EXTRA_MONITOR_PATHS[] = {
        "/desktop/mate/background",
        "/desktop/mate/lockdown",
        NULL
                                           };

/* An enumeration used for installing type specific preferences defaults. */
typedef enum
{
    PREFERENCE_BOOLEAN = 1,
    PREFERENCE_INTEGER,
    PREFERENCE_STRING,
    PREFERENCE_STRING_ARRAY
} PreferenceType;

/*
 * A callback which can be used to fetch dynamic fallback values.
 * For example, values that are dependent on the environment (such as user name)
 * cannot be specified as constants.
 */
typedef gpointer (*PreferencesDefaultValueCallback) (void);

/* A structure that describes a single preference including defaults and visibility. */
typedef struct
{
    const char *name;
    PreferenceType type;
    const gpointer fallback_value;
    PreferencesDefaultValueCallback fallback_callback;
    GFreeFunc fallback_callback_result_free_function;
    const char *enumeration_id;
} PreferenceDefault;

/*
 * Public functions
 */
char *
caja_global_preferences_get_default_folder_viewer_preference_as_iid (void)
{
    int preference_value;
    const char *viewer_iid;

    preference_value =
        g_settings_get_enum (caja_preferences, CAJA_PREFERENCES_DEFAULT_FOLDER_VIEWER);

    if (preference_value == CAJA_DEFAULT_FOLDER_VIEWER_LIST_VIEW)
    {
        viewer_iid = CAJA_LIST_VIEW_IID;
    }
    else if (preference_value == CAJA_DEFAULT_FOLDER_VIEWER_COMPACT_VIEW)
    {
        viewer_iid = CAJA_COMPACT_VIEW_IID;
    }
    else
    {
        viewer_iid = CAJA_ICON_VIEW_IID;
    }

    return g_strdup (viewer_iid);
}

void
caja_global_preferences_init (void)
{
    static gboolean initialized = FALSE;
    int i;

    if (initialized)
    {
        return;
    }

    initialized = TRUE;

    eel_preferences_init ("/apps/caja");
    
    caja_preferences = g_settings_new("org.mate.caja.preferences");
    caja_media_preferences = g_settings_new("org.mate.media-handling");
    caja_window_state = g_settings_new("org.mate.caja.window-state");
    caja_icon_view_preferences = g_settings_new("org.mate.caja.icon-view");
    caja_compact_view_preferences = g_settings_new("org.mate.caja.compact-view");
    caja_desktop_preferences = g_settings_new("org.mate.caja.desktop");
    caja_tree_sidebar_preferences = g_settings_new("org.mate.caja.sidebar-panels.tree");
    caja_list_view_preferences = g_settings_new("org.mate.caja.list-view");

    caja_mateconf_client = mateconf_client_get_default ();

    /* Add monitors for any other MateConf paths we have keys in */
    for (i=0; EXTRA_MONITOR_PATHS[i] != NULL; i++) {
        mateconf_client_add_dir (caja_mateconf_client,
                      EXTRA_MONITOR_PATHS[i],
                      MATECONF_CLIENT_PRELOAD_ONELEVEL,
                      NULL);
    }

    /* Preload everything in a big batch */
    eel_mateconf_preload_cache ("/apps/caja/preferences",
                                MATECONF_CLIENT_PRELOAD_ONELEVEL);
    eel_mateconf_preload_cache ("/desktop/mate/file_views",
                                MATECONF_CLIENT_PRELOAD_ONELEVEL);
    eel_mateconf_preload_cache ("/desktop/mate/background",
                                MATECONF_CLIENT_PRELOAD_ONELEVEL);
    eel_mateconf_preload_cache ("/desktop/mate/lockdown",
                                MATECONF_CLIENT_PRELOAD_ONELEVEL);

    /* These are always needed for the desktop */
    eel_mateconf_preload_cache ("/apps/caja/desktop",
                                MATECONF_CLIENT_PRELOAD_ONELEVEL);
    eel_mateconf_preload_cache ("/apps/caja/icon_view",
                                MATECONF_CLIENT_PRELOAD_ONELEVEL);
    eel_mateconf_preload_cache ("/apps/caja/desktop-metadata",
                                MATECONF_CLIENT_PRELOAD_RECURSIVE);
}
