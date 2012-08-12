/*
 *  caja-column-provider.c - Interface for Caja extensions
 *                               that provide column specifications.
 *
 *  Copyright (C) 2003 Novell, Inc.
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
 *  Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  Author:  Dave Camp <dave@ximian.com>
 *
 */

#include <config.h>
#include "caja-column-provider.h"

#include <glib-object.h>

static void
caja_column_provider_base_init (gpointer g_class)
{
}

GType
caja_column_provider_get_type (void)
{
    static GType type = 0;

    if (!type)
    {
        const GTypeInfo info =
        {
            sizeof (CajaColumnProviderIface),
            caja_column_provider_base_init,
            NULL,
            NULL,
            NULL,
            NULL,
            0,
            0,
            NULL
        };

        type = g_type_register_static (G_TYPE_INTERFACE,
                                       "CajaColumnProvider",
                                       &info, 0);
        g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
    }

    return type;
}

GList *
caja_column_provider_get_columns (CajaColumnProvider *provider)
{
    g_return_val_if_fail (CAJA_IS_COLUMN_PROVIDER (provider), NULL);
    g_return_val_if_fail (CAJA_COLUMN_PROVIDER_GET_IFACE (provider)->get_columns != NULL, NULL);

    return CAJA_COLUMN_PROVIDER_GET_IFACE (provider)->get_columns
           (provider);
}


