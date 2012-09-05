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
static const char *EXTRA_MONITOR_PATHS[] = { "/desktop/mate/file_views",
        "/desktop/mate/background",
        "/desktop/mate/lockdown",
        NULL
                                           };

/* Forward declarations */
static void     global_preferences_install_defaults      (void);
static void     global_preferences_register_enumerations (void);


/* An enumeration used for installing type specific preferences defaults. */
typedef enum
{
    PREFERENCE_BOOLEAN = 1,
    PREFERENCE_INTEGER,
    PREFERENCE_STRING,
    PREFERENCE_STRING_ARRAY
} PreferenceType;

static EelEnumerationEntry default_zoom_level_enum_entries[] =
{
    /* xgettext:no-c-format */
    { "smallest",	    N_("25%"),		CAJA_ZOOM_LEVEL_SMALLEST },
    /* xgettext:no-c-format */
    { "smaller",	    N_("50%"),		CAJA_ZOOM_LEVEL_SMALLER },
    /* xgettext:no-c-format */
    { "small",	    N_("75%"),		CAJA_ZOOM_LEVEL_SMALL },
    /* xgettext:no-c-format */
    { "standard",	    N_("100%"),		CAJA_ZOOM_LEVEL_STANDARD },
    /* xgettext:no-c-format */
    { "large",	    N_("150%"),		CAJA_ZOOM_LEVEL_LARGE },
    /* xgettext:no-c-format */
    { "larger",	    N_("200%"),		CAJA_ZOOM_LEVEL_LARGER },
    /* xgettext:no-c-format */
    { "largest",	    N_("400%"),		CAJA_ZOOM_LEVEL_LARGEST }
};

static EelEnumerationEntry file_size_enum_entries[] =
{
    { "102400",	    N_("100 K"),	102400 },
    { "512000",	    N_("500 K"),	512000 },
    { "1048576",	    N_("1 MB"),		1048576 },
    { "3145728",	    N_("3 MB"),		3145728 },
    { "5242880",	    N_("5 MB"),		5242880 },
    { "10485760",	    N_("10 MB"),	10485760 },
    { "104857600",	    N_("100 MB"),	104857600 },
    { "1073741824",     N_("1 GB"),         1073741824 },
    { "2147483648",     N_("2 GB"),         2147483648U },
    { "4294967295",     N_("4 GB"),         4294967295U }
};

static EelEnumerationEntry standard_font_size_entries[] =
{
    { "8",		   N_("8"),	8 },
    { "10",		   N_("10"),	10 },
    { "12",		   N_("12"),	12 },
    { "14",		   N_("14"),	14 },
    { "16",		   N_("16"),	16 },
    { "18",		   N_("18"),	18 },
    { "20",		   N_("20"),	20 },
    { "22",		   N_("22"),	22 },
    { "24",		   N_("24"),	24 }
};

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

/* The following table defines the default values and user level visibilities of
 * Caja preferences.  Each of these preferences does not necessarily need to
 * have a UI item in the preferences dialog.  To add an item to the preferences
 * dialog, see the CajaPreferencesItemDescription tables later in this file.
 *
 * Field definitions:
 *
 * 1. name
 *
 *    The name of the preference.  Usually defined in
 *    caja-global-preferences.h
 *
 * 2. type
 *    The preference type.  One of:
 *
 *	PREFERENCE_BOOLEAN
 *	PREFERENCE_INTEGER
 *	PREFERENCE_STRING
 *	PREFERENCE_STRING_ARRAY
 *
 * 3. fallback_value
 *    Emergency fallback value if our mateconf schemas are hosed somehow.
 *
 * 4. fallback_callback
 *    callback to get dynamic fallback
 *
 * 5. fallback_callback_result_free_function
 *    free result of fallback_callback
 *
 * 6. enumeration_id
 *    An an enumeration id is a unique string that identifies an enumeration.
 *    If given, an enumeration id can be used to qualify a INTEGER preference.
 *    The preferences dialog widgetry will use this enumeration id to find out
 *    what choices and descriptions of choices to present to the user.
 */

