/*
 *  caja-configurable.c - Interface for configuration
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
 *  Author: 20kdc <gamemanj@hotmail.co.uk>
 *  Based on caja-menu-provider.c by Dave Camp <dave@ximian.com>
 *
 */

#include <config.h>
#include "caja-configurable.h"

#include <glib-object.h>

/**
 * SECTION:caja-configurable
 * @title: CajaConfigurable
 * @short_description: Interface to allow an extension to be configured
 * @include: libcaja-extension/caja-configurable.h
 *
 * #CajaConfigurable allows an extension to show a configuration page.
 * The presence of CajaConfigurable enables the 'Configure' button.
 */

static void
caja_configurable_base_init (gpointer g_class)
{
}

GType
caja_configurable_get_type (void)
{
    static GType type = 0;

    if (!type) {
        const GTypeInfo info = {
            sizeof (CajaConfigurableIface),
            caja_configurable_base_init,
            NULL,
            NULL,
            NULL,
            NULL,
            0,
            0,
            NULL
        };

        type = g_type_register_static (G_TYPE_INTERFACE,
                                       "CajaConfigurable",
                                       &info, 0);
        g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
    }

    return type;
}

/**
 * caja_configurable_run:
 * @provider: a #CajaConfigurable
 */
void
caja_configurable_run_config (CajaConfigurable *provider)
{
    if (!CAJA_IS_CONFIGURABLE(provider)) {
        return;
    }

    if (CAJA_CONFIGURABLE_GET_IFACE (provider)->run_config) {
        CAJA_CONFIGURABLE_GET_IFACE (provider)->run_config(provider);
    }
}


