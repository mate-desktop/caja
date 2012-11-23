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

#include <config.h>
#include "caja-search-engine-simple.h"

#include <string.h>
#include <glib.h>

#include <eel/eel-gtk-macros.h>
#include <gio/gio.h>

#include <src/glibcompat.h> /* for g_list_free_full */

#define BATCH_SIZE 500

typedef struct
{
    CajaSearchEngineSimple *engine;
    GCancellable *cancellable;

    GList *mime_types;
    char **words;
    GList *found_list;

    GQueue *directories; /* GFiles */

    GHashTable *visited;

    gint n_processed_files;
    GList *uri_hits;
} SearchThreadData;


struct CajaSearchEngineSimpleDetails
{
    CajaQuery *query;

    SearchThreadData *active_search;

    gboolean query_finished;
};


static void  caja_search_engine_simple_class_init       (CajaSearchEngineSimpleClass *class);
static void  caja_search_engine_simple_init             (CajaSearchEngineSimple      *engine);

G_DEFINE_TYPE (CajaSearchEngineSimple,
               caja_search_engine_simple,
               CAJA_TYPE_SEARCH_ENGINE);

static CajaSearchEngineClass *parent_class = NULL;

static void
finalize (GObject *object)
{
    CajaSearchEngineSimple *simple;

    simple = CAJA_SEARCH_ENGINE_SIMPLE (object);

    if (simple->details->query)
    {
        g_object_unref (simple->details->query);
        simple->details->query = NULL;
    }

    g_free (simple->details);

    EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static SearchThreadData *
search_thread_data_new (CajaSearchEngineSimple *engine,
                        CajaQuery *query)
{
    SearchThreadData *data;
    char *text, *lower, *normalized, *uri;
    GFile *location;

    data = g_new0 (SearchThreadData, 1);

    data->engine = engine;
    data->directories = g_queue_new ();
    data->visited = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    uri = caja_query_get_location (query);
    location = NULL;
    if (uri != NULL)
    {
        location = g_file_new_for_uri (uri);
        g_free (uri);
    }
    if (location == NULL)
    {
        location = g_file_new_for_path ("/");
    }
    g_queue_push_tail (data->directories, location);

    text = caja_query_get_text (query);
    normalized = g_utf8_normalize (text, -1, G_NORMALIZE_NFD);
    lower = g_utf8_strdown (normalized, -1);
    data->words = g_strsplit (lower, " ", -1);
    g_free (text);
    g_free (lower);
    g_free (normalized);

    data->mime_types = caja_query_get_mime_types (query);

    data->cancellable = g_cancellable_new ();

    return data;
}

static void
search_thread_data_free (SearchThreadData *data)
{
    g_queue_foreach (data->directories,
                     (GFunc)g_object_unref, NULL);
    g_queue_free (data->directories);
    g_hash_table_destroy (data->visited);
    g_object_unref (data->cancellable);
    g_strfreev (data->words);
    g_list_free_full (data->mime_types, g_free);
    g_list_free_full (data->uri_hits, g_free);
    g_free (data);
}

static gboolean
search_thread_done_idle (gpointer user_data)
{
    SearchThreadData *data;

    data = user_data;

    if (!g_cancellable_is_cancelled (data->cancellable))
    {
        caja_search_engine_finished (CAJA_SEARCH_ENGINE (data->engine));
        data->engine->details->active_search = NULL;
    }

    search_thread_data_free (data);

    return FALSE;
}

typedef struct
{
    GList *uris;
    SearchThreadData *thread_data;
} SearchHits;


static gboolean
search_thread_add_hits_idle (gpointer user_data)
{
    SearchHits *hits;

    hits = user_data;

    if (!g_cancellable_is_cancelled (hits->thread_data->cancellable))
    {
        caja_search_engine_hits_added (CAJA_SEARCH_ENGINE (hits->thread_data->engine),
                                       hits->uris);
    }

    g_list_free_full (hits->uris, g_free);
    g_free (hits);

    return FALSE;
}

static void
send_batch (SearchThreadData *data)
{
    SearchHits *hits;

    data->n_processed_files = 0;

    if (data->uri_hits)
    {
        hits = g_new (SearchHits, 1);
        hits->uris = data->uri_hits;
        hits->thread_data = data;
        g_idle_add (search_thread_add_hits_idle, hits);
    }
    data->uri_hits = NULL;
}

#define STD_ATTRIBUTES \
	G_FILE_ATTRIBUTE_STANDARD_NAME "," \
	G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME "," \
	G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN "," \
	G_FILE_ATTRIBUTE_STANDARD_TYPE "," \
	G_FILE_ATTRIBUTE_ID_FILE

static void
visit_directory (GFile *dir, SearchThreadData *data)
{
    GFileEnumerator *enumerator;
    GFileInfo *info;
    GFile *child;
    const char *mime_type, *display_name;
    char *lower_name, *normalized;
    gboolean hit;
    int i;
    GList *l;
    const char *id;
    gboolean visited;

    enumerator = g_file_enumerate_children (dir,
                                            data->mime_types != NULL ?
                                            STD_ATTRIBUTES ","
                                            G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE
                                            :
                                            STD_ATTRIBUTES
                                            ,
                                            0, data->cancellable, NULL);

    if (enumerator == NULL)
    {
        return;
    }

    while ((info = g_file_enumerator_next_file (enumerator, data->cancellable, NULL)) != NULL)
    {
        if (g_file_info_get_is_hidden (info))
        {
            goto next;
        }

        display_name = g_file_info_get_display_name (info);
        if (display_name == NULL)
        {
            goto next;
        }

        normalized = g_utf8_normalize (display_name, -1, G_NORMALIZE_NFD);
        lower_name = g_utf8_strdown (normalized, -1);
        g_free (normalized);

        hit = TRUE;
        for (i = 0; data->words[i] != NULL; i++)
        {
            if (strstr (lower_name, data->words[i]) == NULL)
            {
                hit = FALSE;
                break;
            }
        }
        g_free (lower_name);

        if (hit && data->mime_types)
        {
            mime_type = g_file_info_get_content_type (info);
            hit = FALSE;

            for (l = data->mime_types; mime_type != NULL && l != NULL; l = l->next)
            {
                if (g_content_type_equals (mime_type, l->data))
                {
                    hit = TRUE;
                    break;
                }
            }
        }

        child = g_file_get_child (dir, g_file_info_get_name (info));

        if (hit)
        {
            data->uri_hits = g_list_prepend (data->uri_hits, g_file_get_uri (child));
        }

        data->n_processed_files++;
        if (data->n_processed_files > BATCH_SIZE)
        {
            send_batch (data);
        }

        if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
        {
            id = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_ID_FILE);
            visited = FALSE;
            if (id)
            {
                if (g_hash_table_lookup_extended (data->visited,
                                                  id, NULL, NULL))
                {
                    visited = TRUE;
                }
                else
                {
                    g_hash_table_insert (data->visited, g_strdup (id), NULL);
                }
            }

            if (!visited)
            {
                g_queue_push_tail (data->directories, g_object_ref (child));
            }
        }

        g_object_unref (child);
next:
        g_object_unref (info);
    }

    g_object_unref (enumerator);
}


