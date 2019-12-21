/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-gtk-extensions.h - interface for new functions that operate on
  			       gtk classes. Perhaps some of these should be
  			       rolled into gtk someday.

   Copyright (C) 1999, 2000, 2001 Eazel, Inc.

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

   Authors: John Sullivan <sullivan@eazel.com>
            Ramiro Estrugo <ramiro@eazel.com>
*/

#ifndef EEL_GTK_EXTENSIONS_H
#define EEL_GTK_EXTENSIONS_H

#include <gtk/gtk.h>
#include "eel-gdk-extensions.h"

/* GtkWindow */
void                  eel_gtk_window_set_initial_geometry             (GtkWindow            *window,
        EelGdkGeometryFlags   geometry_flags,
        int                   left,
        int                   top,
        guint                 width,
        guint                 height);
void                  eel_gtk_window_set_initial_geometry_from_string (GtkWindow            *window,
        const char           *geometry_string,
        guint                 minimum_width,
        guint                 minimum_height,
        gboolean		     ignore_position);
char *                eel_gtk_window_get_geometry_string              (GtkWindow            *window);


/* GtkMenu and GtkMenuItem */
void                  eel_pop_up_context_menu                         (GtkMenu              *menu,
        GdkEventButton       *event);
GtkMenuItem *         eel_gtk_menu_append_separator                   (GtkMenu              *menu);
GtkMenuItem *         eel_gtk_menu_insert_separator                   (GtkMenu              *menu,
        int                   index);

/* GtkMenuToolButton */
GtkWidget *           eel_gtk_menu_tool_button_get_button             (GtkMenuToolButton    *tool_button);

/* GtkLabel */
void                  eel_gtk_label_make_bold                         (GtkLabel             *label);

/* GtkTreeView */
void                  eel_gtk_tree_view_set_activate_on_single_click  (GtkTreeView          *tree_view,
                                                                       gboolean              should_activate);

/* GtkMessageDialog */
void                  eel_gtk_message_dialog_set_details_label        (GtkMessageDialog     *dialog,
                                                                       const gchar          *details_text);

GtkWidget *           eel_image_menu_item_new_from_icon               (const gchar          *icon_name,
                                                                       const gchar          *label_name);

GtkWidget *           eel_image_menu_item_new_from_surface              (cairo_surface_t    *icon_surface,
                                                                         const gchar        *label_name);

gboolean              eel_dialog_page_scroll_event_callback           (GtkWidget            *widget,
                                                                       GdkEventScroll       *event,
                                                                       GtkWindow            *window);
#endif /* EEL_GTK_EXTENSIONS_H */
