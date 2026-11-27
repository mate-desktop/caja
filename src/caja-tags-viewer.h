/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Caja
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  Tags property page for the file Properties dialog. Reads and writes the
 *  "user.xdg.tags" extended attribute (exposed by GIO as "xattr::xdg.tags"),
 *  which is the same attribute caja's tag search reads in
 *  caja-search-engine-simple.c.
 */
#ifndef _CAJA_TAGS_VIEWER_H
#define _CAJA_TAGS_VIEWER_H

#include <gtk/gtk.h>

#define CAJA_TYPE_TAGS_VIEWER caja_tags_viewer_get_type()
#define CAJA_TAGS_VIEWER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_TAGS_VIEWER, CajaTagsViewer))
#define CAJA_IS_TAGS_VIEWER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_TAGS_VIEWER))

typedef struct _CajaTagsViewer CajaTagsViewer;

GType caja_tags_viewer_get_type (void);
void caja_tags_viewer_register (void);

#endif
