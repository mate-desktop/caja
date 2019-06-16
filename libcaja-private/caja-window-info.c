/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-window-info.c: Interface for caja window

   Copyright (C) 2004 Red Hat Inc.

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

   Author: Alexander Larsson <alexl@redhat.com>
*/

#include <config.h>
#include "caja-window-info.h"

enum
{
    LOADING_URI,
    SELECTION_CHANGED,
    TITLE_CHANGED,
    HIDDEN_FILES_MODE_CHANGED,
    BACKUP_FILES_MODE_CHANGED,
    LAST_SIGNAL
};

static guint caja_window_info_signals[LAST_SIGNAL] = { 0 };

static void
caja_window_info_base_init (gpointer g_class)
{
    static gboolean initialized = FALSE;

    if (! initialized)
    {
        caja_window_info_signals[LOADING_URI] =
            g_signal_new ("loading_uri",
                          CAJA_TYPE_WINDOW_INFO,
                          G_SIGNAL_RUN_LAST,
                          G_STRUCT_OFFSET (CajaWindowInfoIface, loading_uri),
                          NULL, NULL,
                          g_cclosure_marshal_VOID__STRING,
                          G_TYPE_NONE, 1,
                          G_TYPE_STRING);

        caja_window_info_signals[SELECTION_CHANGED] =
            g_signal_new ("selection_changed",
                          CAJA_TYPE_WINDOW_INFO,
                          G_SIGNAL_RUN_LAST,
                          G_STRUCT_OFFSET (CajaWindowInfoIface, selection_changed),
                          NULL, NULL,
                          g_cclosure_marshal_VOID__VOID,
                          G_TYPE_NONE, 0);

        caja_window_info_signals[TITLE_CHANGED] =
            g_signal_new ("title_changed",
                          CAJA_TYPE_WINDOW_INFO,
                          G_SIGNAL_RUN_LAST,
                          G_STRUCT_OFFSET (CajaWindowInfoIface, title_changed),
                          NULL, NULL,
                          g_cclosure_marshal_VOID__STRING,
                          G_TYPE_NONE, 1,
                          G_TYPE_STRING);

        caja_window_info_signals[HIDDEN_FILES_MODE_CHANGED] =
            g_signal_new ("hidden_files_mode_changed",
                          CAJA_TYPE_WINDOW_INFO,
                          G_SIGNAL_RUN_LAST,
                          G_STRUCT_OFFSET (CajaWindowInfoIface, hidden_files_mode_changed),
                          NULL, NULL,
                          g_cclosure_marshal_VOID__VOID,
                          G_TYPE_NONE, 0);

        caja_window_info_signals[BACKUP_FILES_MODE_CHANGED] =
            g_signal_new ("backup_files_mode_changed",
                          CAJA_TYPE_WINDOW_INFO,
                          G_SIGNAL_RUN_LAST,
                          G_STRUCT_OFFSET (CajaWindowInfoIface, backup_files_mode_changed),
                          NULL, NULL,
                          g_cclosure_marshal_VOID__VOID,
                          G_TYPE_NONE, 0);

        initialized = TRUE;
    }
}

GType
caja_window_info_get_type (void)
{
    static GType type = 0;

    if (!type)
    {
        const GTypeInfo info =
        {
            sizeof (CajaWindowInfoIface),
            caja_window_info_base_init,
            NULL,
            NULL,
            NULL,
            NULL,
            0,
            0,
            NULL
        };

        type = g_type_register_static (G_TYPE_INTERFACE,
                                       "CajaWindowInfo",
                                       &info, 0);
        g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
    }

    return type;
}

void
caja_window_info_report_load_underway (CajaWindowInfo      *window,
                                       CajaView            *view)
{
    g_return_if_fail (CAJA_IS_WINDOW_INFO (window));
    g_return_if_fail (CAJA_IS_VIEW (view));

    (* CAJA_WINDOW_INFO_GET_IFACE (window)->report_load_underway) (window,
            view);
}

void
caja_window_info_report_load_complete (CajaWindowInfo      *window,
                                       CajaView            *view)
{
    g_return_if_fail (CAJA_IS_WINDOW_INFO (window));
    g_return_if_fail (CAJA_IS_VIEW (view));

    (* CAJA_WINDOW_INFO_GET_IFACE (window)->report_load_complete) (window,
            view);
}

void
caja_window_info_report_view_failed (CajaWindowInfo      *window,
                                     CajaView            *view)
{
    g_return_if_fail (CAJA_IS_WINDOW_INFO (window));
    g_return_if_fail (CAJA_IS_VIEW (view));

    (* CAJA_WINDOW_INFO_GET_IFACE (window)->report_view_failed) (window,
            view);
}

void
caja_window_info_report_selection_changed (CajaWindowInfo      *window)
{
    g_return_if_fail (CAJA_IS_WINDOW_INFO (window));

    (* CAJA_WINDOW_INFO_GET_IFACE (window)->report_selection_changed) (window);
}

void
caja_window_info_view_visible (CajaWindowInfo      *window,
                               CajaView            *view)
{
    g_return_if_fail (CAJA_IS_WINDOW_INFO (window));

    (* CAJA_WINDOW_INFO_GET_IFACE (window)->view_visible) (window, view);
}

