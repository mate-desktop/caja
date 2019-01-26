/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-clipboard-monitor.h: lets you notice clipboard changes.

   Copyright (C) 2004 Red Hat, Inc.

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

#ifndef CAJA_CLIPBOARD_MONITOR_H
#define CAJA_CLIPBOARD_MONITOR_H

#include <gtk/gtk.h>

#define CAJA_TYPE_CLIPBOARD_MONITOR caja_clipboard_monitor_get_type()
#define CAJA_CLIPBOARD_MONITOR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_CLIPBOARD_MONITOR, CajaClipboardMonitor))
#define CAJA_CLIPBOARD_MONITOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_CLIPBOARD_MONITOR, CajaClipboardMonitorClass))
#define CAJA_IS_CLIPBOARD_MONITOR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_CLIPBOARD_MONITOR))
#define CAJA_IS_CLIPBOARD_MONITOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), CAJA_TYPE_CLIPBOARD_MONITOR))
#define CAJA_CLIPBOARD_MONITOR_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), CAJA_TYPE_CLIPBOARD_MONITOR, CajaClipboardMonitorClass))

typedef struct _CajaClipboardMonitorPrivate CajaClipboardMonitorPrivate;
typedef struct CajaClipboardInfo CajaClipboardInfo;

typedef struct
{
    GObject parent_slot;

    CajaClipboardMonitorPrivate *details;
} CajaClipboardMonitor;

typedef struct
{
    GObjectClass parent_slot;

    void (* clipboard_changed) (CajaClipboardMonitor *monitor);
    void (* clipboard_info) (CajaClipboardMonitor *monitor,
                             CajaClipboardInfo *info);
} CajaClipboardMonitorClass;

struct CajaClipboardInfo
{
    GList *files;
    gboolean cut;
};

GType   caja_clipboard_monitor_get_type (void);

CajaClipboardMonitor *   caja_clipboard_monitor_get (void);
void caja_clipboard_monitor_set_clipboard_info (CajaClipboardMonitor *monitor,
        CajaClipboardInfo *info);
CajaClipboardInfo * caja_clipboard_monitor_get_clipboard_info (CajaClipboardMonitor *monitor);
void caja_clipboard_monitor_emit_changed (void);

void caja_clear_clipboard_callback (GtkClipboard *clipboard,
                                    gpointer      user_data);
void caja_get_clipboard_callback   (GtkClipboard     *clipboard,
                                    GtkSelectionData *selection_data,
                                    guint             info,
                                    gpointer          user_data);



#endif /* CAJA_CLIPBOARD_MONITOR_H */

