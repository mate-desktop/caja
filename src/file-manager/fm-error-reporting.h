/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-error-reporting.h - interface for file manager functions that report
 			  errors to the user.

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

   Authors: John Sullivan <sullivan@eazel.com>
*/

#ifndef FM_ERROR_REPORTING_H
#define FM_ERROR_REPORTING_H

#include <gtk/gtk.h>

#include <libcaja-private/caja-file.h>

void fm_report_error_loading_directory	 (CajaFile   *file,
        GError         *error,
        GtkWindow	 *parent_window);
GtkDialog *fm_report_error_renaming_file (CajaFile   *file,
        const char     *new_name,
        GError         *error,
        GtkWindow	 *parent_window);
void fm_report_error_setting_permissions (CajaFile   *file,
        GError         *error,
        GtkWindow	 *parent_window);
void fm_report_error_setting_owner       (CajaFile   *file,
        GError         *error,
        GtkWindow	 *parent_window);
void fm_report_error_setting_group       (CajaFile   *file,
        GError         *error,
        GtkWindow	 *parent_window);

/* Caller-owned context, valid until the completion callback. The returned
 * error dialog is borrowed and remains valid until destroyed. */
typedef struct {
    GtkWindow *parent_window;
    GtkDialog *error_dialog;
    gboolean wait_for_activation;
} FMRenameContext;
void fm_rename_file_with_context (CajaFile *file,
                                  const char *new_name,
                                  CajaFileOperationCallback callback,
                                  gpointer callback_data,
                                  FMRenameContext *context);

/* FIXME bugzilla.gnome.org 42394: Should this file be renamed or should this function be moved? */
void fm_rename_file                      (CajaFile   *file,
        const char     *new_name,
        CajaFileOperationCallback callback,
        gpointer callback_data);

#endif /* FM_ERROR_REPORTING_H */
