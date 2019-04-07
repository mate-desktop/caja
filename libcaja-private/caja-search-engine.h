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

#ifndef CAJA_SEARCH_ENGINE_H
#define CAJA_SEARCH_ENGINE_H

#include <glib-object.h>

#include "caja-query.h"

#define CAJA_TYPE_SEARCH_ENGINE		(caja_search_engine_get_type ())
#define CAJA_SEARCH_ENGINE(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_SEARCH_ENGINE, CajaSearchEngine))
#define CAJA_SEARCH_ENGINE_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_SEARCH_ENGINE, CajaSearchEngineClass))
#define CAJA_IS_SEARCH_ENGINE(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_SEARCH_ENGINE))
#define CAJA_IS_SEARCH_ENGINE_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), CAJA_TYPE_SEARCH_ENGINE))
#define CAJA_SEARCH_ENGINE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), CAJA_TYPE_SEARCH_ENGINE, CajaSearchEngineClass))

typedef struct CajaSearchEngineDetails CajaSearchEngineDetails;

typedef struct CajaSearchEngine
{
    GObject parent;
    CajaSearchEngineDetails *details;
} CajaSearchEngine;

typedef struct
{
    GObjectClass parent_class;

    /* VTable */
    void (*set_query) (CajaSearchEngine *engine, CajaQuery *query);
    void (*start) (CajaSearchEngine *engine);
    void (*stop) (CajaSearchEngine *engine);
    gboolean (*is_indexed) (CajaSearchEngine *engine);

    /* Signals */
    void (*hits_added) (CajaSearchEngine *engine, GList *hits);
    void (*hits_subtracted) (CajaSearchEngine *engine, GList *hits);
    void (*finished) (CajaSearchEngine *engine);
    void (*error) (CajaSearchEngine *engine, const char *error_message);
} CajaSearchEngineClass;

GType          caja_search_engine_get_type  (void);
gboolean       caja_search_engine_enabled (void);

CajaSearchEngine* caja_search_engine_new       (void);

void           caja_search_engine_set_query (CajaSearchEngine *engine, CajaQuery *query);
void	       caja_search_engine_start (CajaSearchEngine *engine);
void	       caja_search_engine_stop (CajaSearchEngine *engine);
gboolean       caja_search_engine_is_indexed (CajaSearchEngine *engine);

void	       caja_search_engine_hits_added (CajaSearchEngine *engine, GList *hits);
void	       caja_search_engine_hits_subtracted (CajaSearchEngine *engine, GList *hits);
void	       caja_search_engine_finished (CajaSearchEngine *engine);
void	       caja_search_engine_error (CajaSearchEngine *engine, const char *error_message);

#endif /* CAJA_SEARCH_ENGINE_H */
