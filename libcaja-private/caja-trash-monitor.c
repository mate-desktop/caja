/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
   caja-trash-monitor.c: Caja trash state watcher.

   Copyright (C) 2000, 2001 Eazel, Inc.

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

   Author: Pavel Cisler <pavel@eazel.com>
*/

#include <config.h>

#include <gio/gio.h>
#include <string.h>

#include <eel/eel-debug.h>

#include "caja-trash-monitor.h"
#include "caja-directory-notify.h"
#include "caja-directory.h"
#include "caja-file-attributes.h"
#include "caja-icon-names.h"

struct _CajaTrashMonitorPrivate
{
    gboolean empty;
    GIcon *icon;
    GFileMonitor *file_monitor;
};

enum
{
    TRASH_STATE_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };
static CajaTrashMonitor *caja_trash_monitor = NULL;

G_DEFINE_TYPE_WITH_PRIVATE (CajaTrashMonitor, caja_trash_monitor, G_TYPE_OBJECT)

static void
caja_trash_monitor_finalize (GObject *object)
{
    CajaTrashMonitor *trash_monitor;

    trash_monitor = CAJA_TRASH_MONITOR (object);

    if (trash_monitor->details->icon)
    {
        g_object_unref (trash_monitor->details->icon);
    }
    if (trash_monitor->details->file_monitor)
    {
        g_object_unref (trash_monitor->details->file_monitor);
    }

    G_OBJECT_CLASS (caja_trash_monitor_parent_class)->finalize (object);
}

static void
caja_trash_monitor_class_init (CajaTrashMonitorClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = caja_trash_monitor_finalize;

    signals[TRASH_STATE_CHANGED] = g_signal_new
                                   ("trash_state_changed",
                                    G_TYPE_FROM_CLASS (object_class),
                                    G_SIGNAL_RUN_LAST,
                                    G_STRUCT_OFFSET (CajaTrashMonitorClass, trash_state_changed),
                                    NULL, NULL,
                                    g_cclosure_marshal_VOID__BOOLEAN,
                                    G_TYPE_NONE, 1,
                                    G_TYPE_BOOLEAN);
}

static void
update_info_cb (GObject *source_object,
                GAsyncResult *res,
                gpointer user_data)
{
    CajaTrashMonitor *trash_monitor;
    GFileInfo *info;
    gboolean empty;

    trash_monitor = CAJA_TRASH_MONITOR (user_data);

    info = g_file_query_info_finish (G_FILE (source_object),
                                     res, NULL);

    if (info != NULL)
    {
        GIcon *icon;

        icon = g_file_info_get_icon (info);

        if (icon)
        {
            g_object_unref (trash_monitor->details->icon);
            trash_monitor->details->icon = g_object_ref (icon);
            empty = TRUE;
            if (G_IS_THEMED_ICON (icon))
            {
                const char * const *names;
                int i;

                names = g_themed_icon_get_names (G_THEMED_ICON (icon));
                for (i = 0; names[i] != NULL; i++)
                {
                    if (strcmp (names[i], CAJA_ICON_TRASH_FULL) == 0)
                    {
                        empty = FALSE;
                        break;
                    }
                }
            }
            if (trash_monitor->details->empty != empty)
            {
                trash_monitor->details->empty = empty;

                /* trash got empty or full, notify everyone who cares */
                g_signal_emit (trash_monitor,
                               signals[TRASH_STATE_CHANGED], 0,
                               trash_monitor->details->empty);
            }
        }
        g_object_unref (info);
    }

    g_object_unref (trash_monitor);
}

static void
schedule_update_info (CajaTrashMonitor *trash_monitor)
{
    GFile *location;

    location = g_file_new_for_uri ("trash:///");

    g_file_query_info_async (location,
                             G_FILE_ATTRIBUTE_STANDARD_ICON,
                             0, 0, NULL,
                             update_info_cb, g_object_ref (trash_monitor));

    g_object_unref (location);
}

static void
file_changed (GFileMonitor* monitor,
              GFile *child,
              GFile *other_file,
              GFileMonitorEvent event_type,
              gpointer user_data)
{
    CajaTrashMonitor *trash_monitor;

    trash_monitor = CAJA_TRASH_MONITOR (user_data);

    schedule_update_info (trash_monitor);
}

static void
caja_trash_monitor_init (CajaTrashMonitor *trash_monitor)
{
    GFile *location;

    trash_monitor->details = caja_trash_monitor_get_instance_private (trash_monitor);

    trash_monitor->details->empty = TRUE;
    trash_monitor->details->icon = g_themed_icon_new (CAJA_ICON_TRASH);

    location = g_file_new_for_uri ("trash:///");

    trash_monitor->details->file_monitor = g_file_monitor_file (location, 0, NULL, NULL);

    g_signal_connect (trash_monitor->details->file_monitor, "changed",
                      (GCallback)file_changed, trash_monitor);

    g_object_unref (location);

    schedule_update_info (trash_monitor);
}

static void
unref_trash_monitor (void)
{
    g_object_unref (caja_trash_monitor);
}

CajaTrashMonitor *
caja_trash_monitor_get (void)
{
    if (caja_trash_monitor == NULL)
    {
        /* not running yet, start it up */

        caja_trash_monitor = CAJA_TRASH_MONITOR
                             (g_object_new (CAJA_TYPE_TRASH_MONITOR, NULL));
        eel_debug_call_at_shutdown (unref_trash_monitor);
    }

    return caja_trash_monitor;
}

gboolean
caja_trash_monitor_is_empty (void)
{
    CajaTrashMonitor *monitor;

    monitor = caja_trash_monitor_get ();
    return monitor->details->empty;
}

GIcon *
caja_trash_monitor_get_icon (void)
{
    CajaTrashMonitor *monitor;

    monitor = caja_trash_monitor_get ();
    if (monitor->details->icon)
    {
        return g_object_ref (monitor->details->icon);
    }
    return NULL;
}
