/*
 *  caja-property-page-provider.c - Interface for Caja extensions
 *                                      that provide property pages for
 *                                      files.
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
#include "caja-property-page-provider.h"

#include <glib-object.h>

/**
 * SECTION:caja-property-page-provider
 * @title: CajaPropertyPageProvider
 * @short_description: Interface to provide additional property pages
 * @include: libcaja-extension/caja-property-page-provider.h
 *
 * #CajaPropertyPageProvider allows extension to provide additional pages
 * for the file properties dialog.
 */

static void
caja_property_page_provider_base_init (gpointer g_class)
{
}

GType
caja_property_page_provider_get_type (void)
{
    static GType type = 0;

    if (!type) {
        const GTypeInfo info = {
            sizeof (CajaPropertyPageProviderIface),
            caja_property_page_provider_base_init,
            NULL,
            NULL,
            NULL,
            NULL,
            0,
            0,
            NULL
        };

        type = g_type_register_static (G_TYPE_INTERFACE,
                                       "CajaPropertyPageProvider",
                                       &info, 0);
        g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
    }

    return type;
}

/**
 * caja_property_page_provider_get_pages:
 * @provider: a #CajaPropertyPageProvider
 * @files: (element-type CajaFileInfo): a #GList of #CajaFileInfo
 *
 * This function is called by Caja when it wants property page
 * items from the extension.
 *
 * This function is called in the main thread before a property page
 * is shown, so it should return quickly.
 *
 * Returns: (element-type CajaPropertyPage) (transfer full): A #GList of allocated #CajaPropertyPage items.
 */
GList *
caja_property_page_provider_get_pages (CajaPropertyPageProvider *provider,
                                       GList *files)
{
    g_return_val_if_fail (CAJA_IS_PROPERTY_PAGE_PROVIDER (provider), NULL);
    g_return_val_if_fail (CAJA_PROPERTY_PAGE_PROVIDER_GET_IFACE (provider)->get_pages != NULL, NULL);

    return CAJA_PROPERTY_PAGE_PROVIDER_GET_IFACE (provider)->get_pages
           (provider, files);
}


