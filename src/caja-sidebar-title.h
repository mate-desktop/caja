/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Caja
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Caja is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Andy Hertzfeld <andy@eazel.com>
 */

/*
 * This is the header file for the sidebar title, which is part of the sidebar.
 */

#ifndef CAJA_SIDEBAR_TITLE_H
#define CAJA_SIDEBAR_TITLE_H

#include <gtk/gtk.h>

#include <eel/eel-background.h>

#include <libcaja-private/caja-file.h>

#define CAJA_TYPE_SIDEBAR_TITLE caja_sidebar_title_get_type()
#define CAJA_SIDEBAR_TITLE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_SIDEBAR_TITLE, CajaSidebarTitle))
#define CAJA_SIDEBAR_TITLE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_SIDEBAR_TITLE, CajaSidebarTitleClass))
#define CAJA_IS_SIDEBAR_TITLE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_SIDEBAR_TITLE))
#define CAJA_IS_SIDEBAR_TITLE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), CAJA_TYPE_SIDEBAR_TITLE))
#define CAJA_SIDEBAR_TITLE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), CAJA_TYPE_SIDEBAR_TITLE, CajaSidebarTitleClass))

typedef struct _CajaSidebarTitlePrivate CajaSidebarTitlePrivate;

typedef struct
{
    GtkBox box;
    CajaSidebarTitlePrivate *details;
} CajaSidebarTitle;

typedef struct
{
    GtkBoxClass parent_class;
} CajaSidebarTitleClass;

GType      caja_sidebar_title_get_type          (void);
GtkWidget *caja_sidebar_title_new               (void);
void       caja_sidebar_title_set_file          (CajaSidebarTitle *sidebar_title,
        CajaFile         *file,
        const char           *initial_text);
void       caja_sidebar_title_set_text          (CajaSidebarTitle *sidebar_title,
        const char           *new_title);
char *     caja_sidebar_title_get_text          (CajaSidebarTitle *sidebar_title);
gboolean   caja_sidebar_title_hit_test_icon     (CajaSidebarTitle *sidebar_title,
        int                   x,
        int                   y);
void       caja_sidebar_title_select_text_color (CajaSidebarTitle *sidebar_title,
        					 EelBackground        *background);

#endif /* CAJA_SIDEBAR_TITLE_H */
