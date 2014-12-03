/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-link.c: .desktop link files.

   Copyright (C) 2001 Red Hat, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the historicalied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
   Boston, MA 02110-1301, USA.

   Authors: Jonathan Blandford <jrb@redhat.com>
            Alexander Larsson <alexl@redhat.com>
*/

#include <config.h>
#include "caja-link.h"

#include "caja-directory-notify.h"
#include "caja-directory.h"
#include "caja-file-utilities.h"
#include "caja-file.h"
#include "caja-program-choosing.h"
#include "caja-icon-names.h"
#include <eel/eel-vfs-extensions.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>

#define MAIN_GROUP "Desktop Entry"

#define CAJA_LINK_GENERIC_TAG	"Link"
#define CAJA_LINK_TRASH_TAG 	"X-caja-trash"
#define CAJA_LINK_MOUNT_TAG 	"FSDevice"
#define CAJA_LINK_HOME_TAG 		"X-caja-home"

static gboolean
is_link_mime_type (const char *mime_type)
{
    if (mime_type != NULL &&
            (g_ascii_strcasecmp (mime_type, "application/x-mate-app-info") == 0 ||
             g_ascii_strcasecmp (mime_type, "application/x-desktop") == 0))
    {
        return TRUE;
    }

    return FALSE;
}

static gboolean
is_local_file_a_link (const char *uri)
{
    gboolean link;
    GFile *file;
    GFileInfo *info;
    GError *error;

    error = NULL;
    link = FALSE;

    file = g_file_new_for_uri (uri);

    info = g_file_query_info (file,
                              G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                              0, NULL, &error);
    if (info)
    {
        link = is_link_mime_type (g_file_info_get_content_type (info));
        g_object_unref (info);
    }
    else
    {
        g_warning ("Error getting info: %s\n", error->message);
        g_error_free (error);
    }

    g_object_unref (file);

    return link;
}

static gboolean
_g_key_file_load_from_gfile (GKeyFile *key_file,
                             GFile *file,
                             GKeyFileFlags flags,
                             GError **error)
{
    char *data;
    gsize len;
    gboolean res;

    if (!g_file_load_contents (file, NULL, &data, &len, NULL, error))
    {
        return FALSE;
    }

    res = g_key_file_load_from_data (key_file, data, len, flags, error);

    g_free (data);

    return res;
}

static gboolean
_g_key_file_save_to_gfile (GKeyFile *key_file,
                           GFile *file,
                           GError  **error)
{
    char *data;
    gsize len;

    data = g_key_file_to_data (key_file, &len, error);
    if (data == NULL)
    {
        return FALSE;
    }

    if (!g_file_replace_contents (file,
                                  data, len,
                                  NULL, FALSE,
                                  G_FILE_CREATE_NONE,
                                  NULL, NULL, error))
    {
        g_free (data);
        return FALSE;
    }
    g_free (data);
    return TRUE;
}



static GKeyFile *
_g_key_file_new_from_uri (const char *uri,
                          GKeyFileFlags flags,
                          GError **error)
{
    GKeyFile *key_file;
    GFile *file;

    file = g_file_new_for_uri (uri);
    key_file = g_key_file_new ();
    if (!_g_key_file_load_from_gfile (key_file, file, flags, error))
    {
        g_key_file_free (key_file);
        key_file = NULL;
    }
    g_object_unref (file);
    return key_file;
}

static char *
slurp_key_string (const char *uri,
                  const char *keyname,
                  gboolean    localize)
{
    GKeyFile *key_file;
    char *result;

    key_file = _g_key_file_new_from_uri (uri, G_KEY_FILE_NONE, NULL);
    if (key_file == NULL)
    {
        return NULL;
    }

    if (localize)
    {
        result = g_key_file_get_locale_string (key_file, MAIN_GROUP, keyname, NULL, NULL);
    }
    else
    {
        result = g_key_file_get_string (key_file, MAIN_GROUP, keyname, NULL);
    }
    g_key_file_free (key_file);

    return result;
}

