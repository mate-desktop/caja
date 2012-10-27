/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Caja
 *
 * Copyright (C) 2003 Red Hat, Inc.
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

#ifndef CAJA_CONNECT_SERVER_DIALOG_H
#define CAJA_CONNECT_SERVER_DIALOG_H

#include <gio/gio.h>
#include <gtk/gtk.h>
#include "caja-window.h"

#define CAJA_TYPE_CONNECT_SERVER_DIALOG         (caja_connect_server_dialog_get_type ())
#define CAJA_CONNECT_SERVER_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_CONNECT_SERVER_DIALOG, CajaConnectServerDialog))
#define CAJA_CONNECT_SERVER_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_CONNECT_SERVER_DIALOG, CajaConnectServerDialogClass))
#define CAJA_IS_CONNECT_SERVER_DIALOG(obj)      (G_TYPE_INSTANCE_CHECK_TYPE ((obj), CAJA_TYPE_CONNECT_SERVER_DIALOG)

typedef struct _CajaConnectServerDialog        CajaConnectServerDialog;
typedef struct _CajaConnectServerDialogClass   CajaConnectServerDialogClass;
typedef struct _CajaConnectServerDialogDetails CajaConnectServerDialogDetails;

struct _CajaConnectServerDialog
{
    GtkDialog parent;
    CajaConnectServerDialogDetails *details;
};

struct _CajaConnectServerDialogClass
{
    GtkDialogClass parent_class;
};

GType      caja_connect_server_dialog_get_type (void);
GtkWidget* caja_connect_server_dialog_new      (CajaWindow *window);

/* Private internal calls */

void       caja_connect_server_dialog_present_uri (CajaApplication *application,
        GFile *location,
        GtkWidget *widget);

#endif /* CAJA_CONNECT_SERVER_DIALOG_H */
