/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2005 Mr Jamie McCracken
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
 * Author: Jamie McCracken (jamiemcc@gnome.org)
 *
 */

#ifndef CAJA_SEARCH_ENGINE_TRACKER_H
#define CAJA_SEARCH_ENGINE_TRACKER_H

#include "caja-search-engine.h"

#define CAJA_TYPE_SEARCH_ENGINE_TRACKER		(caja_search_engine_tracker_get_type ())
#define CAJA_SEARCH_ENGINE_TRACKER(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_SEARCH_ENGINE_TRACKER, CajaSearchEngineTracker))
#define CAJA_SEARCH_ENGINE_TRACKER_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_SEARCH_ENGINE_TRACKER, CajaSearchEngineTrackerClass))
#define CAJA_IS_SEARCH_ENGINE_TRACKER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_SEARCH_ENGINE_TRACKER))
#define CAJA_IS_SEARCH_ENGINE_TRACKER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), CAJA_TYPE_SEARCH_ENGINE_TRACKER))
#define CAJA_SEARCH_ENGINE_TRACKER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CAJA_TYPE_SEARCH_ENGINE_TRACKER, CajaSearchEngineTrackerClass))

typedef struct CajaSearchEngineTrackerDetails CajaSearchEngineTrackerDetails;

typedef struct CajaSearchEngineTracker
{
    CajaSearchEngine parent;
    CajaSearchEngineTrackerDetails *details;
} CajaSearchEngineTracker;

typedef struct
{
    CajaSearchEngineClass parent_class;
} CajaSearchEngineTrackerClass;

GType caja_search_engine_tracker_get_type (void);

CajaSearchEngine* caja_search_engine_tracker_new (void);

#endif /* CAJA_SEARCH_ENGINE_TRACKER_H */
