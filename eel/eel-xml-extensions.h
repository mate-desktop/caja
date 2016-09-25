/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-xml-extensions.h - functions that extend mate-xml

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

#ifndef EEL_XML_EXTENSIONS_H
#define EEL_XML_EXTENSIONS_H

#include <libxml/tree.h>
#include <glib.h>

xmlNodePtr eel_xml_get_children                        (xmlNodePtr  parent);
xmlNodePtr eel_xml_get_child_by_name_and_property      (xmlNodePtr  parent,
        const char *child_name,
        const char *property_name,
        const char *property_value);
xmlNodePtr eel_xml_get_root_child_by_name_and_property (xmlDocPtr   document,
        const char *child_name,
        const char *property_name,
        const char *property_value);
xmlChar   *eel_xml_get_property_translated             (xmlNodePtr  parent,
        const char *property_name);

#endif /* EEL_XML_EXTENSIONS_H */