gboolean
caja_link_local_create (const char     *directory_uri,
                        const char     *base_name,
                        const char     *display_name,
                        const char     *image,
                        const char     *target_uri,
                        const GdkPoint *point,
                        int             screen,
                        gboolean        unique_filename)
{
    char *real_directory_uri;
    char *uri, *contents;
    GFile *file;
    GList dummy_list;
    CajaFileChangesQueuePosition item;

    g_return_val_if_fail (directory_uri != NULL, FALSE);
    g_return_val_if_fail (base_name != NULL, FALSE);
    g_return_val_if_fail (display_name != NULL, FALSE);
    g_return_val_if_fail (target_uri != NULL, FALSE);

    if (eel_uri_is_trash (directory_uri) ||
            eel_uri_is_search (directory_uri))
    {
        return FALSE;
    }

    if (eel_uri_is_desktop (directory_uri))
    {
        real_directory_uri = caja_get_desktop_directory_uri ();
    }
    else
    {
        real_directory_uri = g_strdup (directory_uri);
    }

    if (unique_filename)
    {
        uri = caja_ensure_unique_file_name (real_directory_uri,
                                            base_name, ".desktop");
        if (uri == NULL)
        {
            g_free (real_directory_uri);
            return FALSE;
        }
        file = g_file_new_for_uri (uri);
        g_free (uri);
    }
    else
    {
        char *link_name;
        GFile *dir;

        link_name = g_strdup_printf ("%s.desktop", base_name);

        /* replace '/' with '-', just in case */
        g_strdelimit (link_name, "/", '-');

        dir = g_file_new_for_uri (directory_uri);
        file = g_file_get_child (dir, link_name);

        g_free (link_name);
        g_object_unref (dir);
    }

    g_free (real_directory_uri);

    contents = g_strdup_printf ("[Desktop Entry]\n"
                                "Encoding=UTF-8\n"
                                "Name=%s\n"
                                "Type=Link\n"
                                "URL=%s\n"
                                "%s%s\n",
                                display_name,
                                target_uri,
                                image != NULL ? "Icon=" : "",
                                image != NULL ? image : "");


    if (!g_file_replace_contents (file,
                                  contents, strlen (contents),
                                  NULL, FALSE,
                                  G_FILE_CREATE_NONE,
                                  NULL, NULL, NULL))
    {
        g_free (contents);
        g_object_unref (file);
        return FALSE;
    }
    g_free (contents);

    dummy_list.data = file;
    dummy_list.next = NULL;
    dummy_list.prev = NULL;
    caja_directory_notify_files_added (&dummy_list);

    if (point != NULL)
    {
        item.location = file;
        item.set = TRUE;
        item.point.x = point->x;
        item.point.y = point->y;
        item.screen = screen;
        dummy_list.data = &item;
        dummy_list.next = NULL;
        dummy_list.prev = NULL;

        caja_directory_schedule_position_set (&dummy_list);
    }

    g_object_unref (file);
    return TRUE;
}

static const char *
get_language (void)
{
    const char * const *langs_pointer;
    int i;

    langs_pointer = g_get_language_names ();
    for (i = 0; langs_pointer[i] != NULL; i++)
    {
        /* find first without encoding */
        if (strchr (langs_pointer[i], '.') == NULL)
        {
            return langs_pointer[i];
        }
    }
    return NULL;
}

static gboolean
caja_link_local_set_key (const char *uri,
                         const char *key,
                         const char *value,
                         gboolean    localize)
{
    gboolean success;
    GKeyFile *key_file;
    GFile *file;

    file = g_file_new_for_uri (uri);
    key_file = g_key_file_new ();
    if (!_g_key_file_load_from_gfile (key_file, file, G_KEY_FILE_KEEP_COMMENTS, NULL))
    {
        g_key_file_free (key_file);
        g_object_unref (file);
        return FALSE;
    }
    if (localize)
    {
        g_key_file_set_locale_string (key_file,
                                      MAIN_GROUP,
                                      key,
                                      get_language (),
                                      value);
    }
    else
    {
        g_key_file_set_string (key_file, MAIN_GROUP, key, value);
    }


    success = _g_key_file_save_to_gfile (key_file,  file, NULL);
    g_key_file_free (key_file);
    g_object_unref (file);
    return success;
}

