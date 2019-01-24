/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Caja
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Caja is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Caja is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Andy Hertzfeld <andy@eazel.com>
 */

/* This is the header file for the property browser window, which
 * gives the user access to an extensible palette of properties which
 * can be dropped on various elements of the user interface to
 * customize them
 */

#ifndef CAJA_PROPERTY_BROWSER_H
#define CAJA_PROPERTY_BROWSER_H

#include <gdk/gdk.h>
#include <gtk/gtk.h>

typedef struct CajaPropertyBrowser CajaPropertyBrowser;
typedef struct CajaPropertyBrowserClass  CajaPropertyBrowserClass;

#define CAJA_TYPE_PROPERTY_BROWSER caja_property_browser_get_type()
#define CAJA_PROPERTY_BROWSER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_PROPERTY_BROWSER, CajaPropertyBrowser))
#define CAJA_PROPERTY_BROWSER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_PROPERTY_BROWSER, CajaPropertyBrowserClass))
#define CAJA_IS_PROPERTY_BROWSER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_PROPERTY_BROWSER))
#define CAJA_IS_PROPERTY_BROWSER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), CAJA_TYPE_PROPERTY_BROWSER))
#define CAJA_PROPERTY_BROWSER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), CAJA_TYPE_PROPERTY_BROWSER, CajaPropertyBrowserClass))

typedef struct _CajaPropertyBrowserPrivate CajaPropertyBrowserPrivate;

struct CajaPropertyBrowser
{
    GtkWindow window;
    CajaPropertyBrowserPrivate *details;
};

struct CajaPropertyBrowserClass
{
    GtkWindowClass parent_class;
};

GType                    caja_property_browser_get_type (void);
CajaPropertyBrowser *caja_property_browser_new      (GdkScreen               *screen);
void                     caja_property_browser_show     (GdkScreen               *screen);
void                     caja_property_browser_set_path (CajaPropertyBrowser *panel,
        const char              *new_path);

#endif /* CAJA_PROPERTY_BROWSER_H */
