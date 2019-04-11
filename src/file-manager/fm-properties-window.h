/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-properties-window.h - interface for window that lets user modify
                            icon properties

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

   Authors: Darin Adler <darin@bentspoon.com>
*/

#ifndef FM_PROPERTIES_WINDOW_H
#define FM_PROPERTIES_WINDOW_H

#include <gtk/gtk.h>

#include <libcaja-private/caja-file.h>

typedef struct FMPropertiesWindow FMPropertiesWindow;

#define FM_TYPE_PROPERTIES_WINDOW fm_properties_window_get_type()
#define FM_PROPERTIES_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), FM_TYPE_PROPERTIES_WINDOW, FMPropertiesWindow))
#define FM_PROPERTIES_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), FM_TYPE_PROPERTIES_WINDOW, FMPropertiesWindowClass))
#define FM_IS_PROPERTIES_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FM_TYPE_PROPERTIES_WINDOW))
#define FM_IS_PROPERTIES_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), FM_TYPE_PROPERTIES_WINDOW))
#define FM_PROPERTIES_WINDOW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), FM_TYPE_PROPERTIES_WINDOW, FMPropertiesWindowClass))

typedef struct _FMPropertiesWindowPrivate FMPropertiesWindowPrivate;

struct FMPropertiesWindow
{
    GtkDialog window;
    FMPropertiesWindowPrivate *details;
};

struct FMPropertiesWindowClass
{
    GtkDialogClass parent_class;

    /* Keybinding signals */
    void (* close)    (FMPropertiesWindow *window);
};

typedef struct FMPropertiesWindowClass FMPropertiesWindowClass;

GType   fm_properties_window_get_type   (void);

void 	fm_properties_window_present 	(GList *files,
        GtkWidget *parent_widget);

#endif /* FM_PROPERTIES_WINDOW_H */
