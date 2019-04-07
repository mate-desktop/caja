/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-sidebar-provider.h: register and create CajaSidebars

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

#ifndef CAJA_SIDEBAR_PROVIDER_H
#define CAJA_SIDEBAR_PROVIDER_H

#include "caja-sidebar.h"
#include "caja-window-info.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CAJA_TYPE_SIDEBAR_PROVIDER           (caja_sidebar_provider_get_type ())
#define CAJA_SIDEBAR_PROVIDER(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_SIDEBAR_PROVIDER, CajaSidebarProvider))
#define CAJA_IS_SIDEBAR_PROVIDER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_SIDEBAR_PROVIDER))
#define CAJA_SIDEBAR_PROVIDER_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), CAJA_TYPE_SIDEBAR_PROVIDER, CajaSidebarProviderIface))

    typedef struct _CajaSidebarProvider       CajaSidebarProvider;
    typedef struct _CajaSidebarProviderIface  CajaSidebarProviderIface;

    struct _CajaSidebarProviderIface
    {
        GTypeInterface g_iface;

        CajaSidebar * (*create) (CajaSidebarProvider *provider,
                                 CajaWindowInfo *window);
    };

    /* Interface Functions */
    GType                   caja_sidebar_provider_get_type  (void);
    CajaSidebar *       caja_sidebar_provider_create (CajaSidebarProvider *provider,
            CajaWindowInfo  *window);
    GList *                 caja_list_sidebar_providers (void);

#ifdef __cplusplus
}
#endif

#endif /* CAJA_SIDEBAR_PROVIDER_H */
