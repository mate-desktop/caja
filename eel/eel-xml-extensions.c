/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-xml-extensions.c - functions that extend mate-xml

   Copyright (C) 2000 Eazel, Inc.

   The Mate Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Mate Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Mate Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
   Boston, MA 02110-1301, USA.

   Authors: Darin Adler <darin@eazel.com>
*/

#include <config.h>
#include "eel-xml-extensions.h"

#include "eel-string.h"
#include <glib.h>
#include <glib/gi18n-lib.h>

#include <libxml/parser.h>
#include <stdlib.h>

xmlNodePtr
eel_xml_get_children (xmlNodePtr parent)
{
    if (parent == NULL)
    {
        return NULL;
    }
    return parent->children;
}

xmlNodePtr
eel_xml_get_child_by_name_and_property (xmlNodePtr     parent,
                                        const xmlChar *child_name,
                                        const xmlChar *property_name,
                                        const xmlChar *property_value)
{
    xmlNodePtr child;
    xmlChar *property;
    gboolean match;

    if (parent == NULL || property_name == NULL || property_value == NULL)
    {
        return NULL;
    }
    for (child = parent->children; child != NULL; child = child->next)
    {
        if (child->name != NULL && xmlStrcmp (child->name, child_name) == 0)
        {
            property = xmlGetProp (child, property_name);
            match = property != NULL && xmlStrcmp (property, property_value) == 0;
            xmlFree (property);
            if (match)
            {
                return child;
            }
        }
    }
    return NULL;
}

xmlNodePtr
eel_xml_get_root_child_by_name_and_property (xmlDocPtr      document,
                                             const xmlChar *child_name,
                                             const xmlChar *property_name,
                                             const xmlChar *property_value)
{
    return eel_xml_get_child_by_name_and_property (xmlDocGetRootElement (document),
                                                   child_name,
                                                   property_name,
                                                   property_value);
}
