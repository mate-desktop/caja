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

#ifndef CAJA_LOCATION_DIALOG_H
#define CAJA_LOCATION_DIALOG_H

#include <gtk/gtk.h>
#include "caja-window.h"

#define CAJA_TYPE_LOCATION_DIALOG         (caja_location_dialog_get_type ())
#define CAJA_LOCATION_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_LOCATION_DIALOG, CajaLocationDialog))
#define CAJA_LOCATION_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_LOCATION_DIALOG, CajaLocationDialogClass))
#define CAJA_IS_LOCATION_DIALOG(obj)      (G_TYPE_INSTANCE_CHECK_TYPE ((obj), CAJA_TYPE_LOCATION_DIALOG)

typedef struct _CajaLocationDialog        CajaLocationDialog;
typedef struct _CajaLocationDialogClass   CajaLocationDialogClass;
typedef struct _CajaLocationDialogDetails CajaLocationDialogDetails;

struct _CajaLocationDialog
{
    GtkDialog parent;
    CajaLocationDialogDetails *details;
};

struct _CajaLocationDialogClass
{
    GtkDialogClass parent_class;
};

GType      caja_location_dialog_get_type     (void);
GtkWidget* caja_location_dialog_new          (CajaWindow         *window);
void       caja_location_dialog_set_location (CajaLocationDialog *dialog,
        const char             *location);

#endif /* CAJA_LOCATION_DIALOG_H */
