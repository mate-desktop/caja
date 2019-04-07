/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-saved-search-file.h: Subclass of CajaVFSFile to implement the
   the case of a Saved Search file.

   Copyright (C) 2005 Red Hat, Inc

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

   Author: Alexander Larsson
*/

#ifndef CAJA_SAVED_SEARCH_FILE_H
#define CAJA_SAVED_SEARCH_FILE_H

#include "caja-vfs-file.h"

#define CAJA_TYPE_SAVED_SEARCH_FILE caja_saved_search_file_get_type()
#define CAJA_SAVED_SEARCH_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_SAVED_SEARCH_FILE, CajaSavedSearchFile))
#define CAJA_SAVED_SEARCH_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_SAVED_SEARCH_FILE, CajaSavedSearchFileClass))
#define CAJA_IS_SAVED_SEARCH_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_SAVED_SEARCH_FILE))
#define CAJA_IS_SAVED_SEARCH_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), CAJA_TYPE_SAVED_SEARCH_FILE))
#define CAJA_SAVED_SEARCH_FILE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), CAJA_TYPE_SAVED_SEARCH_FILE, CajaSavedSearchFileClass))


typedef struct CajaSavedSearchFileDetails CajaSavedSearchFileDetails;

typedef struct
{
    CajaFile parent_slot;
} CajaSavedSearchFile;

typedef struct
{
    CajaFileClass parent_slot;
} CajaSavedSearchFileClass;

GType   caja_saved_search_file_get_type (void);

#endif /* CAJA_SAVED_SEARCH_FILE_H */
