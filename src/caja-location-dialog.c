/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Caja
 *
 * Copyright (C) 2003 Ximian, Inc.
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

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <eel/eel-gtk-macros.h>
#include <eel/eel-stock-dialogs.h>

#include <libcaja-private/caja-file-utilities.h>

#include "caja-location-dialog.h"
#include "caja-location-entry.h"
#include "caja-desktop-window.h"

struct _CajaLocationDialogDetails
{
    GtkWidget *entry;
    CajaWindow *window;
};

static void  caja_location_dialog_class_init       (CajaLocationDialogClass *class);
static void  caja_location_dialog_init             (CajaLocationDialog      *dialog);

EEL_CLASS_BOILERPLATE (CajaLocationDialog,
                       caja_location_dialog,
                       GTK_TYPE_DIALOG)
enum
{
    RESPONSE_OPEN
};

static void
caja_location_dialog_finalize (GObject *object)
{
    CajaLocationDialog *dialog;

    dialog = CAJA_LOCATION_DIALOG (object);

    g_free (dialog->details);

    EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
open_current_location (CajaLocationDialog *dialog)
{
    GFile *location;
    char *user_location;

    user_location = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->entry), 0, -1);
    location = g_file_parse_name (user_location);
    caja_window_go_to (dialog->details->window, location);
    g_object_unref (location);
    g_free (user_location);
}

static void
response_callback (CajaLocationDialog *dialog,
                   int response_id,
                   gpointer data)
{
    GError *error;

    switch (response_id)
    {
    case RESPONSE_OPEN :
        open_current_location (dialog);
        gtk_widget_destroy (GTK_WIDGET (dialog));
        break;
    case GTK_RESPONSE_NONE :
    case GTK_RESPONSE_DELETE_EVENT :
    case GTK_RESPONSE_CANCEL :
        gtk_widget_destroy (GTK_WIDGET (dialog));
        break;
    case GTK_RESPONSE_HELP :
        error = NULL;
        gtk_show_uri_on_window (GTK_WINDOW (dialog),
                                "help:mate-user-guide/caja-open-location",
                                gtk_get_current_event_time (), &error);
        if (error)
        {
            eel_show_error_dialog (_("There was an error displaying help."), error->message,
                                   GTK_WINDOW (dialog));
            g_error_free (error);
        }
        break;
    default :
        g_assert_not_reached ();
    }
}

static void
entry_activate_callback (GtkEntry *entry,
                         gpointer user_data)
{
    CajaLocationDialog *dialog;

    dialog = CAJA_LOCATION_DIALOG (user_data);

    if (gtk_entry_get_text_length (GTK_ENTRY (dialog->details->entry)) != 0)
    {
        gtk_dialog_response (GTK_DIALOG (dialog), RESPONSE_OPEN);
    }
}

static void
caja_location_dialog_class_init (CajaLocationDialogClass *class)
{
    G_OBJECT_CLASS (class)->finalize = caja_location_dialog_finalize;
}

static void
entry_text_changed (GObject *object, GParamSpec *spec, gpointer user_data)
{
    CajaLocationDialog *dialog;

    dialog = CAJA_LOCATION_DIALOG (user_data);

    if (gtk_entry_get_text_length (GTK_ENTRY (dialog->details->entry)) != 0)
    {
        gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), RESPONSE_OPEN, TRUE);
    }
    else
    {
        gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), RESPONSE_OPEN, FALSE);
    }
}

static void
caja_location_dialog_init (CajaLocationDialog *dialog)
{
    GtkWidget *box;
    GtkWidget *label;

    dialog->details = g_new0 (CajaLocationDialogDetails, 1);

    gtk_window_set_title (GTK_WINDOW (dialog), _("Open Location"));
    gtk_window_set_default_size (GTK_WINDOW (dialog), 300, -1);
    gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
    gtk_box_set_spacing (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), 2);

    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_container_set_border_width (GTK_CONTAINER (box), 5);
    gtk_widget_show (box);

    label = gtk_label_new_with_mnemonic (_("_Location:"));
    gtk_widget_show (label);
    gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

    dialog->details->entry = caja_location_entry_new ();
    gtk_entry_set_width_chars (GTK_ENTRY (dialog->details->entry), 30);
    g_signal_connect_after (dialog->details->entry,
                            "activate",
                            G_CALLBACK (entry_activate_callback),
                            dialog);

    gtk_widget_show (dialog->details->entry);

    gtk_label_set_mnemonic_widget (GTK_LABEL (label), dialog->details->entry);

    gtk_box_pack_start (GTK_BOX (box), dialog->details->entry,
                        TRUE, TRUE, 0);

    gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
                        box, FALSE, TRUE, 0);

    eel_dialog_add_button (GTK_DIALOG (dialog),
                           _("_Help"),
                           "help-browser",
                           GTK_RESPONSE_HELP);

    eel_dialog_add_button (GTK_DIALOG (dialog),
                           _("_Cancel"),
                           "process-stop",
                           GTK_RESPONSE_CANCEL);

    eel_dialog_add_button (GTK_DIALOG (dialog),
                           _("_Open"),
                           "document-open",
                           RESPONSE_OPEN);

    gtk_dialog_set_default_response (GTK_DIALOG (dialog),
                                     RESPONSE_OPEN);

    g_signal_connect (dialog->details->entry, "notify::text",
                      G_CALLBACK (entry_text_changed), dialog);

    g_signal_connect (dialog, "response",
                      G_CALLBACK (response_callback),
                      dialog);
}

GtkWidget *
caja_location_dialog_new (CajaWindow *window)
{
    CajaLocationDialog *loc_dialog;
    GtkWidget *dialog;
    GFile *location;

    dialog = gtk_widget_new (CAJA_TYPE_LOCATION_DIALOG, NULL);
    loc_dialog = CAJA_LOCATION_DIALOG (dialog);

    if (window)
    {
        gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window));
        gtk_window_set_screen (GTK_WINDOW (dialog),
                               gtk_window_get_screen (GTK_WINDOW (window)));
        loc_dialog->details->window = window;
        location = window->details->active_pane->active_slot->location;
    }
    else
        location = NULL;

    if (location != NULL)
    {
        char *formatted_location;

        if (CAJA_IS_DESKTOP_WINDOW (window))
        {
            formatted_location = g_strdup_printf ("%s/", g_get_home_dir ());
        }
        else
        {
            formatted_location = g_file_get_parse_name (location);
        }
        caja_location_entry_update_current_location (CAJA_LOCATION_ENTRY (loc_dialog->details->entry),
                formatted_location);
        g_free (formatted_location);
    }

    gtk_widget_grab_focus (loc_dialog->details->entry);

    return dialog;
}

void
caja_location_dialog_set_location (CajaLocationDialog *dialog,
                                   const char *location)
{
    caja_location_entry_update_current_location (CAJA_LOCATION_ENTRY (dialog->details->entry),
            location);
}
