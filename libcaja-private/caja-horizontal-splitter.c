/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* caja-horizontal-splitter.c - A horizontal splitter with a semi gradient look

   Copyright (C) 1999, 2000 Eazel, Inc.

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

#include <config.h>
#include "caja-horizontal-splitter.h"

#include <eel/eel-gtk-macros.h>
#include <stdlib.h>

struct CajaHorizontalSplitterDetails
{
    guint32 press_time;
    int press_position;
    int saved_size;
};

#define CLOSED_THRESHOLD 4
#define NOMINAL_SIZE 148
#define SPLITTER_CLICK_SLOP 4
#define SPLITTER_CLICK_TIMEOUT	400

static void caja_horizontal_splitter_class_init (CajaHorizontalSplitterClass *horizontal_splitter_class);
static void caja_horizontal_splitter_init       (CajaHorizontalSplitter      *horizontal_splitter);

EEL_CLASS_BOILERPLATE (CajaHorizontalSplitter,
                       caja_horizontal_splitter,
                       GTK_TYPE_HPANED)

static void
caja_horizontal_splitter_init (CajaHorizontalSplitter *horizontal_splitter)
{
    horizontal_splitter->details = g_new0 (CajaHorizontalSplitterDetails, 1);
}

static void
caja_horizontal_splitter_finalize (GObject *object)
{
    CajaHorizontalSplitter *horizontal_splitter;

    horizontal_splitter = CAJA_HORIZONTAL_SPLITTER (object);

    g_free (horizontal_splitter->details);

    EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
splitter_expand (CajaHorizontalSplitter *splitter, int position)
{
    g_assert (CAJA_IS_HORIZONTAL_SPLITTER (splitter));

    if (position >= CLOSED_THRESHOLD)
    {
        return;
    }

    position = splitter->details->saved_size;
    if (position < CLOSED_THRESHOLD)
    {
        position = NOMINAL_SIZE;
    }

    gtk_paned_set_position (GTK_PANED (splitter), position);
}

static void
splitter_collapse (CajaHorizontalSplitter *splitter, int position)
{
    g_assert (CAJA_IS_HORIZONTAL_SPLITTER (splitter));

    splitter->details->saved_size = position;
    gtk_paned_set_position (GTK_PANED (splitter), 0);
}

static void
splitter_toggle (CajaHorizontalSplitter *splitter, int position)
{
    g_assert (CAJA_IS_HORIZONTAL_SPLITTER (splitter));

    if (gtk_paned_get_position (GTK_PANED (splitter)) >= CLOSED_THRESHOLD)
    {
        caja_horizontal_splitter_collapse (splitter);
    }
    else
    {
        caja_horizontal_splitter_expand (splitter);
    }
}

static void
splitter_hide (CajaHorizontalSplitter *splitter)
{
    GtkPaned *parent;

    parent = GTK_PANED (splitter);

    gtk_widget_hide (gtk_paned_get_child1 (parent));
}

static void
splitter_show (CajaHorizontalSplitter *splitter)
{
    GtkPaned *parent;

    parent = GTK_PANED (splitter);

    gtk_widget_show (gtk_paned_get_child1 (parent));
}

static gboolean
splitter_is_hidden (CajaHorizontalSplitter *splitter)
{
    GtkPaned *parent;

    parent = GTK_PANED (splitter);

    return gtk_widget_get_visible (gtk_paned_get_child1 (parent));
}

void
caja_horizontal_splitter_expand (CajaHorizontalSplitter *splitter)
{
    splitter_expand (splitter, gtk_paned_get_position (GTK_PANED (splitter)));
}

void
caja_horizontal_splitter_hide (CajaHorizontalSplitter *splitter)
{
    splitter_hide (splitter);
}

void
caja_horizontal_splitter_show (CajaHorizontalSplitter *splitter)
{
    splitter_show (splitter);
}

gboolean
caja_horizontal_splitter_is_hidden (CajaHorizontalSplitter *splitter)
{
    return splitter_is_hidden (splitter);
}

void
caja_horizontal_splitter_collapse (CajaHorizontalSplitter *splitter)
{
    splitter_collapse (splitter, gtk_paned_get_position (GTK_PANED (splitter)));
}

/* routine to toggle the open/closed state of the splitter */
void
caja_horizontal_splitter_toggle_position (CajaHorizontalSplitter *splitter)
{
    splitter_toggle (splitter, gtk_paned_get_position (GTK_PANED (splitter)));
}

/* CajaHorizontalSplitter public methods */
GtkWidget *
caja_horizontal_splitter_new (void)
{
    return gtk_widget_new (caja_horizontal_splitter_get_type (), NULL);
}

/* handle mouse downs by remembering the position and the time */
static gboolean
caja_horizontal_splitter_button_press (GtkWidget *widget, GdkEventButton *event)
{
    gboolean result;
    CajaHorizontalSplitter *splitter;
    int position;

    splitter = CAJA_HORIZONTAL_SPLITTER (widget);

    position = gtk_paned_get_position (GTK_PANED (widget));

    result = EEL_CALL_PARENT_WITH_RETURN_VALUE
             (GTK_WIDGET_CLASS, button_press_event, (widget, event));

    if (result)
    {
        splitter->details->press_time = event->time;
        splitter->details->press_position = position;
    }

    return result;
}

/* handle mouse ups by seeing if it was a tap and toggling the open state accordingly */
static gboolean
caja_horizontal_splitter_button_release (GtkWidget *widget, GdkEventButton *event)
{
    gboolean result;
    CajaHorizontalSplitter *splitter;
    int position, delta, delta_time;
    splitter = CAJA_HORIZONTAL_SPLITTER (widget);

    position = gtk_paned_get_position (GTK_PANED (widget));

    result = EEL_CALL_PARENT_WITH_RETURN_VALUE
             (GTK_WIDGET_CLASS, button_release_event, (widget, event));

    if (result)
    {
        delta = abs (position - splitter->details->press_position);
        delta_time = event->time - splitter->details->press_time;
        if (delta < SPLITTER_CLICK_SLOP && delta_time < SPLITTER_CLICK_TIMEOUT)
        {
            caja_horizontal_splitter_toggle_position (splitter);
        }
    }

    return result;
}

static void
caja_horizontal_splitter_size_allocate (GtkWidget     *widget,
                                        GtkAllocation *allocation)
{
    gint border_width;
    GtkPaned *paned;
    GtkAllocation child_allocation;
    GtkRequisition child_requisition;

    paned = GTK_PANED (widget);
    border_width = gtk_container_get_border_width (GTK_CONTAINER (paned));

    gtk_widget_set_allocation (widget, allocation);

    if (gtk_paned_get_child2 (paned) != NULL && gtk_widget_get_visible (gtk_paned_get_child2 (paned)))
    {
        EEL_CALL_PARENT (GTK_WIDGET_CLASS, size_allocate,
                         (widget, allocation));
    }
    else if (gtk_paned_get_child1 (paned) && gtk_widget_get_visible (gtk_paned_get_child1 (paned)))
    {

        if (gtk_widget_get_realized (widget))
        {
            gdk_window_hide (gtk_paned_get_handle_window (paned));
        }

        gtk_widget_get_child_requisition (gtk_paned_get_child1 (paned),
                                          &child_requisition);

        child_allocation.x = allocation->x + border_width;
        child_allocation.y = allocation->y + border_width;
        child_allocation.width = MIN (child_requisition.width,
                                      allocation->width - 2 * border_width);
        child_allocation.height = MIN (child_requisition.height,
                                       allocation->height - 2 * border_width);

        gtk_widget_size_allocate (gtk_paned_get_child1 (paned), &child_allocation);
    }
    else if (gtk_widget_get_realized (widget))
    {
        gdk_window_hide (gtk_paned_get_handle_window (paned));
    }

}

static void
caja_horizontal_splitter_class_init (CajaHorizontalSplitterClass *class)
{
    GtkWidgetClass *widget_class;

    widget_class = GTK_WIDGET_CLASS (class);

    G_OBJECT_CLASS (class)->finalize = caja_horizontal_splitter_finalize;

    widget_class->size_allocate = caja_horizontal_splitter_size_allocate;
    widget_class->button_press_event = caja_horizontal_splitter_button_press;
    widget_class->button_release_event = caja_horizontal_splitter_button_release;
}

void
caja_horizontal_splitter_pack2 (CajaHorizontalSplitter *splitter,
                                GtkWidget                  *child2)
{
    GtkPaned *paned;

    g_return_if_fail (GTK_IS_WIDGET (child2));
    g_return_if_fail (CAJA_IS_HORIZONTAL_SPLITTER (splitter));

    paned = GTK_PANED (splitter);
    gtk_paned_pack2 (paned, child2, TRUE, FALSE);
}
