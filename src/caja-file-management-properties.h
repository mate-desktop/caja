/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* caja-file-management-properties.h - Function to show the caja preference dialog.

   Copyright (C) 2002 Jan Arne Petersen

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

   Authors: Jan Arne Petersen <jpetersen@uni-bonn.de>
*/

#ifndef CAJA_FILE_MANAGEMENT_PROPERTIES_H
#define CAJA_FILE_MANAGEMENT_PROPERTIES_H

#include <glib-object.h>
#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

    void caja_file_management_properties_dialog_show (GCallback close_callback, GtkWindow *window);

#ifdef __cplusplus
}
#endif

#endif /* CAJA_FILE_MANAGEMENT_PROPERTIES_H */