gboolean
caja_link_local_set_text (const char *uri,
                          const char *text)
{
    return caja_link_local_set_key (uri, "Name", text, TRUE);
}


gboolean
caja_link_local_set_icon (const char        *uri,
                          const char        *icon)
{
    return caja_link_local_set_key (uri, "Icon", icon, FALSE);
}

char *
caja_link_local_get_text (const char *path)
{
    return slurp_key_string (path, "Name", TRUE);
}

char *
caja_link_local_get_additional_text (const char *path)
{
    /* The comment field of current .desktop files is often bad.
     * It just contains a copy of the name. This is probably because the
     * panel shows the comment field as a tooltip.
     */
    return NULL;
#ifdef THIS_IS_NOT_USED_RIGHT_NOW
    char *type;
    char *retval;

    if (!is_local_file_a_link (uri))
    {
        return NULL;
    }

    type = slurp_key_string (path, "Type", FALSE);
    retval = NULL;
    if (type == NULL)
    {
        return NULL;
    }

    if (strcmp (type, "Application") == 0)
    {
        retval = slurp_key_string (path, "Comment", TRUE);
    }

    g_free (type);

    return retval;
#endif
}

static char *
caja_link_get_link_uri_from_desktop (GKeyFile *key_file, const char *desktop_file_uri)
{
    char *type = g_key_file_get_string (key_file, MAIN_GROUP, "Type", NULL);
    if (type == NULL)
    {
        return NULL;
    }

    char *retval = NULL;

    if (strcmp (type, "URL") == 0)
    {
        /* Some old broken desktop files use this nonstandard feature, we need handle it though */
        retval = g_key_file_get_string (key_file, MAIN_GROUP, "Exec", NULL);
    }
    else if ((strcmp (type, CAJA_LINK_GENERIC_TAG) == 0) ||
             (strcmp (type, CAJA_LINK_MOUNT_TAG) == 0) ||
             (strcmp (type, CAJA_LINK_TRASH_TAG) == 0) ||
             (strcmp (type, CAJA_LINK_HOME_TAG) == 0))
    {
        retval = g_key_file_get_string (key_file, MAIN_GROUP, "URL", NULL);
    }
    g_free (type);

    if (retval != NULL && desktop_file_uri != NULL)
    {
        /* Handle local file names.
         * Ideally, we'd be able to use
         * g_file_parse_name(), but it does not know how to resolve
         * relative file names, since the base directory is unknown.
         */
        char *scheme = g_uri_parse_scheme (retval);
        if (scheme == NULL)
        {
            GFile *file = g_file_new_for_uri (desktop_file_uri);
            GFile *parent = g_file_get_parent (file);
            g_object_unref (file);

            if (parent != NULL)
            {
                file = g_file_resolve_relative_path (parent, retval);
                g_free (retval);
                retval = g_file_get_uri (file);
                g_object_unref (file);
                g_object_unref (parent);
            }
        }
        else
        {
            g_free (scheme);
        }
    }

    return retval;
}

static char *
caja_link_get_link_name_from_desktop (GKeyFile *key_file)
{
    return g_key_file_get_locale_string (key_file, MAIN_GROUP, "Name", NULL, NULL);
}

