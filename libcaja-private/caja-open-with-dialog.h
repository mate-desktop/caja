/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
   caja-open-with-dialog.c: an open-with dialog

   Copyright (C) 2004 Novell, Inc.

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

   Authors: Dave Camp <dave@novell.com>
*/

#ifndef CAJA_OPEN_WITH_DIALOG_H
#define CAJA_OPEN_WITH_DIALOG_H

#include <gtk/gtk.h>
#include <gio/gio.h>

#define CAJA_TYPE_OPEN_WITH_DIALOG         (caja_open_with_dialog_get_type ())
#define CAJA_OPEN_WITH_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_OPEN_WITH_DIALOG, CajaOpenWithDialog))
#define CAJA_OPEN_WITH_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_OPEN_WITH_DIALOG, CajaOpenWithDialogClass))
#define CAJA_IS_OPEN_WITH_DIALOG(obj)      (G_TYPE_INSTANCE_CHECK_TYPE ((obj), CAJA_TYPE_OPEN_WITH_DIALOG)

typedef struct _CajaOpenWithDialog        CajaOpenWithDialog;
typedef struct _CajaOpenWithDialogClass   CajaOpenWithDialogClass;
typedef struct _CajaOpenWithDialogDetails CajaOpenWithDialogDetails;

struct _CajaOpenWithDialog
{
    GtkDialog parent;
    CajaOpenWithDialogDetails *details;
};

struct _CajaOpenWithDialogClass
{
    GtkDialogClass parent_class;

    void (*application_selected) (CajaOpenWithDialog *dialog,
                                  GAppInfo *application);
};

GType      caja_open_with_dialog_get_type (void);
GtkWidget* caja_open_with_dialog_new      (const char *uri,
        const char *mime_type,
        const char *extension);
GtkWidget* caja_add_application_dialog_new (const char *uri,
        const char *mime_type);
GtkWidget* caja_add_application_dialog_new_for_multiple_files (const char *extension,
        const char *mime_type);



#endif /* CAJA_OPEN_WITH_DIALOG_H */
