/*
 *  caja-property-page.h - Property pages exported by
 *                             CajaPropertyProvider objects.
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

#ifndef CAJA_PROPERTY_PAGE_H
#define CAJA_PROPERTY_PAGE_H

#include <glib-object.h>
#include <gtk/gtk.h>
#include "caja-extension-types.h"

G_BEGIN_DECLS

#define CAJA_TYPE_PROPERTY_PAGE            (caja_property_page_get_type())
#define CAJA_PROPERTY_PAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_PROPERTY_PAGE, CajaPropertyPage))
#define CAJA_PROPERTY_PAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_PROPERTY_PAGE, CajaPropertyPageClass))
#define CAJA_IS_PROPERTY_PAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_PROPERTY_PAGE))
#define CAJA_IS_PROPERTY_PAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), CAJA_TYPE_PROPERTY_PAGE))
#define CAJA_PROPERTY_PAGE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), CAJA_TYPE_PROPERTY_PAGE, CajaPropertyPageClass))

typedef struct _CajaPropertyPage        CajaPropertyPage;
typedef struct _CajaPropertyPageDetails CajaPropertyPageDetails;
typedef struct _CajaPropertyPageClass   CajaPropertyPageClass;

struct _CajaPropertyPage {
    GObject parent;

    CajaPropertyPageDetails *details;
};

struct _CajaPropertyPageClass {
    GObjectClass parent;
};

GType             caja_property_page_get_type  (void);
CajaPropertyPage *caja_property_page_new       (const char *name,
                                                GtkWidget  *label,
                                                GtkWidget  *page);

/* CajaPropertyPage has the following properties:
 *   name (string)        - the identifier for the property page
 *   label (widget)       - the user-visible label of the property page
 *   page (widget)        - the property page to display
 */

G_END_DECLS

#endif