static gpointer
search_thread_func (gpointer user_data)
{
    SearchThreadData *data;
    GFile *dir;
    GFileInfo *info;
    const char *id;

    data = user_data;

    /* Insert id for toplevel directory into visited */
    dir = g_queue_peek_head (data->directories);
    info = g_file_query_info (dir, G_FILE_ATTRIBUTE_ID_FILE, 0, data->cancellable, NULL);
    if (info)
    {
        id = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_ID_FILE);
        if (id)
        {
            g_hash_table_insert (data->visited, g_strdup (id), NULL);
        }
        g_object_unref (info);
    }

    while (!g_cancellable_is_cancelled (data->cancellable) &&
            (dir = g_queue_pop_head (data->directories)) != NULL)
    {
        visit_directory (dir, data);
        g_object_unref (dir);
    }
    send_batch (data);

    g_idle_add (search_thread_done_idle, data);

    return NULL;
}

static void
caja_search_engine_simple_start (CajaSearchEngine *engine)
{
    CajaSearchEngineSimple *simple;
    SearchThreadData *data;

    simple = CAJA_SEARCH_ENGINE_SIMPLE (engine);

    if (simple->details->active_search != NULL)
    {
        return;
    }

    if (simple->details->query == NULL)
    {
        return;
    }

    data = search_thread_data_new (simple, simple->details->query);

    g_thread_create (search_thread_func, data, FALSE, NULL);

    simple->details->active_search = data;
}

static void
caja_search_engine_simple_stop (CajaSearchEngine *engine)
{
    CajaSearchEngineSimple *simple;

    simple = CAJA_SEARCH_ENGINE_SIMPLE (engine);

    if (simple->details->active_search != NULL)
    {
        g_cancellable_cancel (simple->details->active_search->cancellable);
        simple->details->active_search = NULL;
    }
}

static gboolean
caja_search_engine_simple_is_indexed (CajaSearchEngine *engine)
{
    return FALSE;
}

static void
caja_search_engine_simple_set_query (CajaSearchEngine *engine, CajaQuery *query)
{
    CajaSearchEngineSimple *simple;

    simple = CAJA_SEARCH_ENGINE_SIMPLE (engine);

    if (query)
    {
        g_object_ref (query);
    }

    if (simple->details->query)
    {
        g_object_unref (simple->details->query);
    }

    simple->details->query = query;
}

static void
caja_search_engine_simple_class_init (CajaSearchEngineSimpleClass *class)
{
    GObjectClass *gobject_class;
    CajaSearchEngineClass *engine_class;

    parent_class = g_type_class_peek_parent (class);

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = finalize;

    engine_class = CAJA_SEARCH_ENGINE_CLASS (class);
    engine_class->set_query = caja_search_engine_simple_set_query;
    engine_class->start = caja_search_engine_simple_start;
    engine_class->stop = caja_search_engine_simple_stop;
    engine_class->is_indexed = caja_search_engine_simple_is_indexed;
}

static void
caja_search_engine_simple_init (CajaSearchEngineSimple *engine)
{
    engine->details = g_new0 (CajaSearchEngineSimpleDetails, 1);
}


CajaSearchEngine *
caja_search_engine_simple_new (void)
{
    CajaSearchEngine *engine;

    engine = g_object_new (CAJA_TYPE_SEARCH_ENGINE_SIMPLE, NULL);

    return engine;
}
