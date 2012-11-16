/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* caja-program-choosing.c - functions for selecting and activating
 				 programs for opening/viewing particular files.

   Copyright (C) 2000 Eazel, Inc.

   The Mate Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Mate Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Mate Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
   Boston, MA 02110-1301, USA.

   Author: John Sullivan <sullivan@eazel.com>
*/

#include <config.h>
#include "caja-program-choosing.h"

#include "caja-mime-actions.h"
#include "caja-global-preferences.h"
#include "caja-icon-info.h"
#include "caja-recent.h"
#include "caja-desktop-icon-file.h"
#include <eel/eel-mate-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <stdlib.h>

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

/**
 * application_cannot_open_location
 *
 * Handle the case where an application has been selected to be launched,
 * and it cannot handle the current uri scheme.  This can happen
 * because the default application for a file type may not be able
 * to handle some kinds of locations.   We want to tell users that their
 * default application doesn't work here, rather than switching off to
 * a different one without them noticing.
 *
 * @application: The application that was to be launched.
 * @file: The file whose location was passed as a parameter to the application
 * @parent_window: A window to use as the parent for any error dialogs.
 *  */
static void
application_cannot_open_location (GAppInfo *application,
                                  CajaFile *file,
                                  const char *uri_scheme,
                                  GtkWindow *parent_window)
{
#ifdef NEW_MIME_COMPLETE
    GtkDialog *message_dialog;
    LaunchParameters *launch_parameters;
    char *prompt;
    char *message;
    char *file_name;
    int response;

    file_name = caja_file_get_display_name (file);

    if (caja_mime_has_any_applications_for_file (file))
    {
        if (application != NULL)
        {
            prompt = _("Open Failed, would you like to choose another application?");
            message = g_strdup_printf (_("\"%s\" cannot open \"%s\" because \"%s\" cannot access files at \"%s\" "
                                         "locations."),
                                       g_app_info_get_display_name (application), file_name,
                                       g_app_info_get_display_name (application), uri_scheme);
        }
        else
        {
            prompt = _("Open Failed, would you like to choose another action?");
            message = g_strdup_printf (_("The default action cannot open \"%s\" because it cannot access files at \"%s\" "
                                         "locations."),
                                       file_name, uri_scheme);
        }

        message_dialog = eel_show_yes_no_dialog (prompt,
                         message,
                         GTK_STOCK_OK,
                         GTK_STOCK_CANCEL,
                         parent_window);
        response = gtk_dialog_run (message_dialog);
        gtk_widget_destroy (GTK_WIDGET (message_dialog));

        if (response == GTK_RESPONSE_YES)
        {
            launch_parameters = launch_parameters_new (file, parent_window);
            caja_choose_application_for_file
            (file,
             parent_window,
             launch_application_callback,
             launch_parameters);

        }
        g_free (message);
    }
    else
    {
        if (application != NULL)
        {
            prompt = g_strdup_printf (_("\"%s\" cannot open \"%s\" because \"%s\" cannot access files at \"%s\" "
                                        "locations."), g_app_info_get_display_name (application), file_name,
                                      g_app_info_get_display_name (application), uri_scheme);
            message = _("No other applications are available to view this file. "
                        "If you copy this file onto your computer, you may be able to open "
                        "it.");
        }
        else
        {
            prompt = g_strdup_printf (_("The default action cannot open \"%s\" because it cannot access files at \"%s\" "
                                        "locations."), file_name, uri_scheme);
            message = _("No other actions are available to view this file. "
                        "If you copy this file onto your computer, you may be able to open "
                        "it.");
        }

        eel_show_info_dialog (prompt, message, parent_window);
        g_free (prompt);
    }

    g_free (file_name);
#endif
}

/**
 * caja_launch_application:
 *
 * Fork off a process to launch an application with a given file as a
 * parameter. Provide a parent window for error dialogs.
 *
 * @application: The application to be launched.
 * @uris: The files whose locations should be passed as a parameter to the application.
 * @parent_window: A window to use as the parent for any error dialogs.
 */
void
caja_launch_application (GAppInfo *application,
                         GList *files,
                         GtkWindow *parent_window)
{
    GList *uris, *l;

    uris = NULL;
    for (l = files; l != NULL; l = l->next)
    {
        uris = g_list_prepend (uris, caja_file_get_activation_uri (l->data));
    }
    uris = g_list_reverse (uris);
    caja_launch_application_by_uri (application, uris,
                                    parent_window);
    g_list_foreach(uris, (GFunc) g_free, NULL);
    g_list_free(uris);
}

