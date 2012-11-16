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
#include <string.h>

#include "caja-query.h"
#include <eel/eel-gtk-macros.h>
#include <eel/eel-glib-extensions.h>
#include <glib/gi18n.h>
#include <libcaja-private/caja-file-utilities.h>

struct CajaQueryDetails
{
    char *text;
    char *location_uri;
    GList *mime_types;
};

static void  caja_query_class_init       (CajaQueryClass *class);
static void  caja_query_init             (CajaQuery      *query);

G_DEFINE_TYPE (CajaQuery,
               caja_query,
               G_TYPE_OBJECT);

static GObjectClass *parent_class = NULL;

static void
finalize (GObject *object)
{
    CajaQuery *query;

    query = CAJA_QUERY (object);

    g_free (query->details->text);
    g_free (query->details);

    EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
caja_query_class_init (CajaQueryClass *class)
{
    GObjectClass *gobject_class;

    parent_class = g_type_class_peek_parent (class);

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = finalize;
}

static void
caja_query_init (CajaQuery *query)
{
    query->details = g_new0 (CajaQueryDetails, 1);
}

CajaQuery *
caja_query_new (void)
{
    return g_object_new (CAJA_TYPE_QUERY,  NULL);
}


char *
caja_query_get_text (CajaQuery *query)
{
    return g_strdup (query->details->text);
}

void
caja_query_set_text (CajaQuery *query, const char *text)
{
    g_free (query->details->text);
    query->details->text = g_strdup (text);
}

char *
caja_query_get_location (CajaQuery *query)
{
    return g_strdup (query->details->location_uri);
}

void
caja_query_set_location (CajaQuery *query, const char *uri)
{
    g_free (query->details->location_uri);
    query->details->location_uri = g_strdup (uri);
}

GList *
caja_query_get_mime_types (CajaQuery *query)
{
    return eel_g_str_list_copy (query->details->mime_types);
}

void
caja_query_set_mime_types (CajaQuery *query, GList *mime_types)
{
    g_list_foreach(query->details->mime_types, (GFunc) g_free, NULL);
    g_list_free(query->details->mime_types);
    g_list_foreach(query->details->mime_types, (GFunc) g_free, NULL);
    g_list_free(query->details->mime_types);
    query->details->mime_types = eel_g_str_list_copy (mime_types);
}

void
caja_query_add_mime_type (CajaQuery *query, const char *mime_type)
{
    query->details->mime_types = g_list_append (query->details->mime_types,
                                 g_strdup (mime_type));
}

char *
caja_query_to_readable_string (CajaQuery *query)
{
    if (!query || !query->details->text)
    {
        return g_strdup (_("Search"));
    }

    return g_strdup_printf (_("Search for \"%s\""), query->details->text);
}

static char *
encode_home_uri (const char *uri)
{
    char *home_uri;
    const char *encoded_uri;

    home_uri = caja_get_home_directory_uri ();

    if (g_str_has_prefix (uri, home_uri))
    {
        encoded_uri = uri + strlen (home_uri);
        if (*encoded_uri == '/')
        {
            encoded_uri++;
        }
    }
    else
    {
        encoded_uri = uri;
    }

    g_free (home_uri);

    return g_markup_escape_text (encoded_uri, -1);
}

static char *
decode_home_uri (const char *uri)
{
    char *home_uri;
    char *decoded_uri;

    if (g_str_has_prefix (uri, "file:"))
    {
        decoded_uri = g_strdup (uri);
    }
    else
    {
        home_uri = caja_get_home_directory_uri ();

        decoded_uri = g_strconcat (home_uri, "/", uri, NULL);

        g_free (home_uri);
    }

    return decoded_uri;
}


typedef struct
{
    CajaQuery *query;
    gboolean in_text;
    gboolean in_location;
    gboolean in_mimetypes;
    gboolean in_mimetype;
} ParserInfo;

static void
start_element_cb (GMarkupParseContext *ctx,
                  const char *element_name,
                  const char **attribute_names,
                  const char **attribute_values,
                  gpointer user_data,
                  GError **err)
{
    ParserInfo *info;

    info = (ParserInfo *) user_data;

    if (strcmp (element_name, "text") == 0)
        info->in_text = TRUE;
    else if (strcmp (element_name, "location") == 0)
        info->in_location = TRUE;
    else if (strcmp (element_name, "mimetypes") == 0)
        info->in_mimetypes = TRUE;
    else if (strcmp (element_name, "mimetype") == 0)
        info->in_mimetype = TRUE;
}

static void
end_element_cb (GMarkupParseContext *ctx,
                const char *element_name,
                gpointer user_data,
                GError **err)
{
    ParserInfo *info;

    info = (ParserInfo *) user_data;

    if (strcmp (element_name, "text") == 0)
        info->in_text = FALSE;
    else if (strcmp (element_name, "location") == 0)
        info->in_location = FALSE;
    else if (strcmp (element_name, "mimetypes") == 0)
        info->in_mimetypes = FALSE;
    else if (strcmp (element_name, "mimetype") == 0)
        info->in_mimetype = FALSE;
}

static void
text_cb (GMarkupParseContext *ctx,
         const char *text,
         gsize text_len,
         gpointer user_data,
         GError **err)
{
    ParserInfo *info;
    char *t, *uri;

    info = (ParserInfo *) user_data;

    t = g_strndup (text, text_len);

    if (info->in_text)
    {
        caja_query_set_text (info->query, t);
    }
    else if (info->in_location)
    {
        uri = decode_home_uri (t);
        caja_query_set_location (info->query, uri);
        g_free (uri);
    }
    else if (info->in_mimetypes && info->in_mimetype)
    {
        caja_query_add_mime_type (info->query, t);
    }

    g_free (t);

}

static void
error_cb (GMarkupParseContext *ctx,
          GError *err,
          gpointer user_data)
{
}

static GMarkupParser parser =
{
    start_element_cb,
    end_element_cb,
    text_cb,
    NULL,
    error_cb
};


static CajaQuery *
caja_query_parse_xml (char *xml, gsize xml_len)
{
    ParserInfo info = { NULL };
    GMarkupParseContext *ctx;

    if (xml_len == -1)
    {
        xml_len = strlen (xml);
    }

    info.query = caja_query_new ();
    info.in_text = FALSE;

    ctx = g_markup_parse_context_new (&parser, 0, &info, NULL);
    g_markup_parse_context_parse (ctx, xml, xml_len, NULL);

    return info.query;
}


CajaQuery *
caja_query_load (char *file)
{
    CajaQuery *query;
    char *xml;
    gsize xml_len;

    if (!g_file_test (file, G_FILE_TEST_EXISTS))
    {
        return NULL;
    }


    g_file_get_contents (file, &xml, &xml_len, NULL);
    query = caja_query_parse_xml (xml, xml_len);
    g_free (xml);

    return query;
}

static char *
caja_query_to_xml (CajaQuery *query)
{
    GString *xml;
    char *text;
    char *uri;
    char *mimetype;
    GList *l;

    xml = g_string_new ("");
    g_string_append (xml,
                     "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                     "<query version=\"1.0\">\n");

    text = g_markup_escape_text (query->details->text, -1);
    g_string_append_printf (xml, "   <text>%s</text>\n", text);
    g_free (text);

    if (query->details->location_uri)
    {
        uri = encode_home_uri (query->details->location_uri);
        g_string_append_printf (xml, "   <location>%s</location>\n", uri);
        g_free (uri);
    }

    if (query->details->mime_types)
    {
        g_string_append (xml, "   <mimetypes>\n");
        for (l = query->details->mime_types; l != NULL; l = l->next)
        {
            mimetype = g_markup_escape_text (l->data, -1);
            g_string_append_printf (xml, "      <mimetype>%s</mimetype>\n", mimetype);
            g_free (mimetype);
        }
        g_string_append (xml, "   </mimetypes>\n");
    }

    g_string_append (xml, "</query>\n");

    return g_string_free (xml, FALSE);
}

gboolean
caja_query_save (CajaQuery *query, char *file)
{
    char *xml;
    GError *err = NULL;
    gboolean res;


    res = TRUE;
    xml = caja_query_to_xml (query);
    g_file_set_contents (file, xml, strlen (xml), &err);
    g_free (xml);

    if (err != NULL)
    {
        res = FALSE;
        g_error_free (err);
    }
    return res;
}
