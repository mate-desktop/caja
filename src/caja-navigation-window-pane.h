/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-navigation-window-pane.h: Caja navigation window pane

   Copyright (C) 2008 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
   Boston, MA 02110-1301, USA.

   Author: Holger Berndt <berndth@gmx.de>
*/

#ifndef CAJA_NAVIGATION_WINDOW_PANE_H
#define CAJA_NAVIGATION_WINDOW_PANE_H

#include "caja-window-pane.h"
#include "caja-navigation-window-slot.h"

#define CAJA_TYPE_NAVIGATION_WINDOW_PANE     (caja_navigation_window_pane_get_type())
#define CAJA_NAVIGATION_WINDOW_PANE_CLASS(k) (G_TYPE_CHECK_CLASS_CAST((k), CAJA_TYPE_NAVIGATION_WINDOW_PANE, CajaNavigationWindowPaneClass))
#define CAJA_NAVIGATION_WINDOW_PANE(obj)     (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_NAVIGATION_WINDOW_PANE, CajaNavigationWindowPane))
#define CAJA_IS_NAVIGATION_WINDOW_PANE(obj)  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_NAVIGATION_WINDOW_PANE))
#define CAJA_IS_NAVIGATION_WINDOW_PANE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CAJA_TYPE_NAVIGATION_WINDOW_PANE))
#define CAJA_NAVIGATION_WINDOW_PANE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CAJA_TYPE_NAVIGATION_WINDOW_PANE, CajaNavigationWindowPaneClass))

typedef struct _CajaNavigationWindowPaneClass CajaNavigationWindowPaneClass;
typedef struct _CajaNavigationWindowPane      CajaNavigationWindowPane;

struct _CajaNavigationWindowPaneClass
{
    CajaWindowPaneClass parent_class;
};

struct _CajaNavigationWindowPane
{
    CajaWindowPane parent;

    GtkWidget *widget;

    /* location bar */
    GtkWidget *location_bar;
    GtkWidget *location_button;
    GtkWidget *navigation_bar;
    GtkWidget *path_bar;
    GtkWidget *search_bar;

    gboolean temporary_navigation_bar;
    gboolean temporary_location_bar;
    gboolean temporary_search_bar;

    /* notebook */
    GtkWidget *notebook;

    /* split view */
    GtkWidget *split_view_hpane;
};

GType    caja_navigation_window_pane_get_type (void);

CajaNavigationWindowPane* caja_navigation_window_pane_new (CajaWindow *window);

/* location bar */
void     caja_navigation_window_pane_setup             (CajaNavigationWindowPane *pane);

void     caja_navigation_window_pane_hide_location_bar (CajaNavigationWindowPane *pane, gboolean save_preference);
void     caja_navigation_window_pane_show_location_bar (CajaNavigationWindowPane *pane, gboolean save_preference);
gboolean caja_navigation_window_pane_location_bar_showing (CajaNavigationWindowPane *pane);
void     caja_navigation_window_pane_hide_path_bar (CajaNavigationWindowPane *pane);
void     caja_navigation_window_pane_show_path_bar (CajaNavigationWindowPane *pane);
gboolean caja_navigation_window_pane_path_bar_showing (CajaNavigationWindowPane *pane);
gboolean caja_navigation_window_pane_search_bar_showing (CajaNavigationWindowPane *pane);
void     caja_navigation_window_pane_set_bar_mode  (CajaNavigationWindowPane *pane, CajaBarMode mode);
void     caja_navigation_window_pane_show_location_bar_temporarily (CajaNavigationWindowPane *pane);
void     caja_navigation_window_pane_show_navigation_bar_temporarily (CajaNavigationWindowPane *pane);
void     caja_navigation_window_pane_always_use_location_entry (CajaNavigationWindowPane *pane, gboolean use_entry);
gboolean caja_navigation_window_pane_hide_temporary_bars (CajaNavigationWindowPane *pane);
/* notebook */
void     caja_navigation_window_pane_add_slot_in_tab (CajaNavigationWindowPane *pane, CajaWindowSlot *slot, CajaWindowOpenSlotFlags flags);
void     caja_navigation_window_pane_remove_page (CajaNavigationWindowPane *pane, int page_num);

#endif /* CAJA_NAVIGATION_WINDOW_PANE_H */
