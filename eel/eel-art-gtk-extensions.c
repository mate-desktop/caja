/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-eel-gtk-extensions.c - Access gtk/gdk attributes as libeel rectangles.

   Copyright (C) 2000 Eazel, Inc.

   The Mate Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Mate Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PEELICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Mate Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
   Boston, MA 02110-1301, USA.

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#include <config.h>

#include "eel-art-gtk-extensions.h"

/**
 * eel_gtk_widget_get_bounds:
 * @gtk_widget: The source GtkWidget.
 *
 * Return value: An EelIRect representation of the given GtkWidget's geometry
 * relative to its parent.  In the Gtk universe this is known as "allocation."
 *
 */
EelIRect
eel_gtk_widget_get_bounds (GtkWidget *gtk_widget)
{
    GtkAllocation allocation;
    g_return_val_if_fail (GTK_IS_WIDGET (gtk_widget), eel_irect_empty);

    gtk_widget_get_allocation (gtk_widget, &allocation);
    return eel_irect_assign (allocation.x,
                             allocation.y,
                             (int) allocation.width,
                             (int) allocation.height);
}

/**
 * eel_gtk_widget_get_dimensions:
 * @gtk_widget: The source GtkWidget.
 *
 * Return value: The widget's dimensions.  The returned dimensions are only valid
 *               after the widget's geometry has been "allocated" by its container.
 */
EelDimensions
eel_gtk_widget_get_dimensions (GtkWidget *gtk_widget)
{
    EelDimensions dimensions;
    GtkAllocation allocation;

    g_return_val_if_fail (GTK_IS_WIDGET (gtk_widget), eel_dimensions_empty);

    gtk_widget_get_allocation (gtk_widget, &allocation);
    dimensions.width = (int) allocation.width;
    dimensions.height = (int) allocation.height;

    return dimensions;
}
