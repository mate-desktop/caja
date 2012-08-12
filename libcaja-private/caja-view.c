/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-view.c: Interface for caja views

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

#include <config.h>
#include "caja-view.h"

enum
{
    TITLE_CHANGED,
    ZOOM_LEVEL_CHANGED,
    LAST_SIGNAL
};

static guint caja_view_signals[LAST_SIGNAL] = { 0 };

static void
caja_view_base_init (gpointer g_class)
{
    static gboolean initialized = FALSE;

    if (! initialized)
    {
        caja_view_signals[TITLE_CHANGED] =
            g_signal_new ("title_changed",
                          CAJA_TYPE_VIEW,
                          G_SIGNAL_RUN_LAST,
                          G_STRUCT_OFFSET (CajaViewIface, title_changed),
                          NULL, NULL,
                          g_cclosure_marshal_VOID__VOID,
                          G_TYPE_NONE, 0);

        caja_view_signals[ZOOM_LEVEL_CHANGED] =
            g_signal_new ("zoom_level_changed",
                          CAJA_TYPE_VIEW,
                          G_SIGNAL_RUN_LAST,
                          G_STRUCT_OFFSET (CajaViewIface, zoom_level_changed),
                          NULL, NULL,
                          g_cclosure_marshal_VOID__VOID,
                          G_TYPE_NONE, 0);

        initialized = TRUE;
    }
}

GType
caja_view_get_type (void)
{
    static GType type = 0;

    if (!type)
    {
        const GTypeInfo info =
        {
            sizeof (CajaViewIface),
            caja_view_base_init,
            NULL,
            NULL,
            NULL,
            NULL,
            0,
            0,
            NULL
        };

        type = g_type_register_static (G_TYPE_INTERFACE,
                                       "CajaView",
                                       &info, 0);
        g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
    }

    return type;
}

const char *
caja_view_get_view_id (CajaView *view)
{
    g_return_val_if_fail (CAJA_IS_VIEW (view), NULL);

    return (* CAJA_VIEW_GET_IFACE (view)->get_view_id) (view);
}

GtkWidget *
caja_view_get_widget (CajaView *view)
{
    g_return_val_if_fail (CAJA_IS_VIEW (view), NULL);

    return (* CAJA_VIEW_GET_IFACE (view)->get_widget) (view);
}

void
caja_view_load_location (CajaView *view,
                         const char   *location_uri)
{
    g_return_if_fail (CAJA_IS_VIEW (view));
    g_return_if_fail (location_uri != NULL);

    (* CAJA_VIEW_GET_IFACE (view)->load_location) (view,
            location_uri);
}

void
caja_view_stop_loading (CajaView *view)
{
    g_return_if_fail (CAJA_IS_VIEW (view));

    (* CAJA_VIEW_GET_IFACE (view)->stop_loading) (view);
}

int
caja_view_get_selection_count (CajaView *view)
{
    g_return_val_if_fail (CAJA_IS_VIEW (view), 0);

    return (* CAJA_VIEW_GET_IFACE (view)->get_selection_count) (view);
}

GList *
caja_view_get_selection (CajaView *view)
{
    g_return_val_if_fail (CAJA_IS_VIEW (view), NULL);

    return (* CAJA_VIEW_GET_IFACE (view)->get_selection) (view);
}

void
caja_view_set_selection (CajaView *view,
                         GList        *list)
{
    g_return_if_fail (CAJA_IS_VIEW (view));

    (* CAJA_VIEW_GET_IFACE (view)->set_selection) (view,
            list);
}

void
caja_view_set_is_active (CajaView *view,
                         gboolean is_active)
{
    g_return_if_fail (CAJA_IS_VIEW (view));

    (* CAJA_VIEW_GET_IFACE (view)->set_is_active) (view,
            is_active);
}

void
caja_view_invert_selection (CajaView *view)
{
    g_return_if_fail (CAJA_IS_VIEW (view));

    (* CAJA_VIEW_GET_IFACE (view)->invert_selection) (view);
}

char *
caja_view_get_first_visible_file (CajaView *view)
{
    g_return_val_if_fail (CAJA_IS_VIEW (view), NULL);

    return (* CAJA_VIEW_GET_IFACE (view)->get_first_visible_file) (view);
}

void
caja_view_scroll_to_file (CajaView *view,
                          const char   *uri)
{
    g_return_if_fail (CAJA_IS_VIEW (view));

    (* CAJA_VIEW_GET_IFACE (view)->scroll_to_file) (view, uri);
}

