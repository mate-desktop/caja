/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* caja-dialog-notebook.c - GTKNotebook management for dialog windows

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

   Authors: Laurent Napias <tamplan@free.fr>
*/

#include <gtk/gtk.h>

gboolean
dialog_page_scroll_event_cb (GtkWidget *widget, GdkEventScroll *event, GtkWindow *window)
{
    GtkNotebook *notebook = GTK_NOTEBOOK (widget);
    GtkWidget *child, *event_widget, *action_widget;

    child = gtk_notebook_get_nth_page (notebook, gtk_notebook_get_current_page (notebook));
    if (child == NULL)
        return FALSE;

    event_widget = gtk_get_event_widget ((GdkEvent *) event);

    /* Ignore scroll events from the content of the page */
    if (event_widget == NULL ||
        event_widget == child ||
        gtk_widget_is_ancestor (event_widget, child))
        return FALSE;

    /* And also from the action widgets */
    action_widget = gtk_notebook_get_action_widget (notebook, GTK_PACK_START);
    if (event_widget == action_widget ||
        (action_widget != NULL && gtk_widget_is_ancestor (event_widget, action_widget)))
        return FALSE;
    action_widget = gtk_notebook_get_action_widget (notebook, GTK_PACK_END);
    if (event_widget == action_widget ||
        (action_widget != NULL && gtk_widget_is_ancestor (event_widget, action_widget)))
        return FALSE;

    switch (event->direction) {
    case GDK_SCROLL_RIGHT:
    case GDK_SCROLL_DOWN:
        gtk_notebook_next_page (notebook);
        break;
    case GDK_SCROLL_LEFT:
    case GDK_SCROLL_UP:
        gtk_notebook_prev_page (notebook);
        break;
    case GDK_SCROLL_SMOOTH:
        switch (gtk_notebook_get_tab_pos (notebook)) {
            case GTK_POS_LEFT:
            case GTK_POS_RIGHT:
                if (event->delta_y > 0)
                    gtk_notebook_next_page (notebook);
                else if (event->delta_y < 0)
                    gtk_notebook_prev_page (notebook);
                break;
            case GTK_POS_TOP:
            case GTK_POS_BOTTOM:
                if (event->delta_x > 0)
                    gtk_notebook_next_page (notebook);
                else if (event->delta_x < 0)
                    gtk_notebook_prev_page (notebook);
                break;
            }
        break;
    }

    return TRUE;
}
