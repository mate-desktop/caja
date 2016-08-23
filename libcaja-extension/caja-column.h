/*
 *  caja-column.h - Info columns exported by
 *                      CajaColumnProvider objects.
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

#ifndef CAJA_COLUMN_H
#define CAJA_COLUMN_H

#include <glib-object.h>
#include "caja-extension-types.h"

G_BEGIN_DECLS

#define CAJA_TYPE_COLUMN            (caja_column_get_type())
#define CAJA_COLUMN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_COLUMN, CajaColumn))
#define CAJA_COLUMN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_COLUMN, CajaColumnClass))
#define CAJA_INFO_IS_COLUMN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_COLUMN))
#define CAJA_INFO_IS_COLUMN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), CAJA_TYPE_COLUMN))
#define CAJA_COLUMN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), CAJA_TYPE_COLUMN, CajaColumnClass))

typedef struct _CajaColumn        CajaColumn;
typedef struct _CajaColumnDetails CajaColumnDetails;
typedef struct _CajaColumnClass   CajaColumnClass;

struct _CajaColumn {
    GObject parent;

    CajaColumnDetails *details;
};

struct _CajaColumnClass {
    GObjectClass parent;
};

GType       caja_column_get_type  (void);
CajaColumn *caja_column_new       (const char *name,
                                   const char *attribute,
                                   const char *label,
                                   const char *description);

/* CajaColumn has the following properties:
 *   name (string)        - the identifier for the column
 *   attribute (string)   - the file attribute to be displayed in the
 *                          column
 *   label (string)       - the user-visible label for the column
 *   description (string) - a user-visible description of the column
 *   xalign (float)       - x-alignment of the column
 */

G_END_DECLS

#endif