static char *
caja_link_get_link_icon_from_desktop (GKeyFile *key_file)
{
    char *icon_uri, *icon, *p, *type;

    icon_uri = g_key_file_get_string (key_file, MAIN_GROUP, "X-Caja-Icon", NULL);
    if (icon_uri != NULL)
    {
        return icon_uri;
    }

    icon = g_key_file_get_string (key_file, MAIN_GROUP, "Icon", NULL);
    if (icon != NULL)
    {
        if (!g_path_is_absolute (icon))
        {
            /* Strip out any extension on non-filename icons. Old desktop files may have this */
            p = strchr (icon, '.');
            /* Only strip known icon extensions */
            if ((p != NULL) &&
                    ((g_ascii_strcasecmp (p, ".png") == 0)
                     || (g_ascii_strcasecmp (p, ".svn") == 0)
                     || (g_ascii_strcasecmp (p, ".jpg") == 0)
                     || (g_ascii_strcasecmp (p, ".xpm") == 0)
                     || (g_ascii_strcasecmp (p, ".bmp") == 0)
                     || (g_ascii_strcasecmp (p, ".jpeg") == 0)))
            {
                *p = 0;
            }
        }
        return icon;
    }

    type = g_key_file_get_string (key_file, MAIN_GROUP, "Type", NULL);
    if (g_strcmp0 (type, "Application") == 0)
    {
        icon = g_strdup ("mate-fs-executable");
    }
    else if (g_strcmp0 (type, "Link") == 0)
    {
        icon = g_strdup ("mate-dev-symlink");
    }
    else if (g_strcmp0 (type, "FSDevice") == 0)
    {
        icon = g_strdup ("mate-dev-harddisk");
    }
    else if (g_strcmp0 (type, "Directory") == 0)
    {
        icon = g_strdup (CAJA_ICON_FOLDER);
    }
    else if (g_strcmp0 (type, "Service") == 0 ||
             g_strcmp0 (type, "ServiceType") == 0)
    {
        icon = g_strdup ("mate-fs-web");
    }
    else
    {
        icon = g_strdup ("mate-fs-regular");
    }
    g_free (type);

    return icon;
}

char *
caja_link_local_get_link_uri (const char *uri)
{
    GKeyFile *key_file;
    char *retval;

    if (!is_local_file_a_link (uri))
    {
        return NULL;
    }

    key_file = _g_key_file_new_from_uri (uri, G_KEY_FILE_NONE, NULL);
    if (key_file == NULL)
    {
        return NULL;
    }

    retval = caja_link_get_link_uri_from_desktop (key_file, uri);
    g_key_file_free (key_file);

    return retval;
}

static gboolean
string_array_contains (char **array,
                       const char *str)
{
    char **p;

    if (!array)
        return FALSE;

    for (p = array; *p; p++)
        if (g_ascii_strcasecmp (*p, str) == 0)
        {
            return TRUE;
        }

    return FALSE;
}

void
caja_link_get_link_info_given_file_contents (const char  *file_contents,
        int          link_file_size,
        const char  *file_uri,
        char       **uri,
        char       **name,
        char       **icon,
        gboolean    *is_launcher,
        gboolean    *is_foreign)
{
    GKeyFile *key_file;
    char *type;
    char **only_show_in;
    char **not_show_in;

    key_file = g_key_file_new ();
    if (!g_key_file_load_from_data (key_file,
                                    file_contents,
                                    link_file_size,
                                    G_KEY_FILE_NONE,
                                    NULL))
    {
        g_key_file_free (key_file);
        return;
    }

    *uri = caja_link_get_link_uri_from_desktop (key_file, file_uri);
    *name = caja_link_get_link_name_from_desktop (key_file);
    *icon = caja_link_get_link_icon_from_desktop (key_file);

    *is_launcher = FALSE;
    type = g_key_file_get_string (key_file, MAIN_GROUP, "Type", NULL);
    if (g_strcmp0 (type, "Application") == 0 &&
            g_key_file_has_key (key_file, MAIN_GROUP, "Exec", NULL))
    {
        *is_launcher = TRUE;
    }
    g_free (type);

    *is_foreign = FALSE;
    only_show_in = g_key_file_get_string_list (key_file, MAIN_GROUP,
                   "OnlyShowIn", NULL, NULL);
    if (only_show_in && !string_array_contains (only_show_in, "MATE"))
    {
        *is_foreign = TRUE;
    }
    g_strfreev (only_show_in);

    not_show_in = g_key_file_get_string_list (key_file, MAIN_GROUP,
                  "NotShowIn", NULL, NULL);
    if (not_show_in && string_array_contains (not_show_in, "MATE"))
    {
        *is_foreign = TRUE;
    }
    g_strfreev (not_show_in);

    g_key_file_free (key_file);
}