void
caja_window_info_close (CajaWindowInfo      *window)
{
    g_return_if_fail (CAJA_IS_WINDOW_INFO (window));

    (* CAJA_WINDOW_INFO_GET_IFACE (window)->close_window) (window);
}

void
caja_window_info_push_status (CajaWindowInfo      *window,
                              const char              *status)
{
    g_return_if_fail (CAJA_IS_WINDOW_INFO (window));

    (* CAJA_WINDOW_INFO_GET_IFACE (window)->push_status) (window,
            status);
}

CajaWindowType
caja_window_info_get_window_type (CajaWindowInfo *window)
{
    g_return_val_if_fail (CAJA_IS_WINDOW_INFO (window), CAJA_WINDOW_SPATIAL);

    return (* CAJA_WINDOW_INFO_GET_IFACE (window)->get_window_type) (window);
}

char *
caja_window_info_get_title (CajaWindowInfo *window)
{
    g_return_val_if_fail (CAJA_IS_WINDOW_INFO (window), NULL);

    return (* CAJA_WINDOW_INFO_GET_IFACE (window)->get_title) (window);
}

GList *
caja_window_info_get_history (CajaWindowInfo *window)
{
    g_return_val_if_fail (CAJA_IS_WINDOW_INFO (window), NULL);

    return (* CAJA_WINDOW_INFO_GET_IFACE (window)->get_history) (window);
}

char *
caja_window_info_get_current_location (CajaWindowInfo *window)
{
    g_return_val_if_fail (CAJA_IS_WINDOW_INFO (window), NULL);

    return (* CAJA_WINDOW_INFO_GET_IFACE (window)->get_current_location) (window);
}

int
caja_window_info_get_selection_count (CajaWindowInfo *window)
{
    g_return_val_if_fail (CAJA_IS_WINDOW_INFO (window), 0);

    return (* CAJA_WINDOW_INFO_GET_IFACE (window)->get_selection_count) (window);
}

GList *
caja_window_info_get_selection (CajaWindowInfo *window)
{
    g_return_val_if_fail (CAJA_IS_WINDOW_INFO (window), NULL);

    return (* CAJA_WINDOW_INFO_GET_IFACE (window)->get_selection) (window);
}

CajaWindowShowHiddenFilesMode
caja_window_info_get_hidden_files_mode (CajaWindowInfo *window)
{
    g_return_val_if_fail (CAJA_IS_WINDOW_INFO (window), CAJA_WINDOW_SHOW_HIDDEN_FILES_DEFAULT);

    return (* CAJA_WINDOW_INFO_GET_IFACE (window)->get_hidden_files_mode) (window);
}

void
caja_window_info_set_hidden_files_mode (CajaWindowInfo *window,
                                        CajaWindowShowHiddenFilesMode  mode)
{
    g_return_if_fail (CAJA_IS_WINDOW_INFO (window));

    (* CAJA_WINDOW_INFO_GET_IFACE (window)->set_hidden_files_mode) (window,
            mode);
}

CajaWindowShowBackupFilesMode
caja_window_info_get_backup_files_mode (CajaWindowInfo *window)
{
    g_return_val_if_fail (CAJA_IS_WINDOW_INFO (window), CAJA_WINDOW_SHOW_BACKUP_FILES_DEFAULT);

    return (* CAJA_WINDOW_INFO_GET_IFACE (window)->get_backup_files_mode) (window);
}

void
caja_window_info_set_backup_files_mode (CajaWindowInfo *window,
                                        CajaWindowShowBackupFilesMode  mode)
{
    g_return_if_fail (CAJA_IS_WINDOW_INFO (window));

    (* CAJA_WINDOW_INFO_GET_IFACE (window)->set_backup_files_mode) (window,
            mode);
}

GtkUIManager *
caja_window_info_get_ui_manager (CajaWindowInfo *window)
{
    g_return_val_if_fail (CAJA_IS_WINDOW_INFO (window), NULL);

    return (* CAJA_WINDOW_INFO_GET_IFACE (window)->get_ui_manager) (window);
}

CajaWindowSlotInfo *
caja_window_info_get_active_slot (CajaWindowInfo *window)
{
    g_return_val_if_fail (CAJA_IS_WINDOW_INFO (window), NULL);

    return (* CAJA_WINDOW_INFO_GET_IFACE (window)->get_active_slot) (window);
}

CajaWindowSlotInfo *
caja_window_info_get_extra_slot (CajaWindowInfo *window)
{
    g_return_val_if_fail (CAJA_IS_WINDOW_INFO (window), NULL);

    return (* CAJA_WINDOW_INFO_GET_IFACE (window)->get_extra_slot) (window);
}

gboolean
caja_window_info_get_initiated_unmount (CajaWindowInfo *window)
{
    g_return_val_if_fail (CAJA_IS_WINDOW_INFO (window), FALSE);

    return (* CAJA_WINDOW_INFO_GET_IFACE (window)->get_initiated_unmount) (window);
}

void
caja_window_info_set_initiated_unmount (CajaWindowInfo *window, gboolean initiated_unmount)
{
    g_return_if_fail (CAJA_IS_WINDOW_INFO (window));

    (* CAJA_WINDOW_INFO_GET_IFACE (window)->set_initiated_unmount) (window,
            initiated_unmount);

}
