/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-window-slot-info.c: Interface for caja window slots

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
#include "caja-window-slot-info.h"

enum
{
    ACTIVE,
    INACTIVE,
    LAST_SIGNAL
};

static guint caja_window_slot_info_signals[LAST_SIGNAL] = { 0 };

static void
caja_window_slot_info_base_init (gpointer g_class)
{
    static gboolean initialized = FALSE;

    if (!initialized)
    {
        caja_window_slot_info_signals[ACTIVE] =
            g_signal_new ("active",
                          CAJA_TYPE_WINDOW_SLOT_INFO,
                          G_SIGNAL_RUN_LAST,
                          G_STRUCT_OFFSET (CajaWindowSlotInfoIface, active),
                          NULL, NULL,
                          g_cclosure_marshal_VOID__VOID,
                          G_TYPE_NONE, 0);

        caja_window_slot_info_signals[INACTIVE] =
            g_signal_new ("inactive",
                          CAJA_TYPE_WINDOW_SLOT_INFO,
                          G_SIGNAL_RUN_LAST,
                          G_STRUCT_OFFSET (CajaWindowSlotInfoIface, inactive),
                          NULL, NULL,
                          g_cclosure_marshal_VOID__VOID,
                          G_TYPE_NONE, 0);

        initialized = TRUE;
    }
}

GType
caja_window_slot_info_get_type (void)
{
    static GType type = 0;

    if (!type)
    {
        const GTypeInfo info =
        {
            sizeof (CajaWindowSlotInfoIface),
            caja_window_slot_info_base_init,
            NULL,
            NULL,
            NULL,
            NULL,
            0,
            0,
            NULL
        };

        type = g_type_register_static (G_TYPE_INTERFACE,
                                       "CajaWindowSlotInfo",
                                       &info, 0);
        g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
    }

    return type;
}

void
caja_window_slot_info_set_status (CajaWindowSlotInfo *slot,
                                  const char             *status)
{
    g_assert (CAJA_IS_WINDOW_SLOT_INFO (slot));

    (* CAJA_WINDOW_SLOT_INFO_GET_IFACE (slot)->set_status) (slot,
            status);
}

void
caja_window_slot_info_make_hosting_pane_active (CajaWindowSlotInfo *slot)
{
    g_assert (CAJA_IS_WINDOW_SLOT_INFO (slot));
    (* CAJA_WINDOW_SLOT_INFO_GET_IFACE (slot)->make_hosting_pane_active) (slot);
}

void
caja_window_slot_info_open_location_full (CajaWindowSlotInfo  *slot,
                                     GFile                   *location,
                                     CajaWindowOpenMode       mode,
                                     CajaWindowOpenFlags      flags,
                                     GList                   *selection,
                                     CajaWindowGoToCallback   callback,
                                     gpointer user_data)
{
    g_assert (CAJA_IS_WINDOW_SLOT_INFO (slot));

    (* CAJA_WINDOW_SLOT_INFO_GET_IFACE (slot)->open_location) (slot,
            location,
            mode,
            flags,
            selection,
            callback,
            user_data);
}

char *
caja_window_slot_info_get_title (CajaWindowSlotInfo *slot)
{
    g_assert (CAJA_IS_WINDOW_SLOT_INFO (slot));

    return (* CAJA_WINDOW_SLOT_INFO_GET_IFACE (slot)->get_title) (slot);
}

char *
caja_window_slot_info_get_current_location (CajaWindowSlotInfo *slot)
{
    g_assert (CAJA_IS_WINDOW_SLOT_INFO (slot));

    return (* CAJA_WINDOW_SLOT_INFO_GET_IFACE (slot)->get_current_location) (slot);
}

CajaView *
caja_window_slot_info_get_current_view (CajaWindowSlotInfo *slot)
{
    g_assert (CAJA_IS_WINDOW_SLOT_INFO (slot));

    return (* CAJA_WINDOW_SLOT_INFO_GET_IFACE (slot)->get_current_view) (slot);
}

CajaWindowInfo *
caja_window_slot_info_get_window (CajaWindowSlotInfo *slot)
{
    g_assert (CAJA_IS_WINDOW_SLOT_INFO (slot));

    return (* CAJA_WINDOW_SLOT_INFO_GET_IFACE (slot)->get_window) (slot);
}

