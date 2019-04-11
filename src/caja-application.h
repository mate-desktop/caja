/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Caja
 *
 * Copyright (C) 2000 Red Hat, Inc.
 *
 * Caja is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Caja is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/* caja-application.h
 */

#ifndef CAJA_APPLICATION_H
#define CAJA_APPLICATION_H

#include <config.h>
#include <gdk/gdk.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include <libegg/eggsmclient.h>

#define CAJA_DESKTOP_ICON_VIEW_IID "OAFIID:Caja_File_Manager_Desktop_Icon_View"

#define CAJA_TYPE_APPLICATION \
	caja_application_get_type()
#define CAJA_APPLICATION(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), CAJA_TYPE_APPLICATION, CajaApplication))
#define CAJA_APPLICATION_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), CAJA_TYPE_APPLICATION, CajaApplicationClass))
#define CAJA_IS_APPLICATION(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), CAJA_TYPE_APPLICATION))
#define CAJA_IS_APPLICATION_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), CAJA_TYPE_APPLICATION))
#define CAJA_APPLICATION_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), CAJA_TYPE_APPLICATION, CajaApplicationClass))

#ifndef CAJA_WINDOW_DEFINED
#define CAJA_WINDOW_DEFINED
typedef struct CajaWindow CajaWindow;
#endif

#ifndef CAJA_SPATIAL_WINDOW_DEFINED
#define CAJA_SPATIAL_WINDOW_DEFINED
typedef struct _CajaSpatialWindow CajaSpatialWindow;
#endif

typedef struct _CajaApplicationPrivate CajaApplicationPrivate;

typedef struct
{
    GtkApplication parent;
    CajaApplicationPrivate *priv;

    EggSMClient* smclient;
    GVolumeMonitor* volume_monitor;
    unsigned int automount_idle_id;
    gboolean screensaver_active;
    guint ss_watch_id;
    GDBusProxy *ss_proxy;
    GList *volume_queue;
} CajaApplication;

typedef struct
{
	GtkApplicationClass parent_class;
} CajaApplicationClass;

GType caja_application_get_type (void);

CajaApplication *caja_application_new (void);

CajaWindow *     caja_application_get_spatial_window     (CajaApplication *application,
        CajaWindow      *requesting_window,
        const char      *startup_id,
        GFile           *location,
        GdkScreen       *screen,
        gboolean        *existing);

CajaWindow *     caja_application_create_navigation_window     (CajaApplication *application,
        GdkScreen           *screen);
void caja_application_close_all_navigation_windows (CajaApplication *self);
void caja_application_close_parent_windows     (CajaSpatialWindow *window);
void caja_application_close_all_spatial_windows  (void);

void caja_application_open_location (CajaApplication *application,
        GFile *location,
        GFile *selection,
        const char *startup_id,
        const gboolean open_in_tabs);

#endif /* CAJA_APPLICATION_H */
