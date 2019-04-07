/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-thumbnails.h: Thumbnail code for icon factory.

   Copyright (C) 2000 Eazel, Inc.

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

   Author: Andy Hertzfeld <andy@eazel.com>
*/

#ifndef CAJA_THUMBNAILS_H
#define CAJA_THUMBNAILS_H

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "caja-file.h"

/* Returns NULL if there's no thumbnail yet. */
void       caja_create_thumbnail                (CajaFile *file);
gboolean   caja_can_thumbnail                   (CajaFile *file);
gboolean   caja_can_thumbnail_internally        (CajaFile *file);
gboolean   caja_thumbnail_is_mimetype_limited_by_size
(const char *mime_type);

/* Queue handling: */
void       caja_thumbnail_remove_from_queue     (const char   *file_uri);
void       caja_thumbnail_prioritize            (const char   *file_uri);


#endif /* CAJA_THUMBNAILS_H */
