/*
 *  caja-extension.c - extension management functions
 *
 *  Copyright (C) 2014 MATE Desktop.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin St, Fifth Floor, 
 *  Boston, MA 02110-1301, USA.
 *
 *  Author: Alexander van der Meij <alexandervdm@gliese.me>
 */

#include "caja-extensions.h"

#include "caja-global-preferences.h"
#include "caja-module.h"

#include <string.h>


static GList *caja_extensions = NULL;


Extension *
extension_new (gchar *filename, gboolean state, GObject *module)
{
    Extension *ext;
    
    ext = g_new0 (Extension, 1);
    ext->filename = filename;
    ext->state = state;
    ext->module = module;
    return ext;
}

/* functions related to persistent configuration through gsettings: */

static gboolean
gsettings_key_has_value (const gchar *value)
{
    gchar **list;
    gint i;
    
    list = g_settings_get_strv (caja_extension_preferences, 
                                CAJA_PREFERENCES_DISABLED_EXTENSIONS);

    if (list != NULL)
    {
        for (i = 0; list[i]; i++)
        {
            if (g_ascii_strcasecmp (value, list[i]) == 0)
            {
                g_strfreev (list);
                return TRUE;
            }
        }
    }
    g_strfreev (list);
    return FALSE;
}

static gboolean
gsettings_append_to_list (const char *value)
{
    gchar **current;
    gchar **new;
    gint size;
    gboolean retval;
    
    current = g_settings_get_strv (caja_extension_preferences, 
                                   CAJA_PREFERENCES_DISABLED_EXTENSIONS);
    
    for (size = 0; current[size] != NULL; size++);

    size += 1;
    size += 1;
    
    new = g_realloc_n (current, size, sizeof (gchar *));

    new[size - 2] = g_strdup (value);
    new[size - 1] = NULL;
    
    retval = g_settings_set_strv (caja_extension_preferences, 
                                  CAJA_PREFERENCES_DISABLED_EXTENSIONS,
                                 (const gchar **) new);

    g_strfreev (new);
    return retval;
}

static gboolean
gsettings_remove_from_list (const char *value)
{
    gchar **current;
    GArray *array;
    gint i;
    gboolean retval;
    
    current = g_settings_get_strv (caja_extension_preferences, 
                                   CAJA_PREFERENCES_DISABLED_EXTENSIONS);
                                   
    array = g_array_new (TRUE, TRUE, sizeof (gchar *));
    
    for (i = 0; current[i] != NULL; i++) 
    {
        if (g_strcmp0 (current[i], value) != 0)
            array = g_array_append_val (array, current[i]);
    }
    
    retval = g_settings_set_strv (caja_extension_preferences, 
                                  CAJA_PREFERENCES_DISABLED_EXTENSIONS,
                                 (const gchar **) array->data);

    g_strfreev (current);
    g_array_free (array, TRUE);
    return retval;
}

/* functions related to the extension management */

static gboolean
caja_extension_is_disabled (const gchar *extname)
{
    if (gsettings_key_has_value (extname))
    {
        return TRUE;
    }
    return FALSE;
}

GList *
caja_extensions_get_for_type (GType type)
{
    GList *l;
    GList *ret = NULL;

    for (l = caja_extensions; l != NULL; l = l->next)
    {
        Extension *ext = l->data;
        if (ext->state) // only load enabled extensions
        {
            if (G_TYPE_CHECK_INSTANCE_TYPE (G_OBJECT (ext->module), type))
            {
                g_object_ref (ext->module);
                ret = g_list_prepend (ret, ext->module);
            }
        }
    }
    g_list_free (l);
    return ret;
}

GList *
caja_extensions_get_list (void)
{
    return caja_extensions;
}

void
caja_extension_register (gchar *filename, GObject *module)
{
    gboolean state = TRUE; // new extensions are enabled by default.
    gchar *extname;
    
    extname = g_strndup (filename, strlen(filename) - 3);
    
    if (caja_extension_is_disabled (extname))
        state = FALSE;

    Extension *ext = extension_new (extname, state, module);
    caja_extensions = g_list_append (caja_extensions, ext);
}

gboolean
caja_extension_set_state (Extension *ext, gboolean new_state)
{
    if (ext)
    {
        g_return_val_if_fail (ext->state != new_state, FALSE);
        ext->state = new_state;
    }
   
    gboolean retval;
    if (new_state) {
        retval = gsettings_remove_from_list (ext->filename);
    }
    else {
        retval = gsettings_append_to_list (ext->filename);
    }
    
    g_return_val_if_fail (retval == TRUE, FALSE);
    return TRUE;
}

