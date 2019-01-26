/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Caja
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Caja is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Caja is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors: Darin Adler <darin@bentspoon.com>
 */

/* caja-desktop-window.h
 */

#ifndef CAJA_DESKTOP_WINDOW_H
#define CAJA_DESKTOP_WINDOW_H

#include "caja-window.h"
#include "caja-application.h"
#include "caja-spatial-window.h"

#include <gtk/gtk-a11y.h>

#define CAJA_TYPE_DESKTOP_WINDOW caja_desktop_window_get_type()
#define CAJA_DESKTOP_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_DESKTOP_WINDOW, CajaDesktopWindow))
#define CAJA_DESKTOP_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_DESKTOP_WINDOW, CajaDesktopWindowClass))
#define CAJA_IS_DESKTOP_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_DESKTOP_WINDOW))
#define CAJA_IS_DESKTOP_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), CAJA_TYPE_DESKTOP_WINDOW))
#define CAJA_DESKTOP_WINDOW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), CAJA_TYPE_DESKTOP_WINDOW, CajaDesktopWindowClass))

typedef struct _CajaDesktopWindowPrivate CajaDesktopWindowPrivate;

typedef struct
{
    CajaSpatialWindow parent_spot;
    CajaDesktopWindowPrivate *details;
    gboolean affect_desktop_on_next_location_change;
} CajaDesktopWindow;

typedef struct
{
    CajaSpatialWindowClass parent_spot;
} CajaDesktopWindowClass;

GType                  caja_desktop_window_get_type            (void);
CajaDesktopWindow *caja_desktop_window_new                 (CajaApplication *application,
        GdkScreen           *screen);
void                   caja_desktop_window_update_directory    (CajaDesktopWindow *window);
gboolean               caja_desktop_window_loaded              (CajaDesktopWindow *window);

#define CAJA_TYPE_DESKTOP_WINDOW_ACCESSIBLE caja_desktop_window_accessible_get_type()

typedef struct
{
  GtkWindowAccessible parent_spot;
} CajaDesktopWindowAccessible;

typedef struct
{
  GtkWindowAccessibleClass parent_spot;
} CajaDesktopWindowAccessibleClass;

#endif /* CAJA_DESKTOP_WINDOW_H */
