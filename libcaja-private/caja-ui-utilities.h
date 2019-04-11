/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* caja-ui-utilities.h - helper functions for GtkUIManager stuff

   Copyright (C) 2004 Red Hat, Inc.

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

   Authors: Alexander Larsson <alexl@redhat.com>
*/
#ifndef CAJA_UI_UTILITIES_H
#define CAJA_UI_UTILITIES_H

#include <gtk/gtk.h>

#include <libcaja-extension/caja-menu-item.h>

char *      caja_get_ui_directory              (void);
char *      caja_ui_file                       (const char        *partial_path);
void        caja_ui_unmerge_ui                 (GtkUIManager      *ui_manager,
        guint             *merge_id,
        GtkActionGroup   **action_group);
void        caja_ui_prepare_merge_ui           (GtkUIManager      *ui_manager,
        const char        *name,
        guint             *merge_id,
        GtkActionGroup   **action_group);
GtkAction * caja_action_from_menu_item         (CajaMenuItem  *item, GtkWidget *parent_widget);
GtkAction * caja_toolbar_action_from_menu_item (CajaMenuItem  *item, GtkWidget *parent_widget);
const char *caja_ui_string_get                 (const char        *filename);
void   caja_ui_frame_image                     (GdkPixbuf        **pixbuf);

#endif /* CAJA_UI_UTILITIES_H */