void
caja_launch_application_by_uri (GAppInfo *application,
                                GList *uris,
                                GtkWindow *parent_window)
{
    char            *uri, *uri_scheme;
    GList           *locations, *l;
    GFile *location;
    CajaFile    *file;
    gboolean        result;
    GError *error;
    GdkAppLaunchContext *launch_context;
    CajaIconInfo *icon;
    int count, total;

    g_assert (uris != NULL);

    /* count the number of uris with local paths */
    count = 0;
    total = g_list_length (uris);
    locations = NULL;
    for (l = uris; l != NULL; l = l->next)
    {
        uri = l->data;

        location = g_file_new_for_uri (uri);
        if (g_file_is_native (location))
        {
            count++;
        }
        locations = g_list_prepend (locations, location);
    }
    locations = g_list_reverse (locations);

    launch_context = gdk_app_launch_context_new ();
    if (parent_window)
        gdk_app_launch_context_set_screen (launch_context,
                                           gtk_window_get_screen (parent_window));

    file = caja_file_get_by_uri (uris->data);
    icon = caja_file_get_icon (file, 48, 0);
    caja_file_unref (file);
    if (icon)
    {
        gdk_app_launch_context_set_icon_name (launch_context,
                                              caja_icon_info_get_used_name (icon));
        g_object_unref (icon);
    }

    error = NULL;

    if (count == total)
    {
        /* All files are local, so we can use g_app_info_launch () with
         * the file list we constructed before.
         */
        result = g_app_info_launch (application,
                                    locations,
                                    G_APP_LAUNCH_CONTEXT (launch_context),
                                    &error);
    }
    else
    {
        /* Some files are non local, better use g_app_info_launch_uris ().
         */
        result = g_app_info_launch_uris (application,
                                         uris,
                                         G_APP_LAUNCH_CONTEXT (launch_context),
                                         &error);
    }

    g_object_unref (launch_context);

    if (!result)
    {
        if (error->domain == G_IO_ERROR &&
                error->code == G_IO_ERROR_NOT_SUPPORTED)
        {
            uri_scheme = g_uri_parse_scheme (uris->data);
            application_cannot_open_location (application,
                                              file,
                                              uri_scheme,
                                              parent_window);
            g_free (uri_scheme);
        }
        else
        {
#ifdef NEW_MIME_COMPLETE
            caja_program_chooser_show_invalid_message
            (MATE_VFS_MIME_ACTION_TYPE_APPLICATION, file, parent_window);
#else
            g_warning ("Cannot open app: %s\n", error->message);
#endif
        }
        g_error_free (error);
    }
    else
    {
        for (l = uris; l != NULL; l = l->next)
        {
            file = caja_file_get_by_uri (l->data);
            caja_recent_add_file (file, application);
            caja_file_unref (file);
        }
    }

    g_list_foreach(locations, (GFunc) g_object_unref, NULL);
    g_list_free(locations);
}

/**
 * caja_launch_application_from_command:
 *
 * Fork off a process to launch an application with a given uri as
 * a parameter.
 *
 * @command_string: The application to be launched, with any desired
 * command-line options.
 * @...: Passed as parameters to the application after quoting each of them.
 */
void
caja_launch_application_from_command (GdkScreen  *screen,
                                      const char *name,
                                      const char *command_string,
                                      gboolean use_terminal,
                                      ...)
{
    char *full_command, *tmp;
    char *quoted_parameter;
    char *parameter;
    va_list ap;

    full_command = g_strdup (command_string);

    va_start (ap, use_terminal);

    while ((parameter = va_arg (ap, char *)) != NULL)
    {
        quoted_parameter = g_shell_quote (parameter);
        tmp = g_strconcat (full_command, " ", quoted_parameter, NULL);
        g_free (quoted_parameter);

        g_free (full_command);
        full_command = tmp;

    }

    va_end (ap);

    if (use_terminal)
    {
        eel_mate_open_terminal_on_screen (full_command, screen);
    }
    else
    {
        gdk_spawn_command_line_on_screen (screen, full_command, NULL);
    }

    g_free (full_command);
}

/**
 * caja_launch_application_from_command:
 *
 * Fork off a process to launch an application with a given uri as
 * a parameter.
 *
 * @command_string: The application to be launched, with any desired
 * command-line options.
 * @parameters: Passed as parameters to the application after quoting each of them.
 */
