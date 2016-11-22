/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
   caja-mime-application-chooser.c: Manages applications for mime types

   Copyright (C) 2004 Novell, Inc.

   The Mate Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Mate Library is distributed in the hope that it will be useful,
   but APPLICATIONOUT ANY WARRANTY; applicationout even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along application the Mate Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
   Boston, MA 02110-1301, USA.

   Authors: Dave Camp <dave@novell.com>
*/

#ifndef CAJA_MIME_APPLICATION_CHOOSER_H
#define CAJA_MIME_APPLICATION_CHOOSER_H

#include <gtk/gtk.h>

#define CAJA_TYPE_MIME_APPLICATION_CHOOSER         (caja_mime_application_chooser_get_type ())
#define CAJA_MIME_APPLICATION_CHOOSER(obj)         (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_MIME_APPLICATION_CHOOSER, CajaMimeApplicationChooser))
#define CAJA_MIME_APPLICATION_CHOOSER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_MIME_APPLICATION_CHOOSER, CajaMimeApplicationChooserClass))
#define CAJA_IS_MIME_APPLICATION_CHOOSER(obj)      (G_TYPE_INSTANCE_CHECK_TYPE ((obj), CAJA_TYPE_MIME_APPLICATION_CHOOSER)

typedef struct _CajaMimeApplicationChooser        CajaMimeApplicationChooser;
typedef struct _CajaMimeApplicationChooserClass   CajaMimeApplicationChooserClass;
typedef struct _CajaMimeApplicationChooserDetails CajaMimeApplicationChooserDetails;

struct _CajaMimeApplicationChooser
{
    GtkBox parent;
    CajaMimeApplicationChooserDetails *details;
};

struct _CajaMimeApplicationChooserClass
{
    GtkBoxClass parent_class;
};

GType      caja_mime_application_chooser_get_type (void);
GtkWidget* caja_mime_application_chooser_new      (const char *uri,
        const char *mime_type);
GtkWidget* caja_mime_application_chooser_new_for_multiple_files (GList *uris,
        const char *mime_type);

#endif /* CAJA_MIME_APPLICATION_CHOOSER_H */
