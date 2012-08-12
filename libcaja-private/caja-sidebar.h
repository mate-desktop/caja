/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-sidebar.h: Interface for caja sidebar plugins

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

#ifndef CAJA_SIDEBAR_H
#define CAJA_SIDEBAR_H

#include <glib-object.h>
#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CAJA_TYPE_SIDEBAR           (caja_sidebar_get_type ())
#define CAJA_SIDEBAR(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_SIDEBAR, CajaSidebar))
#define CAJA_IS_SIDEBAR(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_SIDEBAR))
#define CAJA_SIDEBAR_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), CAJA_TYPE_SIDEBAR, CajaSidebarIface))

    typedef struct _CajaSidebar CajaSidebar; /* dummy typedef */
    typedef struct _CajaSidebarIface CajaSidebarIface;

    /* Must also be a GtkWidget */
    struct _CajaSidebarIface
    {
        GTypeInterface g_iface;

        /* Signals: */
        void           (* tab_icon_changed)       (CajaSidebar *sidebar);

        /* VTable: */
        const char *   (* get_sidebar_id)         (CajaSidebar *sidebar);
        char *         (* get_tab_label)          (CajaSidebar *sidebar);
        char *         (* get_tab_tooltip)        (CajaSidebar *sidebar);
        GdkPixbuf *    (* get_tab_icon)           (CajaSidebar *sidebar);
        void           (* is_visible_changed)     (CajaSidebar *sidebar,
                gboolean         is_visible);


        /* Padding for future expansion */
        void (*_reserved1) (void);
        void (*_reserved2) (void);
        void (*_reserved3) (void);
        void (*_reserved4) (void);
        void (*_reserved5) (void);
        void (*_reserved6) (void);
        void (*_reserved7) (void);
        void (*_reserved8) (void);
    };

    GType             caja_sidebar_get_type             (void);

    const char *caja_sidebar_get_sidebar_id     (CajaSidebar *sidebar);
    char *      caja_sidebar_get_tab_label      (CajaSidebar *sidebar);
    char *      caja_sidebar_get_tab_tooltip    (CajaSidebar *sidebar);
    GdkPixbuf * caja_sidebar_get_tab_icon       (CajaSidebar *sidebar);
    void        caja_sidebar_is_visible_changed (CajaSidebar *sidebar,
            gboolean         is_visible);

#ifdef __cplusplus
}
#endif

#endif /* CAJA_VIEW_H */
