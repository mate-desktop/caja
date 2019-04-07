/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-desktop-directory-file.h: Subclass of CajaFile to implement the
   the case of the desktop directory

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

#ifndef CAJA_DESKTOP_DIRECTORY_FILE_H
#define CAJA_DESKTOP_DIRECTORY_FILE_H

#include "caja-file.h"

#define CAJA_TYPE_DESKTOP_DIRECTORY_FILE caja_desktop_directory_file_get_type()
#define CAJA_DESKTOP_DIRECTORY_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_DESKTOP_DIRECTORY_FILE, CajaDesktopDirectoryFile))
#define CAJA_DESKTOP_DIRECTORY_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_DESKTOP_DIRECTORY_FILE, CajaDesktopDirectoryFileClass))
#define CAJA_IS_DESKTOP_DIRECTORY_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_DESKTOP_DIRECTORY_FILE))
#define CAJA_IS_DESKTOP_DIRECTORY_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), CAJA_TYPE_DESKTOP_DIRECTORY_FILE))
#define CAJA_DESKTOP_DIRECTORY_FILE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), CAJA_TYPE_DESKTOP_DIRECTORY_FILE, CajaDesktopDirectoryFileClass))

typedef struct CajaDesktopDirectoryFileDetails CajaDesktopDirectoryFileDetails;

typedef struct
{
    CajaFile parent_slot;
    CajaDesktopDirectoryFileDetails *details;
} CajaDesktopDirectoryFile;

typedef struct
{
    CajaFileClass parent_slot;
} CajaDesktopDirectoryFileClass;

GType    caja_desktop_directory_file_get_type    (void);

#endif /* CAJA_DESKTOP_DIRECTORY_FILE_H */
