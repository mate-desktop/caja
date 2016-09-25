/*
 *  caja-info-provider.h - Interface for Caja extensions that
 *                             provide info about files.
 *
 *  Copyright (C) 2003 Novell, Inc.
 *  Copyright (C) 2005 Red Hat, Inc.
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
 *           Alexander Larsson <alexl@redhat.com>
 *
 */

/* This interface is implemented by Caja extensions that want to
 * provide extra location widgets for a particular location.
 * Extensions are called when Caja displays a location.
 */

#ifndef CAJA_LOCATION_WIDGET_PROVIDER_H
#define CAJA_LOCATION_WIDGET_PROVIDER_H

#include <glib-object.h>
#include <gtk/gtk.h>
#include "caja-extension-types.h"

G_BEGIN_DECLS

#define CAJA_TYPE_LOCATION_WIDGET_PROVIDER           (caja_location_widget_provider_get_type ())
#define CAJA_LOCATION_WIDGET_PROVIDER(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_LOCATION_WIDGET_PROVIDER, CajaLocationWidgetProvider))
#define CAJA_IS_LOCATION_WIDGET_PROVIDER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_LOCATION_WIDGET_PROVIDER))
#define CAJA_LOCATION_WIDGET_PROVIDER_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), CAJA_TYPE_LOCATION_WIDGET_PROVIDER, CajaLocationWidgetProviderIface))

typedef struct _CajaLocationWidgetProvider       CajaLocationWidgetProvider;
typedef struct _CajaLocationWidgetProviderIface  CajaLocationWidgetProviderIface;

/**
 * CajaLocationWidgetProviderIface:
 * @g_iface: The parent interface.
 * @get_widget: Returns a #GtkWidget.
 *   See caja_location_widget_provider_get_widget() for details.
 *
 * Interface for extensions to provide additional location widgets.
 */
struct _CajaLocationWidgetProviderIface {
    GTypeInterface g_iface;

    GtkWidget *(*get_widget) (CajaLocationWidgetProvider *provider,
                              const char                 *uri,
                              GtkWidget                  *window);
};

/* Interface Functions */
GType      caja_location_widget_provider_get_type   (void);
GtkWidget *caja_location_widget_provider_get_widget (CajaLocationWidgetProvider *provider,
                                                     const char                 *uri,
                                                     GtkWidget                  *window);
G_END_DECLS

#endif
