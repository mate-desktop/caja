/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* caja-file-drag.h - Drag & drop handling code that operated on
   CajaFile objects.

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

   Authors: Pavel Cisler <pavel@eazel.com>,
*/

#ifndef CAJA_FILE_DND_H
#define CAJA_FILE_DND_H

#include "caja-dnd.h"
#include "caja-file.h"

#define CAJA_FILE_DND_ERASE_KEYWORD "erase"

gboolean caja_drag_can_accept_item              (CajaFile *drop_target_item,
        const char   *item_uri);
gboolean caja_drag_can_accept_items             (CajaFile *drop_target_item,
        const GList  *items);
gboolean caja_drag_can_accept_info              (CajaFile *drop_target_item,
        CajaIconDndTargetType drag_type,
        const GList *items);
void     caja_drag_file_receive_dropped_keyword (CajaFile *file,
        const char   *keyword);

#endif /* CAJA_FILE_DND_H */

