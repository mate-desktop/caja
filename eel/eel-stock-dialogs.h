/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-stock-dialogs.h: Various standard dialogs for Eel.

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

   Authors: Darin Adler <darin@eazel.com>
*/

#ifndef EEL_STOCK_DIALOGS_H
#define EEL_STOCK_DIALOGS_H

#include <gtk/gtk.h>

typedef void (* EelCancelCallback) (gpointer callback_data);

/* Dialog for cancelling something that normally is fast enough not to need a dialog. */
void       eel_timed_wait_start               (EelCancelCallback  cancel_callback,
        gpointer           callback_data,
        const char        *wait_message,
        GtkWindow         *parent_window);
void       eel_timed_wait_start_with_duration (int                duration,
        EelCancelCallback  cancel_callback,
        gpointer           callback_data,
        const char        *wait_message,
        GtkWindow         *parent_window);
void       eel_timed_wait_stop                (EelCancelCallback  cancel_callback,
        gpointer           callback_data);

/* Basic dialog with buttons. */
int        eel_run_simple_dialog              (GtkWidget         *parent,
        gboolean           ignore_close_box,
        GtkMessageType     message_type,
        const char        *primary_text,
        const char        *secondary_text,
        ...);
GtkWidget* eel_dialog_add_button (GtkDialog   *dialog,
        const gchar *button_text,
        const gchar *icon_name,
              gint   response_id);
GtkWidget* eel_file_chooser_dialog_new (const gchar *title,
        GtkWindow            *parent,
        GtkFileChooserAction  action,
        const gchar          *first_button_text,
                              ...);

/* Variations on mate stock dialogs; these do line wrapping, we don't
 * bother with non-parented versions, we allow setting the title,
 * primary, and secondary messages, and we return GtkDialog pointers
 * instead of GtkWidget pointers.
 */
GtkDialog *eel_show_info_dialog               (const char        *primary_text,
        const char        *secondary_text,
        GtkWindow         *parent);
GtkDialog *eel_show_info_dialog_with_details  (const char        *primary_text,
        const char        *secondary_text,
        const char        *detailed_informative_message,
        GtkWindow         *parent);
GtkDialog *eel_show_warning_dialog            (const char        *primary_text,
        const char        *secondary_text,
        GtkWindow         *parent);
GtkDialog *eel_show_error_dialog              (const char        *primary_text,
        const char        *secondary_text,
        GtkWindow         *parent);
GtkDialog *eel_show_yes_no_dialog             (const char        *primary_text,
        const char        *secondary_text,
        const char        *yes_label,
        const char        *no_label,
        GtkWindow         *parent);
GtkDialog *eel_create_question_dialog         (const char        *primary_text,
        const char        *secondary_text,
        const char        *answer_one,
        int                response_one,
        const char        *answer_two,
        int                response_two,
        GtkWindow         *parent);

#endif /* EEL_STOCK_DIALOGS_H */
