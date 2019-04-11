/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 *  Caja
 *
 *  Copyright (C) 1999, 2000, 2004 Red Hat, Inc.
 *  Copyright (C) 2000, 2001 Eazel, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  Authors: Andy Hertzfeld <andy@eazel.com>
 *           Alexander Larsson <alexl@redhat.com>
 */
#ifndef _CAJA_NOTES_VIEWER_H
#define _CAJA_NOTES_VIEWER_H

#include <gtk/gtk.h>

#include <libcaja-private/caja-view.h>
#include <libcaja-private/caja-window-info.h>

#define CAJA_NOTES_SIDEBAR_ID    "notes"

#define CAJA_TYPE_NOTES_VIEWER caja_notes_viewer_get_type()
#define CAJA_NOTES_VIEWER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_NOTES_VIEWER, CajaNotesViewer))

typedef struct _CajaNotesViewerDetails CajaNotesViewerDetails;

typedef struct
{
    GtkScrolledWindow parent;
    CajaNotesViewerDetails *details;
} CajaNotesViewer;

GType caja_notes_viewer_get_type (void);
void caja_notes_viewer_register (void);

#endif
