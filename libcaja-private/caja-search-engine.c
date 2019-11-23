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

#include <config.h>

#include <eel/eel-gtk-macros.h>

#include "caja-search-engine.h"
#include "caja-search-engine-beagle.h"
#include "caja-search-engine-simple.h"
#include "caja-search-engine-tracker.h"

struct CajaSearchEngineDetails
{
    int none;
};

enum
{
    HITS_ADDED,
    HITS_SUBTRACTED,
    FINISHED,
    ERROR,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_ABSTRACT_TYPE (CajaSearchEngine,
                        caja_search_engine,
                        G_TYPE_OBJECT);

static GObjectClass *parent_class = NULL;

static void
finalize (GObject *object)
{
    CajaSearchEngine *engine;

    engine = CAJA_SEARCH_ENGINE (object);

    g_free (engine->details);

    EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
caja_search_engine_class_init (CajaSearchEngineClass *class)
{
    GObjectClass *gobject_class;

    parent_class = g_type_class_peek_parent (class);

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = finalize;

    signals[HITS_ADDED] =
        g_signal_new ("hits-added",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (CajaSearchEngineClass, hits_added),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__POINTER,
                      G_TYPE_NONE, 1,
                      G_TYPE_POINTER);

    signals[HITS_SUBTRACTED] =
        g_signal_new ("hits-subtracted",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (CajaSearchEngineClass, hits_subtracted),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__POINTER,
                      G_TYPE_NONE, 1,
                      G_TYPE_POINTER);

    signals[FINISHED] =
        g_signal_new ("finished",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (CajaSearchEngineClass, finished),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    signals[ERROR] =
        g_signal_new ("error",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (CajaSearchEngineClass, error),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE, 1,
                      G_TYPE_STRING);

}

static void
caja_search_engine_init (CajaSearchEngine *engine)
{
    engine->details = g_new0 (CajaSearchEngineDetails, 1);
}

CajaSearchEngine *
caja_search_engine_new (void)
{
    CajaSearchEngine *engine;

    engine = caja_search_engine_tracker_new ();
    if (engine)
    {
        return engine;
    }

    engine = caja_search_engine_beagle_new ();
    if (engine)
    {
        return engine;
    }

    engine = caja_search_engine_simple_new ();
    return engine;
}

void
caja_search_engine_set_query (CajaSearchEngine *engine, CajaQuery *query)
{
    g_return_if_fail (CAJA_IS_SEARCH_ENGINE (engine));
    g_return_if_fail (CAJA_SEARCH_ENGINE_GET_CLASS (engine)->set_query != NULL);

    CAJA_SEARCH_ENGINE_GET_CLASS (engine)->set_query (engine, query);
}

void
caja_search_engine_start (CajaSearchEngine *engine)
{
    g_return_if_fail (CAJA_IS_SEARCH_ENGINE (engine));
    g_return_if_fail (CAJA_SEARCH_ENGINE_GET_CLASS (engine)->start != NULL);

    CAJA_SEARCH_ENGINE_GET_CLASS (engine)->start (engine);
}


void
caja_search_engine_stop (CajaSearchEngine *engine)
{
    g_return_if_fail (CAJA_IS_SEARCH_ENGINE (engine));
    g_return_if_fail (CAJA_SEARCH_ENGINE_GET_CLASS (engine)->stop != NULL);

    CAJA_SEARCH_ENGINE_GET_CLASS (engine)->stop (engine);
}

gboolean
caja_search_engine_is_indexed (CajaSearchEngine *engine)
{
    g_return_val_if_fail (CAJA_IS_SEARCH_ENGINE (engine), FALSE);
    g_return_val_if_fail (CAJA_SEARCH_ENGINE_GET_CLASS (engine)->is_indexed != NULL, FALSE);

    return CAJA_SEARCH_ENGINE_GET_CLASS (engine)->is_indexed (engine);
}

void
caja_search_engine_hits_added (CajaSearchEngine *engine, GList *hits)
{
    g_return_if_fail (CAJA_IS_SEARCH_ENGINE (engine));

    g_signal_emit (engine, signals[HITS_ADDED], 0, hits);
}


void
caja_search_engine_hits_subtracted (CajaSearchEngine *engine, GList *hits)
{
    g_return_if_fail (CAJA_IS_SEARCH_ENGINE (engine));

    g_signal_emit (engine, signals[HITS_SUBTRACTED], 0, hits);
}


void
caja_search_engine_finished (CajaSearchEngine *engine)
{
    g_return_if_fail (CAJA_IS_SEARCH_ENGINE (engine));

    g_signal_emit (engine, signals[FINISHED], 0);
}

void
caja_search_engine_error (CajaSearchEngine *engine, const char *error_message)
{
    g_return_if_fail (CAJA_IS_SEARCH_ENGINE (engine));

    g_signal_emit (engine, signals[ERROR], 0, error_message);
}
