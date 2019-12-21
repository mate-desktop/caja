/* vi: set sw=4 ts=4 wrap ai: */
/*
 * caja-widget-view-provider.h: This file is part of caja.
 *
 * Copyright (C) 2019 Wu Xiaotian <yetist@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * */

#ifndef __CAJA_WIDGET_VIEW_PROVIDER_H__
#define __CAJA_WIDGET_VIEW_PROVIDER_H__  1

#include <glib-object.h>
#include <gtk/gtk.h>
#include "caja-file-info.h"
#include "caja-extension-types.h"

G_BEGIN_DECLS

#define CAJA_TYPE_WIDGET_VIEW_PROVIDER           (caja_widget_view_provider_get_type ())
#define CAJA_WIDGET_VIEW_PROVIDER(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_WIDGET_VIEW_PROVIDER, CajaWidgetViewProvider))
#define CAJA_IS_WIDGET_VIEW_PROVIDER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_WIDGET_VIEW_PROVIDER))
#define CAJA_WIDGET_VIEW_PROVIDER_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), CAJA_TYPE_WIDGET_VIEW_PROVIDER, CajaWidgetViewProviderIface))

typedef struct _CajaWidgetViewProvider       CajaWidgetViewProvider;
typedef struct _CajaWidgetViewProviderIface  CajaWidgetViewProviderIface;

/**
 * CajaWidgetViewProviderIface:
 * @supports_uri: Whether this extension works for this uri
 * @get_widget: Returns a #GtkWidget.
 *   See caja_widget_view_provider_get_widget() for details.
 * @add_file: Adds a file to this widget view.
 * @set_location: Set location to this widget view.
 * @set_window: Set the main window to this widget view.
 * @get_item_count: Return the item count of this widget view.
 * @get_first_visible_file: Return the first visible file from this widget view.
 * @clear: Clear items in this widget view.
 *
 * Interface for extensions to provide widgets view for content.
 */
struct _CajaWidgetViewProviderIface {
    GTypeInterface g_iface;

    gboolean  (*supports_uri)   (CajaWidgetViewProvider *provider,
                                 const char *uri,
                                 GFileType file_type,
                                 const char *mime_type);
    GtkWidget* (*get_widget)     (CajaWidgetViewProvider *provider);
    void       (*add_file)       (CajaWidgetViewProvider *provider, CajaFile *file, CajaFile *directory);
    void       (*set_location)   (CajaWidgetViewProvider *provider, const char *location);
    void       (*set_window)     (CajaWidgetViewProvider *provider, GtkWindow *window);
    guint      (*get_item_count) (CajaWidgetViewProvider *provider);
    gchar*     (*get_first_visible_file) (CajaWidgetViewProvider *provider);
    void       (*clear)          (CajaWidgetViewProvider *provider);
};

/* Interface Functions */
GType      caja_widget_view_provider_get_type       (void);

GtkWidget *caja_widget_view_provider_get_widget     (CajaWidgetViewProvider *provider);
void       caja_widget_view_provider_add_file       (CajaWidgetViewProvider *provider,
                                                     CajaFile *file,
                                                     CajaFile *directory);
void       caja_widget_view_provider_set_location   (CajaWidgetViewProvider *provider,
                                                     const char *location);
void       caja_widget_view_provider_set_window     (CajaWidgetViewProvider *provider,
                                                     GtkWindow *window);
guint      caja_widget_view_provider_get_item_count (CajaWidgetViewProvider *provider);
gchar*     caja_widget_view_provider_get_first_visible_file (CajaWidgetViewProvider *provider);
void       caja_widget_view_provider_clear          (CajaWidgetViewProvider *provider);
gboolean   caja_widget_view_provider_supports_uri   (CajaWidgetViewProvider *provider,
                                                     const char *uri,
                                                     GFileType file_type,
                                                     const char *mime_type);
G_END_DECLS

#endif /* __CAJA_WIDGET_VIEW_PROVIDER_H__ */