/* NOTE THAT THE FALLBACKS HERE ARE NOT SUPPOSED TO BE USED -
 * YOU SHOULD EDIT THE SCHEMAS FILE TO CHANGE DEFAULTS.
 */
static const PreferenceDefault preference_defaults[] =
{
    /* List View Default Preferences */
    {
        CAJA_PREFERENCES_LIST_VIEW_DEFAULT_SORT_ORDER,
        PREFERENCE_STRING,
        "name",
        NULL, NULL,
        NULL,
    },
    {
        CAJA_PREFERENCES_LIST_VIEW_DEFAULT_SORT_IN_REVERSE_ORDER,
        PREFERENCE_BOOLEAN,
        GINT_TO_POINTER (FALSE)
    },
    {
        CAJA_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL,
        PREFERENCE_STRING,
        "smaller",
        NULL, NULL,
        "default_zoom_level"
    },

    {
        CAJA_PREFERENCES_LOCKDOWN_COMMAND_LINE,
        PREFERENCE_BOOLEAN,
        GINT_TO_POINTER (FALSE)
    },
    { NULL }
};


/**
 * global_preferences_register_enumerations
 *
 * Register enumerations for INTEGER preferences that need them.
 *
 * This function needs to be called before the preferences dialog
 * panes are populated, as they use the registered information to
 * create enumeration item widgets.
 */
static void
global_preferences_register_enumerations (void)
{
    guint i;

    /* Register the enumerations.
     * These enumerations are used in the preferences dialog to
     * populate widgets and route preferences changes between the
     * storage (MateConf) and the displayed values.
     */
    eel_enumeration_register ("default_zoom_level",
                              default_zoom_level_enum_entries,
                              G_N_ELEMENTS (default_zoom_level_enum_entries));
    eel_enumeration_register ("file_size",
                              file_size_enum_entries,
                              G_N_ELEMENTS (file_size_enum_entries));
    eel_enumeration_register ("standard_font_size",
                              standard_font_size_entries,
                              G_N_ELEMENTS (standard_font_size_entries));

    /* Set the enumeration ids for preferences that need them */
    for (i = 0; preference_defaults[i].name != NULL; i++)
    {
        if (eel_strlen (preference_defaults[i].enumeration_id) > 0)
        {
            g_assert (preference_defaults[i].type == PREFERENCE_STRING
                      || preference_defaults[i].type == PREFERENCE_STRING_ARRAY
                      || preference_defaults[i].type == PREFERENCE_INTEGER);
            eel_preferences_set_enumeration_id (preference_defaults[i].name,
                                                preference_defaults[i].enumeration_id);
        }
    }
}

static void
global_preferences_install_one_default (const char *preference_name,
                                        PreferenceType preference_type,
                                        const PreferenceDefault *preference_default)
{
    gpointer value = NULL;
    char **string_array_value;

    g_return_if_fail (preference_name != NULL);
    g_return_if_fail (preference_type >= PREFERENCE_BOOLEAN);
    g_return_if_fail (preference_type <= PREFERENCE_STRING_ARRAY);
    g_return_if_fail (preference_default != NULL);

    /* If a callback is given, use that to fetch the default value */
    if (preference_default->fallback_callback != NULL)
    {
        value = (* preference_default->fallback_callback) ();
    }
    else
    {
        value = preference_default->fallback_value;
    }

    switch (preference_type)
    {
    case PREFERENCE_BOOLEAN:
        eel_preferences_set_emergency_fallback_boolean (preference_name,
                GPOINTER_TO_INT (value));
        break;

    case PREFERENCE_INTEGER:
        eel_preferences_set_emergency_fallback_integer (preference_name,

                GPOINTER_TO_INT (value));
        break;

    case PREFERENCE_STRING:
        eel_preferences_set_emergency_fallback_string (preference_name,
                value);
        break;

    case PREFERENCE_STRING_ARRAY:
        string_array_value = g_strsplit (value,
                                         STRING_ARRAY_DEFAULT_TOKENS_DELIMETER,
                                         -1);
        eel_preferences_set_emergency_fallback_string_array (preference_name,
                string_array_value);
        g_strfreev (string_array_value);
        break;

    default:
        g_assert_not_reached ();
    }

    /* Free the dynamic default value if needed */
    if (preference_default->fallback_callback != NULL
            && preference_default->fallback_callback_result_free_function != NULL)
    {
        (* preference_default->fallback_callback_result_free_function) (value);
    }
}

