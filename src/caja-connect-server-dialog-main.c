/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* caja-connect-server-main.c - Start the "Connect to Server" dialog.
 * Caja
 *
 * Copyright (C) 2005 Vincent Untz
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
 *
 * Authors:
 *   Vincent Untz <vincent@vuntz.net>
 *   Cosimo Cecchi <cosimoc@gnome.org>
 */

#include <config.h>
#include <stdlib.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <eel/eel-stock-dialogs.h>

#include <libcaja-private/caja-icon-names.h>
#include <libcaja-private/caja-global-preferences.h>

#include "caja-connect-server-dialog.h"

static GSimpleAsyncResult *display_location_res = NULL;

static void
main_dialog_destroyed (GtkWidget *widget,
                       gpointer   user_data)
{
    /* this only happens when user clicks "cancel"
     * on the main dialog or when we are all done.
     */
    gtk_main_quit ();
}

gboolean
caja_connect_server_dialog_display_location_finish (CajaConnectServerDialog *self,
						    GAsyncResult *res,
						    GError **error)
{
    if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error)) {
    	return FALSE;
    }

    return TRUE;
}

void
caja_connect_server_dialog_display_location_async (CajaConnectServerDialog *self,
						   CajaApplication *application,
						   GFile *location,
						   GAsyncReadyCallback callback,
						   gpointer user_data)
{
    GError *error;
    GdkAppLaunchContext *launch_context;
    gchar *uri;

    display_location_res = g_simple_async_result_new (G_OBJECT (self),
    			    callback, user_data,
    			    caja_connect_server_dialog_display_location_async);

    error = NULL;
    uri = g_file_get_uri (location);

    launch_context = gdk_display_get_app_launch_context (gtk_widget_get_display (GTK_WIDGET (self)));

    gdk_app_launch_context_set_screen (launch_context,
                                       gtk_widget_get_screen (GTK_WIDGET (self)));

    g_app_info_launch_default_for_uri (uri,
                                       G_APP_LAUNCH_CONTEXT (launch_context),
                                       &error);

    g_object_unref (launch_context);

    if (error != NULL) {
    	g_simple_async_result_set_from_error (display_location_res, error);
        g_error_free (error);
    }
    g_simple_async_result_complete_in_idle (display_location_res);

    g_object_unref (display_location_res);
    display_location_res = NULL;
}

int
main (int argc, char *argv[])
{
    GtkWidget *dialog;
    GOptionContext *context;
    GError *error;

    bindtextdomain (GETTEXT_PACKAGE, MATELOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);

    error = NULL;
    /* Translators: This is the --help description for the connect to server app,
       the initial newlines are between the command line arg and the description */
    context = g_option_context_new (N_("\n\nAdd connect to server mount"));
    g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
    g_option_context_add_group (context, gtk_get_option_group (TRUE));

    if (!g_option_context_parse (context, &argc, &argv, &error))
    {
        g_critical ("Failed to parse arguments: %s", error->message);
        g_error_free (error);
        g_option_context_free (context);
        exit (1);
    }

    g_option_context_free (context);

    caja_global_preferences_init ();

    gtk_window_set_default_icon_name (CAJA_ICON_FOLDER);

    dialog = caja_connect_server_dialog_new (NULL);

    g_signal_connect (dialog, "destroy",
                      G_CALLBACK (main_dialog_destroyed), NULL);

    gtk_widget_show (dialog);

    gtk_main ();

    return 0;
}
