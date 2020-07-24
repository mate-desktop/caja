/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 *  Caja:  Bookmarks sidebar
 *
 *  Copyright (C) 2020 Gordon N. Squash.
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
 *  Authors: Gordon N. Squash
 *
 *  Based largely on the Caja Places sidebar and the Caja History sidebar.
 *
 */
#ifndef _CAJA_BOOKMARKS_SIDEBAR_H
#define _CAJA_BOOKMARKS_SIDEBAR_H

#include <gtk/gtk.h>

#include <libcaja-private/caja-view.h>
#include <libcaja-private/caja-window-info.h>
#include <libcaja-private/caja-bookmark.h>

#include "caja-bookmark-list.h"

#define CAJA_BOOKMARKS_SIDEBAR_ID    "bookmarks"

#define CAJA_TYPE_BOOKMARKS_SIDEBAR caja_bookmarks_sidebar_get_type()
#define CAJA_BOOKMARKS_SIDEBAR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_BOOKMARKS_SIDEBAR, CajaBookmarksSidebar))

typedef struct
{
    GtkScrolledWindow parent;
    GtkTreeView      *tree_view;
    CajaWindowInfo   *window;

    char             *current_uri;
    CajaBookmarkList *bookmarks;
} CajaBookmarksSidebar;

GType caja_bookmarks_sidebar_get_type (void);
void  caja_bookmarks_sidebar_register (void);

#endif
