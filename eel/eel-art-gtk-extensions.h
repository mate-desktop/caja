/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-art-gtk-extensions.h - Access gtk/gdk attributes as libart rectangles.

   Copyright (C) 2000 Eazel, Inc.

   The Mate Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Mate Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Mate Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
   Boston, MA 02110-1301, USA.

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

/* The following functions accept gtk/gdk structures and
 * return their bounds and dimensions, where:
 *
 * bounds: The (x,y) and (width, height) of something.
 * dimensions: The (width, height) of something.
 *
 * These are very useful in code that uses libart functions
 * to do operations on ArtIRects (such as intersection)
 */

#ifndef EEL_ART_GTK_EXTENSIONS_H
#define EEL_ART_GTK_EXTENSIONS_H

#include <eel/eel-gdk-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-art-extensions.h>

#ifdef __cplusplus
extern "C" {
#endif

    /* Convert between GdkRectangle and EelIRect and back */
    GdkRectangle  eel_irect_to_gdk_rectangle            (EelIRect      rectangle);
    EelIRect      eel_gdk_rectangle_to_eel_irect            (GdkRectangle  gdk_rectangle);
    EelDimensions eel_screen_get_dimensions                 (void);

    /* GdkWindow parent-relative bounds */
    EelIRect      eel_gdk_window_get_bounds                 (GdkWindow    *window);

    /* GdkWindow dimensions */
    EelDimensions eel_gdk_window_get_dimensions             (GdkWindow    *window);

    /* GdkWindow screen parent-relative bounds */
    EelIRect      eel_gdk_window_get_screen_relative_bounds (GdkWindow    *window);

    /* Clip a dirty area (from exposures) to the on screen parts of a GdkWindow */
    EelIRect      eel_gdk_window_clip_dirty_area_to_screen  (GdkWindow    *window,
            EelIRect      dirty_area);

    /* GtkWidget bounds and dimensions */
    EelIRect      eel_gtk_widget_get_bounds                 (GtkWidget    *widget);
    EelDimensions eel_gtk_widget_get_dimensions             (GtkWidget    *widget);
    EelDimensions eel_gtk_widget_get_preferred_dimensions   (GtkWidget    *widget);
    EelIPoint     eel_gdk_get_pointer_position              (void);

#ifdef __cplusplus
}
#endif

#endif /* EEL_ART_GTK_EXTENSIONS_H */
