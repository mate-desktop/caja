/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
   caja-trash-monitor.h: Caja trash state watcher.

   Copyright (C) 2000 Eazel, Inc.

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

#ifndef CAJA_TRASH_MONITOR_H
#define CAJA_TRASH_MONITOR_H

#include <gtk/gtk.h>
#include <gio/gio.h>

typedef struct CajaTrashMonitor CajaTrashMonitor;
typedef struct CajaTrashMonitorClass CajaTrashMonitorClass;
typedef struct _CajaTrashMonitorPrivate CajaTrashMonitorPrivate;

#define CAJA_TYPE_TRASH_MONITOR caja_trash_monitor_get_type()
#define CAJA_TRASH_MONITOR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_TRASH_MONITOR, CajaTrashMonitor))
#define CAJA_TRASH_MONITOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_TRASH_MONITOR, CajaTrashMonitorClass))
#define CAJA_IS_TRASH_MONITOR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_TRASH_MONITOR))
#define CAJA_IS_TRASH_MONITOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), CAJA_TYPE_TRASH_MONITOR))
#define CAJA_TRASH_MONITOR_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), CAJA_TYPE_TRASH_MONITOR, CajaTrashMonitorClass))

struct CajaTrashMonitor
{
    GObject object;
    CajaTrashMonitorPrivate *details;
};

struct CajaTrashMonitorClass
{
    GObjectClass parent_class;

    void (* trash_state_changed)		(CajaTrashMonitor 	*trash_monitor,
                                         gboolean 		 new_state);
};

GType			caja_trash_monitor_get_type				(void);

CajaTrashMonitor   *caja_trash_monitor_get 				(void);
gboolean		caja_trash_monitor_is_empty 			(void);
GIcon                  *caja_trash_monitor_get_icon                         (void);

#endif