/**
 * global_preferences_install_defaults
 *
 * Install defaults and visibilities.
 *
 * Most of the defaults come from the preference_defaults table above.
 *
 * Many preferences require their defaults to be computed, and so there
 * are special functions to install those.
 */
static void
global_preferences_install_defaults (void)
{
    guint i;

    for (i = 0; preference_defaults[i].name != NULL; i++)
    {
        global_preferences_install_one_default (preference_defaults[i].name,
                                                preference_defaults[i].type,
                                                &preference_defaults[i]);
    }
}

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

/* The icon view uses 2 variables to store the sort order and
 * whether to use manual layout.  However, the UI for these
 * preferences presensts them as single option menu.  So we
 * use the following preference as a proxy for the other two.
 * In caja-global-preferences.c we install callbacks for
 * the proxy preference and update the other 2 when it changes
 */
static void
default_icon_view_sort_order_or_manual_layout_changed_callback (gpointer callback_data)
{
    int default_sort_order_or_manual_layout;
    int default_sort_order;

    default_sort_order_or_manual_layout =
        g_settings_get_enum (caja_icon_view_preferences,
                             CAJA_PREFERENCES_ICON_VIEW_DEFAULT_SORT_ORDER_OR_MANUAL_LAYOUT);

    eel_preferences_set_boolean (CAJA_PREFERENCES_ICON_VIEW_DEFAULT_USE_MANUAL_LAYOUT,
                                 default_sort_order_or_manual_layout == PREFERENCES_SORT_ORDER_MANUALLY);

    if (default_sort_order_or_manual_layout != PREFERENCES_SORT_ORDER_MANUALLY)
    {
        default_sort_order = default_sort_order_or_manual_layout;

        g_return_if_fail (default_sort_order >= CAJA_FILE_SORT_BY_DISPLAY_NAME);
        g_return_if_fail (default_sort_order <= CAJA_FILE_SORT_BY_EMBLEMS);

        g_settings_set_enum (caja_icon_view_preferences,
                             CAJA_PREFERENCES_ICON_VIEW_DEFAULT_SORT_ORDER,
                             default_sort_order);
    }
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

    /* Install defaults */
    global_preferences_install_defaults ();

    global_preferences_register_enumerations ();

    /* Add monitors for any other MateConf paths we have keys in */
    for (i=0; EXTRA_MONITOR_PATHS[i] != NULL; i++)
    {
        eel_preferences_monitor_directory (EXTRA_MONITOR_PATHS[i]);
    }
    
    caja_preferences = g_settings_new("org.mate.caja.preferences");
    caja_media_preferences = g_settings_new("org.mate.media-handling");
    caja_window_state = g_settings_new("org.mate.caja.window-state");
    caja_icon_view_preferences = g_settings_new("org.mate.caja.icon-view");
    caja_compact_view_preferences = g_settings_new("org.mate.caja.compact-view");
    caja_desktop_preferences = g_settings_new("org.mate.caja.desktop");
    caja_tree_sidebar_preferences = g_settings_new("org.mate.caja.sidebar-panels.tree");

    /* Set up storage for values accessed in this file */
    g_signal_connect_swapped (caja_icon_view_preferences,
                              "changed::" CAJA_PREFERENCES_ICON_VIEW_DEFAULT_SORT_ORDER_OR_MANUAL_LAYOUT,
                              G_CALLBACK (default_icon_view_sort_order_or_manual_layout_changed_callback), 
                              NULL);

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
