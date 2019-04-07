/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2005 Red Hat, Inc
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

#ifndef CAJA_SEARCH_ENGINE_SIMPLE_H
#define CAJA_SEARCH_ENGINE_SIMPLE_H

#include "caja-search-engine.h"

#define CAJA_TYPE_SEARCH_ENGINE_SIMPLE		(caja_search_engine_simple_get_type ())
#define CAJA_SEARCH_ENGINE_SIMPLE(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_SEARCH_ENGINE_SIMPLE, CajaSearchEngineSimple))
#define CAJA_SEARCH_ENGINE_SIMPLE_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_SEARCH_ENGINE_SIMPLE, CajaSearchEngineSimpleClass))
#define CAJA_IS_SEARCH_ENGINE_SIMPLE(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_SEARCH_ENGINE_SIMPLE))
#define CAJA_IS_SEARCH_ENGINE_SIMPLE_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), CAJA_TYPE_SEARCH_ENGINE_SIMPLE))
#define CAJA_SEARCH_ENGINE_SIMPLE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), CAJA_TYPE_SEARCH_ENGINE_SIMPLE, CajaSearchEngineSimpleClass))

typedef struct CajaSearchEngineSimpleDetails CajaSearchEngineSimpleDetails;

typedef struct CajaSearchEngineSimple
{
    CajaSearchEngine parent;
    CajaSearchEngineSimpleDetails *details;
} CajaSearchEngineSimple;

typedef struct
{
    CajaSearchEngineClass parent_class;
} CajaSearchEngineSimpleClass;

GType          caja_search_engine_simple_get_type  (void);

CajaSearchEngine* caja_search_engine_simple_new       (void);

#endif /* CAJA_SEARCH_ENGINE_SIMPLE_H */
