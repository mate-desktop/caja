/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-search-directory-file.h: Subclass of CajaFile to implement the
   the case of the search directory

   Copyright (C) 2003 Red Hat, Inc.

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

   Author: Alexander Larsson <alexl@redhat.com>
*/

#ifndef CAJA_SEARCH_DIRECTORY_FILE_H
#define CAJA_SEARCH_DIRECTORY_FILE_H

#include "caja-file.h"

#define CAJA_TYPE_SEARCH_DIRECTORY_FILE caja_search_directory_file_get_type()
#define CAJA_SEARCH_DIRECTORY_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_SEARCH_DIRECTORY_FILE, CajaSearchDirectoryFile))
#define CAJA_SEARCH_DIRECTORY_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_SEARCH_DIRECTORY_FILE, CajaSearchDirectoryFileClass))
#define CAJA_IS_SEARCH_DIRECTORY_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_SEARCH_DIRECTORY_FILE))
#define CAJA_IS_SEARCH_DIRECTORY_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), CAJA_TYPE_SEARCH_DIRECTORY_FILE))
#define CAJA_SEARCH_DIRECTORY_FILE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), CAJA_TYPE_SEARCH_DIRECTORY_FILE, CajaSearchDirectoryFileClass))

typedef struct CajaSearchDirectoryFileDetails CajaSearchDirectoryFileDetails;

typedef struct
{
    CajaFile parent_slot;
    CajaSearchDirectoryFileDetails *details;
} CajaSearchDirectoryFile;

typedef struct
{
    CajaFileClass parent_slot;
} CajaSearchDirectoryFileClass;

GType   caja_search_directory_file_get_type (void);
void    caja_search_directory_file_update_display_name (CajaSearchDirectoryFile *search_file);

#endif /* CAJA_SEARCH_DIRECTORY_FILE_H */
