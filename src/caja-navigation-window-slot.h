/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-navigation-window-slot.h: Caja navigation window slot

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

   Author: Christian Neumair <cneumair@gnome.org>
*/

#ifndef CAJA_NAVIGATION_WINDOW_SLOT_H
#define CAJA_NAVIGATION_WINDOW_SLOT_H

#include "caja-window-slot.h"

typedef struct CajaNavigationWindowSlot CajaNavigationWindowSlot;
typedef struct CajaNavigationWindowSlotClass CajaNavigationWindowSlotClass;


#define CAJA_TYPE_NAVIGATION_WINDOW_SLOT         (caja_navigation_window_slot_get_type())
#define CAJA_NAVIGATION_WINDOW_SLOT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CAJA_NAVIGATION_WINDOW_SLOT_CLASS, CajaNavigationWindowSlotClass))
#define CAJA_NAVIGATION_WINDOW_SLOT(obj)         (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_NAVIGATION_WINDOW_SLOT, CajaNavigationWindowSlot))
#define CAJA_IS_NAVIGATION_WINDOW_SLOT(obj)      (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_NAVIGATION_WINDOW_SLOT))
#define CAJA_IS_NAVIGATION_WINDOW_SLOT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CAJA_TYPE_NAVIGATION_WINDOW_SLOT))
#define CAJA_NAVIGATION_WINDOW_SLOT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CAJA_TYPE_NAVIGATION_WINDOW_SLOT, CajaNavigationWindowSlotClass))

typedef enum
{
    CAJA_BAR_PATH,
    CAJA_BAR_NAVIGATION,
    CAJA_BAR_SEARCH
} CajaBarMode;

struct CajaNavigationWindowSlot
{
    CajaWindowSlot parent;

    CajaBarMode bar_mode;
    GtkTreeModel *viewer_model;
    int num_viewers;

    /* Back/Forward chain, and history list.
     * The data in these lists are CajaBookmark pointers.
     */
    GList *back_list, *forward_list;

    /* Current views stuff */
    GList *sidebar_panels;
};

struct CajaNavigationWindowSlotClass
{
    CajaWindowSlotClass parent;
};

GType caja_navigation_window_slot_get_type (void);

gboolean caja_navigation_window_slot_should_close_with_mount (CajaNavigationWindowSlot *slot,
        GMount *mount);

void caja_navigation_window_slot_clear_forward_list (CajaNavigationWindowSlot *slot);
void caja_navigation_window_slot_clear_back_list    (CajaNavigationWindowSlot *slot);

#endif /* CAJA_NAVIGATION_WINDOW_SLOT_H */