char *
caja_view_get_title (CajaView *view)
{
    g_return_val_if_fail (CAJA_IS_VIEW (view), NULL);

    if (CAJA_VIEW_GET_IFACE (view)->get_title != NULL)
    {
        return (* CAJA_VIEW_GET_IFACE (view)->get_title) (view);
    }
    else
    {
        return NULL;
    }
}


gboolean
caja_view_supports_zooming (CajaView *view)
{
    g_return_val_if_fail (CAJA_IS_VIEW (view), FALSE);

    return (* CAJA_VIEW_GET_IFACE (view)->supports_zooming) (view);
}

void
caja_view_bump_zoom_level (CajaView *view,
                           int zoom_increment)
{
    g_return_if_fail (CAJA_IS_VIEW (view));

    (* CAJA_VIEW_GET_IFACE (view)->bump_zoom_level) (view,
            zoom_increment);
}

void
caja_view_zoom_to_level (CajaView      *view,
                         CajaZoomLevel  level)
{
    g_return_if_fail (CAJA_IS_VIEW (view));

    (* CAJA_VIEW_GET_IFACE (view)->zoom_to_level) (view,
            level);
}

void
caja_view_restore_default_zoom_level (CajaView *view)
{
    g_return_if_fail (CAJA_IS_VIEW (view));

    (* CAJA_VIEW_GET_IFACE (view)->restore_default_zoom_level) (view);
}

gboolean
caja_view_can_zoom_in (CajaView *view)
{
    g_return_val_if_fail (CAJA_IS_VIEW (view), FALSE);

    return (* CAJA_VIEW_GET_IFACE (view)->can_zoom_in) (view);
}

gboolean
caja_view_can_zoom_out (CajaView *view)
{
    g_return_val_if_fail (CAJA_IS_VIEW (view), FALSE);

    return (* CAJA_VIEW_GET_IFACE (view)->can_zoom_out) (view);
}

CajaZoomLevel
caja_view_get_zoom_level (CajaView *view)
{
    g_return_val_if_fail (CAJA_IS_VIEW (view), CAJA_ZOOM_LEVEL_STANDARD);

    return (* CAJA_VIEW_GET_IFACE (view)->get_zoom_level) (view);
}

void
caja_view_grab_focus (CajaView   *view)
{
    g_return_if_fail (CAJA_IS_VIEW (view));

    if (CAJA_VIEW_GET_IFACE (view)->grab_focus != NULL)
    {
        (* CAJA_VIEW_GET_IFACE (view)->grab_focus) (view);
    }
}

void
caja_view_update_menus (CajaView *view)
{
    g_return_if_fail (CAJA_IS_VIEW (view));

    if (CAJA_VIEW_GET_IFACE (view)->update_menus != NULL)
    {
        (* CAJA_VIEW_GET_IFACE (view)->update_menus) (view);
    }
}

void
caja_view_pop_up_location_context_menu (CajaView   *view,
                                        GdkEventButton *event,
                                        const char     *location)
{
    g_return_if_fail (CAJA_IS_VIEW (view));

    if (CAJA_VIEW_GET_IFACE (view)->pop_up_location_context_menu != NULL)
    {
        (* CAJA_VIEW_GET_IFACE (view)->pop_up_location_context_menu) (view, event, location);
    }
}

void
caja_view_drop_proxy_received_uris   (CajaView         *view,
                                      GList                *uris,
                                      const char           *target_location,
                                      GdkDragAction         action)
{
    g_return_if_fail (CAJA_IS_VIEW (view));

    if (CAJA_VIEW_GET_IFACE (view)->drop_proxy_received_uris != NULL)
    {
        (* CAJA_VIEW_GET_IFACE (view)->drop_proxy_received_uris) (view, uris, target_location, action);
    }
}

void
caja_view_drop_proxy_received_netscape_url (CajaView         *view,
        const char           *source_url,
        const char           *target_location,
        GdkDragAction         action)
{
    g_return_if_fail (CAJA_IS_VIEW (view));

    if (CAJA_VIEW_GET_IFACE (view)->drop_proxy_received_netscape_url != NULL)
    {
        (* CAJA_VIEW_GET_IFACE (view)->drop_proxy_received_netscape_url) (view, source_url, target_location, action);
    }
}


