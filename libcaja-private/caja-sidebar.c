/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-sidebar.c: Interface for caja sidebar plugins

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
#include "caja-sidebar.h"

enum
{
    TAB_ICON_CHANGED,
    ZOOM_PARAMETERS_CHANGED,
    ZOOM_LEVEL_CHANGED,
    LAST_SIGNAL
};

static guint caja_sidebar_signals[LAST_SIGNAL] = { 0 };

static void
caja_sidebar_base_init (gpointer g_class)
{
    static gboolean initialized = FALSE;

    if (! initialized)
    {
        caja_sidebar_signals[TAB_ICON_CHANGED] =
            g_signal_new ("tab_icon_changed",
                          CAJA_TYPE_SIDEBAR,
                          G_SIGNAL_RUN_LAST,
                          G_STRUCT_OFFSET (CajaSidebarIface, tab_icon_changed),
                          NULL, NULL,
                          g_cclosure_marshal_VOID__VOID,
                          G_TYPE_NONE, 0);

        initialized = TRUE;
    }
}

GType
caja_sidebar_get_type (void)
{
    static GType type = 0;

    if (!type)
    {
        const GTypeInfo info =
        {
            sizeof (CajaSidebarIface),
            caja_sidebar_base_init,
            NULL,
            NULL,
            NULL,
            NULL,
            0,
            0,
            NULL
        };

        type = g_type_register_static (G_TYPE_INTERFACE,
                                       "CajaSidebar",
                                       &info, 0);
        g_type_interface_add_prerequisite (type, GTK_TYPE_WIDGET);
    }

    return type;
}


const char *
caja_sidebar_get_sidebar_id (CajaSidebar *sidebar)
{
    g_return_val_if_fail (CAJA_IS_SIDEBAR (sidebar), NULL);

    return (* CAJA_SIDEBAR_GET_IFACE (sidebar)->get_sidebar_id) (sidebar);
}

char *
caja_sidebar_get_tab_label (CajaSidebar *sidebar)
{
    g_return_val_if_fail (CAJA_IS_SIDEBAR (sidebar), NULL);

    return (* CAJA_SIDEBAR_GET_IFACE (sidebar)->get_tab_label) (sidebar);
}

char *
caja_sidebar_get_tab_tooltip (CajaSidebar *sidebar)
{
    g_return_val_if_fail (CAJA_IS_SIDEBAR (sidebar), NULL);

    return (* CAJA_SIDEBAR_GET_IFACE (sidebar)->get_tab_tooltip) (sidebar);
}

GdkPixbuf *
caja_sidebar_get_tab_icon (CajaSidebar *sidebar)
{
    g_return_val_if_fail (CAJA_IS_SIDEBAR (sidebar), NULL);

    return (* CAJA_SIDEBAR_GET_IFACE (sidebar)->get_tab_icon) (sidebar);
}

void
caja_sidebar_is_visible_changed (CajaSidebar *sidebar,
                                 gboolean         is_visible)
{
    g_return_if_fail (CAJA_IS_SIDEBAR (sidebar));

    (* CAJA_SIDEBAR_GET_IFACE (sidebar)->is_visible_changed) (sidebar,
            is_visible);
}
