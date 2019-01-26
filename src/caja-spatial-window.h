/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Caja
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000, 2001 Eazel, Inc.
 *  Copyright (C) 2003 Ximian, Inc.
 *
 *  Caja is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  Caja is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */
/* caja-window.h: Interface of the main window object */

#ifndef CAJA_SPATIAL_WINDOW_H
#define CAJA_SPATIAL_WINDOW_H

#include "caja-window.h"
#include "caja-window-private.h"

#define CAJA_TYPE_SPATIAL_WINDOW caja_spatial_window_get_type()
#define CAJA_SPATIAL_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_SPATIAL_WINDOW, CajaSpatialWindow))
#define CAJA_SPATIAL_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_SPATIAL_WINDOW, CajaSpatialWindowClass))
#define CAJA_IS_SPATIAL_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_SPATIAL_WINDOW))
#define CAJA_IS_SPATIAL_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), CAJA_TYPE_SPATIAL_WINDOW))
#define CAJA_SPATIAL_WINDOW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), CAJA_TYPE_SPATIAL_WINDOW, CajaSpatialWindowClass))

#ifndef CAJA_SPATIAL_WINDOW_DEFINED
#define CAJA_SPATIAL_WINDOW_DEFINED
typedef struct _CajaSpatialWindow        CajaSpatialWindow;
#endif
typedef struct _CajaSpatialWindowClass   CajaSpatialWindowClass;
typedef struct _CajaSpatialWindowPrivate CajaSpatialWindowPrivate;

struct _CajaSpatialWindow
{
    CajaWindow parent_object;

    CajaSpatialWindowPrivate *details;
};

struct _CajaSpatialWindowClass
{
    CajaWindowClass parent_spot;
};


GType            caja_spatial_window_get_type			(void);
void             caja_spatial_window_set_location_button		(CajaSpatialWindow *window,
        GFile                 *location);

#endif
