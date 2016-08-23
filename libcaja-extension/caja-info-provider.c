/*
 *  caja-info-provider.c - Interface for Caja extensions that
 *                             provide info about files.
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
#include "caja-info-provider.h"

#include <glib-object.h>

/**
 * SECTION:caja-info-provider
 * @title: CajaInfoProvider
 * @short_description: Interface to provide additional information about files
 * @include: libcaja-extension/caja-column-provider.h
 *
 * #CajaInfoProvider allows extension to provide additional information about
 * files. When caja_info_provider_update_file_info() is called by the application,
 * extensions will know that it's time to add extra information to the provided
 * #CajaFileInfo.
 */

static void
caja_info_provider_base_init (gpointer g_class)
{
}

GType
caja_info_provider_get_type (void)
{
    static GType type = 0;

    if (!type) {
        const GTypeInfo info = {
            sizeof (CajaInfoProviderIface),
            caja_info_provider_base_init,
            NULL,
            NULL,
            NULL,
            NULL,
            0,
            0,
            NULL
        };

        type = g_type_register_static (G_TYPE_INTERFACE,
                                       "CajaInfoProvider",
                                       &info, 0);
        g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
    }

    return type;
}

CajaOperationResult
caja_info_provider_update_file_info (CajaInfoProvider     *provider,
                                     CajaFileInfo         *file,
                                     GClosure             *update_complete,
                                     CajaOperationHandle **handle)
{
    g_return_val_if_fail (CAJA_IS_INFO_PROVIDER (provider),
                          CAJA_OPERATION_FAILED);
    g_return_val_if_fail (CAJA_INFO_PROVIDER_GET_IFACE (provider)->update_file_info != NULL,
                          CAJA_OPERATION_FAILED);
    g_return_val_if_fail (update_complete != NULL,
                          CAJA_OPERATION_FAILED);
    g_return_val_if_fail (handle != NULL, CAJA_OPERATION_FAILED);

    return CAJA_INFO_PROVIDER_GET_IFACE (provider)->update_file_info
           (provider, file, update_complete, handle);
}

void
caja_info_provider_cancel_update (CajaInfoProvider    *provider,
                                  CajaOperationHandle *handle)
{
    g_return_if_fail (CAJA_IS_INFO_PROVIDER (provider));
    g_return_if_fail (CAJA_INFO_PROVIDER_GET_IFACE (provider)->cancel_update != NULL);
    g_return_if_fail (handle != NULL);

    CAJA_INFO_PROVIDER_GET_IFACE (provider)->cancel_update (provider,
            handle);
}

void
caja_info_provider_update_complete_invoke (GClosure            *update_complete,
                                           CajaInfoProvider    *provider,
                                           CajaOperationHandle *handle,
                                           CajaOperationResult  result)
{
    GValue args[3] = { { 0, } };
    GValue return_val = { 0, };

    g_return_if_fail (update_complete != NULL);
    g_return_if_fail (CAJA_IS_INFO_PROVIDER (provider));

    g_value_init (&args[0], CAJA_TYPE_INFO_PROVIDER);
    g_value_init (&args[1], G_TYPE_POINTER);
    g_value_init (&args[2], CAJA_TYPE_OPERATION_RESULT);

    g_value_set_object (&args[0], provider);
    g_value_set_pointer (&args[1], handle);
    g_value_set_enum (&args[2], result);

    g_closure_invoke (update_complete, &return_val, 3, args, NULL);

    g_value_unset (&args[0]);
    g_value_unset (&args[1]);
    g_value_unset (&args[2]);
}

