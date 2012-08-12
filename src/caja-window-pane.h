/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-window-pane.h: Caja window pane

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

#ifndef CAJA_WINDOW_PANE_H
#define CAJA_WINDOW_PANE_H

#include "caja-window.h"

#define CAJA_TYPE_WINDOW_PANE	 (caja_window_pane_get_type())
#define CAJA_WINDOW_PANE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CAJA_TYPE_WINDOW_PANE, CajaWindowPaneClass))
#define CAJA_WINDOW_PANE(obj)	 (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_WINDOW_PANE, CajaWindowPane))
#define CAJA_IS_WINDOW_PANE(obj)      (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_WINDOW_PANE))
#define CAJA_IS_WINDOW_PANE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CAJA_TYPE_WINDOW_PANE))
#define CAJA_WINDOW_PANE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CAJA_TYPE_WINDOW_PANE, CajaWindowPaneClass))

typedef struct _CajaWindowPaneClass CajaWindowPaneClass;

struct _CajaWindowPaneClass
{
    GObjectClass parent_class;

    void (*show) (CajaWindowPane *pane);
    void (*set_active) (CajaWindowPane *pane,
                        gboolean is_active);
    void (*sync_search_widgets) (CajaWindowPane *pane);
    void (*sync_location_widgets) (CajaWindowPane *pane);
};

/* A CajaWindowPane is a layer between a slot and a window.
 * Each slot is contained in one pane, and each pane can contain
 * one or more slots. It also supports the notion of an "active slot".
 * On the other hand, each pane is contained in a window, while each
 * window can contain one or multiple panes. Likewise, the window has
 * the notion of an "active pane".
 *
 * A spatial window has only one pane, which contains a single slot.
 * A navigation window may have one or more panes.
 */
struct _CajaWindowPane
{
    GObject parent;

    /* hosting window */
    CajaWindow *window;
    gboolean visible;

    /* available slots, and active slot.
     * Both of them may never be NULL. */
    GList *slots;
    GList *active_slots;
    CajaWindowSlot *active_slot;

    /* whether or not this pane is active */
    gboolean is_active;
};

GType caja_window_pane_get_type (void);
CajaWindowPane *caja_window_pane_new (CajaWindow *window);


void caja_window_pane_show (CajaWindowPane *pane);
void caja_window_pane_zoom_in (CajaWindowPane *pane);
void caja_window_pane_zoom_to_level (CajaWindowPane *pane, CajaZoomLevel level);
void caja_window_pane_zoom_out (CajaWindowPane *pane);
void caja_window_pane_zoom_to_default (CajaWindowPane *pane);
void caja_window_pane_sync_location_widgets (CajaWindowPane *pane);
void caja_window_pane_sync_search_widgets  (CajaWindowPane *pane);
void caja_window_pane_set_active (CajaWindowPane *pane, gboolean is_active);
void caja_window_pane_slot_close (CajaWindowPane *pane, CajaWindowSlot *slot);

CajaWindowSlot* caja_window_pane_get_slot_for_content_box (CajaWindowPane *pane, GtkWidget *content_box);
void caja_window_pane_switch_to (CajaWindowPane *pane);
void caja_window_pane_grab_focus (CajaWindowPane *pane);


#endif /* CAJA_WINDOW_PANE_H */
