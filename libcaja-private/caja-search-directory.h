/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-search-directory.h: Subclass of CajaDirectory to implement
   a virtual directory consisting of the search directory and the search
   icons

   Copyright (C) 2005 Novell, Inc

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef CAJA_SEARCH_DIRECTORY_H
#define CAJA_SEARCH_DIRECTORY_H

#include "caja-directory.h"
#include "caja-query.h"

#define CAJA_TYPE_SEARCH_DIRECTORY caja_search_directory_get_type()
#define CAJA_SEARCH_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_SEARCH_DIRECTORY, CajaSearchDirectory))
#define CAJA_SEARCH_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_SEARCH_DIRECTORY, CajaSearchDirectoryClass))
#define CAJA_IS_SEARCH_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_SEARCH_DIRECTORY))
#define CAJA_IS_SEARCH_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), CAJA_TYPE_SEARCH_DIRECTORY))
#define CAJA_SEARCH_DIRECTORY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), CAJA_TYPE_SEARCH_DIRECTORY, CajaSearchDirectoryClass))

typedef struct CajaSearchDirectoryDetails CajaSearchDirectoryDetails;

typedef struct
{
    CajaDirectory parent_slot;
    CajaSearchDirectoryDetails *details;
} CajaSearchDirectory;

typedef struct
{
    CajaDirectoryClass parent_slot;
} CajaSearchDirectoryClass;

GType   caja_search_directory_get_type             (void);

char   *caja_search_directory_generate_new_uri     (void);

CajaSearchDirectory *caja_search_directory_new_from_saved_search (const char *uri);

gboolean       caja_search_directory_is_saved_search (CajaSearchDirectory *search);
gboolean       caja_search_directory_is_modified     (CajaSearchDirectory *search);
gboolean       caja_search_directory_is_indexed      (CajaSearchDirectory *search);
void           caja_search_directory_save_search     (CajaSearchDirectory *search);
void           caja_search_directory_save_to_file    (CajaSearchDirectory *search,
        const char              *save_file_uri);

CajaQuery *caja_search_directory_get_query       (CajaSearchDirectory *search);
void           caja_search_directory_set_query       (CajaSearchDirectory *search,
        CajaQuery           *query);

#endif /* CAJA_SEARCH_DIRECTORY_H */
