/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-window-slot-info.h: Interface for caja window slots

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

#ifndef CAJA_WINDOW_SLOT_INFO_H
#define CAJA_WINDOW_SLOT_INFO_H

#include "caja-window-info.h"
#include "caja-view.h"


#define CAJA_TYPE_WINDOW_SLOT_INFO           (caja_window_slot_info_get_type ())
#define CAJA_WINDOW_SLOT_INFO(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_WINDOW_SLOT_INFO, CajaWindowSlotInfo))
#define CAJA_IS_WINDOW_SLOT_INFO(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_WINDOW_SLOT_INFO))
#define CAJA_WINDOW_SLOT_INFO_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), CAJA_TYPE_WINDOW_SLOT_INFO, CajaWindowSlotInfoIface))

typedef struct _CajaWindowSlotInfoIface CajaWindowSlotInfoIface;

struct _CajaWindowSlotInfoIface
{
    GTypeInterface g_iface;

    /* signals */

    /* emitted right after this slot becomes active.
     * Views should connect to this signal and merge their UI
     * into the main window.
     */
    void  (* active)  (CajaWindowSlotInfo *slot);
    /* emitted right before this slot becomes inactive.
     * Views should connect to this signal and unmerge their UI
     * from the main window.
     */
    void  (* inactive) (CajaWindowSlotInfo *slot);

    /* returns the window info associated with this slot */
    CajaWindowInfo * (* get_window) (CajaWindowSlotInfo *slot);

    /* Returns the number of selected items in the view */
    int  (* get_selection_count)  (CajaWindowSlotInfo    *slot);

    /* Returns a list of uris for th selected items in the view, caller frees it */
    GList *(* get_selection)      (CajaWindowSlotInfo    *slot);

    char * (* get_current_location)  (CajaWindowSlotInfo *slot);
    CajaView * (* get_current_view) (CajaWindowSlotInfo *slot);
    void   (* set_status)            (CajaWindowSlotInfo *slot,
                                      const char *status);
    char * (* get_title)             (CajaWindowSlotInfo *slot);

    void   (* open_location)      (CajaWindowSlotInfo *slot,
                                   GFile *location,
                                   CajaWindowOpenMode mode,
                                   CajaWindowOpenFlags flags,
                                   GList *selection,
                                   CajaWindowGoToCallback callback,
                                   gpointer user_data);
    void   (* make_hosting_pane_active) (CajaWindowSlotInfo *slot);
};


GType                             caja_window_slot_info_get_type            (void);
CajaWindowInfo *              caja_window_slot_info_get_window          (CajaWindowSlotInfo            *slot);
#define caja_window_slot_info_open_location(slot, location, mode, flags, selection) \
	caja_window_slot_info_open_location_full(slot, location, mode, \
						 flags, selection, NULL, NULL)

void                              caja_window_slot_info_open_location_full
	(CajaWindowSlotInfo *slot,
        GFile                             *location,
        CajaWindowOpenMode                 mode,
        CajaWindowOpenFlags                flags,
        GList                             *selection,
        CajaWindowGoToCallback		   callback,
        gpointer			   user_data);
void                              caja_window_slot_info_set_status          (CajaWindowSlotInfo            *slot,
        const char *status);
void                              caja_window_slot_info_make_hosting_pane_active (CajaWindowSlotInfo       *slot);

char *                            caja_window_slot_info_get_current_location (CajaWindowSlotInfo           *slot);
CajaView *                    caja_window_slot_info_get_current_view     (CajaWindowSlotInfo           *slot);
int                               caja_window_slot_info_get_selection_count  (CajaWindowSlotInfo           *slot);
GList *                           caja_window_slot_info_get_selection        (CajaWindowSlotInfo           *slot);
char *                            caja_window_slot_info_get_title            (CajaWindowSlotInfo           *slot);

#endif /* CAJA_WINDOW_SLOT_INFO_H */