void
caja_launch_application_from_command_array (GdkScreen  *screen,
        const char *name,
        const char *command_string,
        gboolean use_terminal,
        const char * const * parameters)
{
    char *full_command, *tmp;
    char *quoted_parameter;
    const char * const *p;

    full_command = g_strdup (command_string);

    if (parameters != NULL)
    {
        for (p = parameters; *p != NULL; p++)
        {
            quoted_parameter = g_shell_quote (*p);
            tmp = g_strconcat (full_command, " ", quoted_parameter, NULL);
            g_free (quoted_parameter);

            g_free (full_command);
            full_command = tmp;
        }
    }

    if (use_terminal)
    {
        eel_mate_open_terminal_on_screen (full_command, screen);
    }
    else
    {
        gdk_spawn_command_line_on_screen (screen, full_command, NULL);
    }

    g_free (full_command);
}

void
caja_launch_desktop_file (GdkScreen   *screen,
                          const char  *desktop_file_uri,
                          const GList *parameter_uris,
                          GtkWindow   *parent_window)
{
    GError *error;
    char *message, *desktop_file_path;
    const GList *p;
    GList *files;
    int total, count;
    GFile *file, *desktop_file;
    GDesktopAppInfo *app_info;
    GdkAppLaunchContext *context;

    /* Don't allow command execution from remote locations
     * to partially mitigate the security
     * risk of executing arbitrary commands.
     */
    desktop_file = g_file_new_for_uri (desktop_file_uri);
    desktop_file_path = g_file_get_path (desktop_file);
    if (!g_file_is_native (desktop_file))
    {
        g_free (desktop_file_path);
        g_object_unref (desktop_file);
        eel_show_error_dialog
        (_("Sorry, but you cannot execute commands from "
           "a remote site."),
         _("This is disabled due to security considerations."),
         parent_window);

        return;
    }
    g_object_unref (desktop_file);

    app_info = g_desktop_app_info_new_from_filename (desktop_file_path);
    g_free (desktop_file_path);
    if (app_info == NULL)
    {
        eel_show_error_dialog
        (_("There was an error launching the application."),
         NULL,
         parent_window);
        return;
    }

    /* count the number of uris with local paths */
    count = 0;
    total = g_list_length ((GList *) parameter_uris);
    files = NULL;
    for (p = parameter_uris; p != NULL; p = p->next)
    {
        file = g_file_new_for_uri ((const char *) p->data);
        if (g_file_is_native (file))
        {
            count++;
        }
        files = g_list_prepend (files, file);
    }

    /* check if this app only supports local files */
    if (g_app_info_supports_files (G_APP_INFO (app_info)) &&
            !g_app_info_supports_uris (G_APP_INFO (app_info)) &&
            parameter_uris != NULL)
    {
        if (count == 0)
        {
            /* all files are non-local */
            eel_show_error_dialog
            (_("This drop target only supports local files."),
             _("To open non-local files copy them to a local folder and then"
               " drop them again."),
             parent_window);

            g_list_foreach(files, (GFunc) g_object_unref, NULL);
            g_list_free(files);
            g_object_unref (app_info);
            return;
        }
        else if (count != total)
        {
            /* some files are non-local */
            eel_show_warning_dialog
            (_("This drop target only supports local files."),
             _("To open non-local files copy them to a local folder and then"
               " drop them again. The local files you dropped have already been opened."),
             parent_window);
        }
    }

    error = NULL;
    context = gdk_app_launch_context_new ();
    /* TODO: Ideally we should accept a timestamp here instead of using GDK_CURRENT_TIME */
    gdk_app_launch_context_set_timestamp (context, GDK_CURRENT_TIME);
    gdk_app_launch_context_set_screen (context,
                                       gtk_window_get_screen (parent_window));
    if (count == total)
    {
        /* All files are local, so we can use g_app_info_launch () with
         * the file list we constructed before.
         */
        g_app_info_launch (G_APP_INFO (app_info),
                           files,
                           G_APP_LAUNCH_CONTEXT (context),
                           &error);
    }
    else
    {
        /* Some files are non local, better use g_app_info_launch_uris ().
         */
        g_app_info_launch_uris (G_APP_INFO (app_info),
                                (GList *) parameter_uris,
                                G_APP_LAUNCH_CONTEXT (context),
                                &error);
    }
    if (error != NULL)
    {
        message = g_strconcat (_("Details: "), error->message, NULL);
        eel_show_error_dialog
        (_("There was an error launching the application."),
         message,
         parent_window);

        g_error_free (error);
        g_free (message);
    }

    g_list_foreach(files, (GFunc) g_object_unref, NULL);
    g_list_free(files);
    g_object_unref (context);
    g_object_unref (app_info);
}
