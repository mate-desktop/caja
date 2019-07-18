/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2005 Novell, Inc.
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
 * Author: Anders Carlsson <andersca@imendio.com>
 *
 */

#ifndef CAJA_QUERY_H
#define CAJA_QUERY_H

#include <glib-object.h>

#define CAJA_TYPE_QUERY		(caja_query_get_type ())
#define CAJA_QUERY(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_QUERY, CajaQuery))
#define CAJA_QUERY_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_QUERY, CajaQueryClass))
#define CAJA_IS_QUERY(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_QUERY))
#define CAJA_IS_QUERY_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), CAJA_TYPE_QUERY))
#define CAJA_QUERY_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), CAJA_TYPE_QUERY, CajaQueryClass))

typedef struct CajaQueryDetails CajaQueryDetails;

typedef struct CajaQuery
{
    GObject parent;
    CajaQueryDetails *details;
} CajaQuery;

typedef struct
{
    GObjectClass parent_class;
} CajaQueryClass;

GType          caja_query_get_type (void);
gboolean       caja_query_enabled  (void);

CajaQuery* caja_query_new      (void);

char *         caja_query_get_text           (CajaQuery *query);
void           caja_query_set_text           (CajaQuery *query, const char *text);

char *         caja_query_get_location       (CajaQuery *query);
void           caja_query_set_location       (CajaQuery *query, const char *uri);

GList *        caja_query_get_tags           (CajaQuery *query);
void           caja_query_set_tags           (CajaQuery *query, GList *tags);
void           caja_query_add_tag            (CajaQuery *query, const char *tag);

GList *        caja_query_get_mime_types     (CajaQuery *query);
void           caja_query_set_mime_types     (CajaQuery *query, GList *mime_types);
void           caja_query_add_mime_type      (CajaQuery *query, const char *mime_type);

char *         caja_query_to_readable_string (CajaQuery *query);
CajaQuery *    caja_query_load               (char *file);
gboolean       caja_query_save               (CajaQuery *query, char *file);

gint64         caja_query_get_timestamp      (CajaQuery *query);
void           caja_query_set_timestamp      (CajaQuery *query, gint64 sec);

gint64         caja_query_get_size           (CajaQuery *query);
void           caja_query_set_size           (CajaQuery *query, gint64 size);

char *         caja_query_get_contained_text (CajaQuery *query);
void           caja_query_set_contained_text (CajaQuery *query, const char *text);

#endif /* CAJA_QUERY_H */
