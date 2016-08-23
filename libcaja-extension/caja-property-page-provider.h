/*
 *  caja-property-page-provider.h - Interface for Caja extensions
 *                                      that provide property pages.
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

/* This interface is implemented by Caja extensions that want to
 * add property page to property dialogs.  Extensions are called when
 * Caja needs property pages for a selection.  They are passed a
 * list of CajaFileInfo objects for which information should
 * be displayed  */

#ifndef CAJA_PROPERTY_PAGE_PROVIDER_H
#define CAJA_PROPERTY_PAGE_PROVIDER_H

#include <glib-object.h>
#include "caja-extension-types.h"
#include "caja-file-info.h"
#include "caja-property-page.h"

G_BEGIN_DECLS

#define CAJA_TYPE_PROPERTY_PAGE_PROVIDER           (caja_property_page_provider_get_type ())
#define CAJA_PROPERTY_PAGE_PROVIDER(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_PROPERTY_PAGE_PROVIDER, CajaPropertyPageProvider))
#define CAJA_IS_PROPERTY_PAGE_PROVIDER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_PROPERTY_PAGE_PROVIDER))
#define CAJA_PROPERTY_PAGE_PROVIDER_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), CAJA_TYPE_PROPERTY_PAGE_PROVIDER, CajaPropertyPageProviderIface))

typedef struct _CajaPropertyPageProvider       CajaPropertyPageProvider;
typedef struct _CajaPropertyPageProviderIface  CajaPropertyPageProviderIface;

/**
 * CajaPropertyPageProviderIface:
 * @g_iface: The parent interface.
 * @get_pages: Returns a #GList of #CajaPropertyPage.
 *   See caja_property_page_provider_get_pages() for details.
 *
 * Interface for extensions to provide additional property pages.
 */

struct _CajaPropertyPageProviderIface {
    GTypeInterface g_iface;

    GList *(*get_pages) (CajaPropertyPageProvider *provider,
                         GList                    *files);
};

/* Interface Functions */
GType  caja_property_page_provider_get_type  (void);
GList *caja_property_page_provider_get_pages (CajaPropertyPageProvider *provider,
                                              GList                    *files);

G_END_DECLS

#endif
