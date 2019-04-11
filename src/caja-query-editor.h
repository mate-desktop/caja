/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2005 Red Hat, Inc.
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
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 *
 */

#ifndef CAJA_QUERY_EDITOR_H
#define CAJA_QUERY_EDITOR_H

#include <gtk/gtk.h>

#include <libcaja-private/caja-query.h>
#include <libcaja-private/caja-window-info.h>

#include "caja-search-bar.h"

#define CAJA_TYPE_QUERY_EDITOR caja_query_editor_get_type()
#define CAJA_QUERY_EDITOR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_QUERY_EDITOR, CajaQueryEditor))
#define CAJA_QUERY_EDITOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_QUERY_EDITOR, CajaQueryEditorClass))
#define CAJA_IS_QUERY_EDITOR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_QUERY_EDITOR))
#define CAJA_IS_QUERY_EDITOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), CAJA_TYPE_QUERY_EDITOR))
#define CAJA_QUERY_EDITOR_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), CAJA_TYPE_QUERY_EDITOR, CajaQueryEditorClass))

typedef struct CajaQueryEditorDetails CajaQueryEditorDetails;

typedef struct CajaQueryEditor
{
    GtkBox parent;
    CajaQueryEditorDetails *details;
} CajaQueryEditor;

typedef struct
{
    GtkBoxClass parent_class;

    void (* changed) (CajaQueryEditor  *editor,
                      CajaQuery        *query,
                      gboolean              reload);
    void (* cancel)   (CajaQueryEditor *editor);
} CajaQueryEditorClass;

GType      caja_query_editor_get_type     	   (void);
GtkWidget* caja_query_editor_new          	   (gboolean start_hidden,
        gboolean is_indexed);
GtkWidget* caja_query_editor_new_with_bar      (gboolean start_hidden,
        gboolean is_indexed,
        gboolean start_attached,
        CajaSearchBar *bar,
        CajaWindowSlot *slot);
void       caja_query_editor_set_default_query (CajaQueryEditor *editor);

void	   caja_query_editor_grab_focus (CajaQueryEditor *editor);
void       caja_query_editor_clear_query (CajaQueryEditor *editor);

CajaQuery *caja_query_editor_get_query   (CajaQueryEditor *editor);
void           caja_query_editor_set_query   (CajaQueryEditor *editor,
        CajaQuery       *query);
void           caja_query_editor_set_visible (CajaQueryEditor *editor,
        gboolean             visible);

#endif /* CAJA_QUERY_EDITOR_H */
