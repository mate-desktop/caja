/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-merged-directory.h: Subclass of CajaDirectory to implement
   a virtual directory consisting of the merged contents of some real
   directories.

   Copyright (C) 1999, 2000 Eazel, Inc.

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

   Author: Darin Adler <darin@bentspoon.com>
*/

#ifndef CAJA_MERGED_DIRECTORY_H
#define CAJA_MERGED_DIRECTORY_H

#include <libcaja-private/caja-directory.h>

#define CAJA_TYPE_MERGED_DIRECTORY caja_merged_directory_get_type()
#define CAJA_MERGED_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_MERGED_DIRECTORY, CajaMergedDirectory))
#define CAJA_MERGED_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_MERGED_DIRECTORY, CajaMergedDirectoryClass))
#define CAJA_IS_MERGED_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_MERGED_DIRECTORY))
#define CAJA_IS_MERGED_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), CAJA_TYPE_MERGED_DIRECTORY))
#define CAJA_MERGED_DIRECTORY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), CAJA_TYPE_MERGED_DIRECTORY, CajaMergedDirectoryClass))

typedef struct CajaMergedDirectoryDetails CajaMergedDirectoryDetails;

typedef struct
{
    CajaDirectory parent_slot;
    CajaMergedDirectoryDetails *details;
} CajaMergedDirectory;

typedef struct
{
    CajaDirectoryClass parent_slot;

    void (* add_real_directory)    (CajaMergedDirectory *merged_directory,
                                    CajaDirectory       *real_directory);
    void (* remove_real_directory) (CajaMergedDirectory *merged_directory,
                                    CajaDirectory       *real_directory);
} CajaMergedDirectoryClass;

GType   caja_merged_directory_get_type              (void);
void    caja_merged_directory_add_real_directory    (CajaMergedDirectory *merged_directory,
        CajaDirectory       *real_directory);
void    caja_merged_directory_remove_real_directory (CajaMergedDirectory *merged_directory,
        CajaDirectory       *real_directory);
GList * caja_merged_directory_get_real_directories  (CajaMergedDirectory *merged_directory);

#endif /* CAJA_MERGED_DIRECTORY_H */
