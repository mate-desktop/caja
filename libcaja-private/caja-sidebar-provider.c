/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-sidebar-provider.c: register and create CajaSidebars

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

#include <string.h>
#include "caja-sidebar-provider.h"

static void
caja_sidebar_provider_base_init (gpointer g_class)
{
}

GType
caja_sidebar_provider_get_type (void)
{
    static GType type = 0;

    if (!type)
    {
        const GTypeInfo info =
        {
            sizeof (CajaSidebarProviderIface),
            caja_sidebar_provider_base_init,
            NULL,
            NULL,
            NULL,
            NULL,
            0,
            0,
            NULL
        };

        type = g_type_register_static (G_TYPE_INTERFACE,
                                       "CajaSidebarProvider",
                                       &info, 0);
        g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
    }

    return type;
}

CajaSidebar *
caja_sidebar_provider_create (CajaSidebarProvider *provider,
                              CajaWindowInfo  *window)
{
    return (* CAJA_SIDEBAR_PROVIDER_GET_IFACE (provider)->create) (provider, window);
}


GList *
caja_list_sidebar_providers (void)
{
    return NULL;
}
