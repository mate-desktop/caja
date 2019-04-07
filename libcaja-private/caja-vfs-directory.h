/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-vfs-directory.h: Subclass of CajaDirectory to implement the
   the case of a VFS directory.

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

#ifndef CAJA_VFS_DIRECTORY_H
#define CAJA_VFS_DIRECTORY_H

#include "caja-directory.h"

#define CAJA_TYPE_VFS_DIRECTORY caja_vfs_directory_get_type()
#define CAJA_VFS_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_VFS_DIRECTORY, CajaVFSDirectory))
#define CAJA_VFS_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_VFS_DIRECTORY, CajaVFSDirectoryClass))
#define CAJA_IS_VFS_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_VFS_DIRECTORY))
#define CAJA_IS_VFS_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), CAJA_TYPE_VFS_DIRECTORY))
#define CAJA_VFS_DIRECTORY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), CAJA_TYPE_VFS_DIRECTORY, CajaVFSDirectoryClass))

typedef struct CajaVFSDirectoryDetails CajaVFSDirectoryDetails;

typedef struct
{
    CajaDirectory parent_slot;
} CajaVFSDirectory;

typedef struct
{
    CajaDirectoryClass parent_slot;
} CajaVFSDirectoryClass;

GType   caja_vfs_directory_get_type (void);

#endif /* CAJA_VFS_DIRECTORY_H */
