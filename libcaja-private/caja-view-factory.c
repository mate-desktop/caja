/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*-

   caja-view-factory.c: register and create CajaViews

   Copyright (C) 2004 Red Hat Inc.

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

   Author: Alexander Larsson <alexl@redhat.com>
*/

#include "caja-view-factory.h"

static GList *registered_views;

void
caja_view_factory_register (CajaViewInfo *view_info)
{
    g_return_if_fail (view_info != NULL);
    g_return_if_fail (view_info->id != NULL);
    g_return_if_fail (caja_view_factory_lookup (view_info->id) == NULL);

    registered_views = g_list_append (registered_views, view_info);
}

const CajaViewInfo *
caja_view_factory_lookup (const char *id)
{
    GList *l;
    CajaViewInfo *view_info = NULL;

    g_return_val_if_fail (id != NULL, NULL);


    for (l = registered_views; l != NULL; l = l->next)
    {
        view_info = l->data;

        if (strcmp (view_info->id, id) == 0)
        {
            return view_info;
        }
    }
    return NULL;
}

CajaView *
caja_view_factory_create (const char *id,
                          CajaWindowSlotInfo *slot)
{
    const CajaViewInfo *view_info;
    CajaView *view;

    view_info = caja_view_factory_lookup (id);
    if (view_info == NULL)
    {
        return NULL;
    }

    view = view_info->create (slot);
    if (g_object_is_floating (view))
    {
        g_object_ref_sink (view);
    }
    return view;
}

gboolean
caja_view_factory_view_supports_uri (const char *id,
                                     GFile *location,
                                     GFileType file_type,
                                     const char *mime_type)
{
    const CajaViewInfo *view_info;
    char *uri;
    gboolean res;

    view_info = caja_view_factory_lookup (id);
    if (view_info == NULL)
    {
        return FALSE;
    }
    uri = g_file_get_uri (location);
    res = view_info->supports_uri (uri, file_type, mime_type);
    g_free (uri);
    return res;

}

GList *
caja_view_factory_get_views_for_uri (const char *uri,
                                     GFileType file_type,
                                     const char *mime_type)
{
    GList *l, *res;
    const CajaViewInfo *view_info = NULL;

    res = NULL;

    for (l = registered_views; l != NULL; l = l->next)
    {
        view_info = l->data;

        if (view_info->supports_uri (uri, file_type, mime_type))
        {
            if (view_info->single_view)
            {
                g_list_free_full (res, g_free);
                res = g_list_prepend (NULL, g_strdup (view_info->id));
                break;
            } else {
                res = g_list_prepend (res, g_strdup (view_info->id));
            }
        }
    }

    return g_list_reverse (res);
}


