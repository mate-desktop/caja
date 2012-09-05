/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Caja
 *
 * Copyright (C) 2005 Red Hat, Inc.
 *
 * Caja is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Caja is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <config.h>
#include <gio/gio.h>
#include "caja-connect-server-dialog.h"
#include <libcaja-private/caja-global-preferences.h>

/* This file contains the glue for the calls from the connect to server dialog
 * to the main caja binary. A different version of this glue is in
 * caja-connect-server-dialog-main.c for the standalone version.
 */

void
caja_connect_server_dialog_present_uri (CajaApplication *application,
                                        GFile *location,
                                        GtkWidget *widget)
{
    CajaWindow *window;

    if (g_settings_get_boolean (caja_preferences, CAJA_PREFERENCES_ALWAYS_USE_BROWSER))
    {
        window = caja_application_create_navigation_window (application,
                 NULL,
                 gtk_widget_get_screen (widget));
        caja_window_go_to (window, location);
    }
    else
    {
        caja_application_present_spatial_window (application,
                NULL,
                NULL,
                location,
                gtk_widget_get_screen (widget));
    }

    gtk_widget_destroy (widget);
    g_object_unref (location);
}
